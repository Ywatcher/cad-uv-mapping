# Thread Safety Investigation and Fix

## Summary

The projection pipeline crashed with `SIGILL` / `SEGFAULT` inside
`libTKGeomAlgo.so` when running with multiple threads. This document records
how the crash was diagnosed, what caused it, and how it was fixed.

---

## Symptoms

The crash appeared only when parallelism was enabled. A single-threaded run of
the same input always completed without error. The backtrace pointed into OCCT's
`libTKGeomAlgo.so` — a third-party library we do not control.

This pattern — crash in parallel, clean in serial — is the defining signature
of a **data race**: two threads writing to the same memory location at the same
time, producing corrupted state that causes a crash some time later.

The failing test was `test_stress_repeated_parallel_calls[method1]` (the ray
projection method, 200 parallel iterations).

---

## What a data race is

A data race happens when two threads access the same piece of memory at the same
time and at least one of them is writing. The result is undefined: depending on
exact CPU scheduling the value written by one thread may be partially overwritten
by the other, producing a pointer or counter that contains garbage. The program
then uses that garbage value later — possibly much later and in unrelated code —
and crashes.

Data races are hard to find because:

- They depend on timing, so they may not reproduce every run.
- The crash site (where the program dies) is usually far from the race site
  (where the memory was corrupted).
- They disappear when you add print statements or run under a debugger, because
  those change thread timing.

---

## Step 1 — Try automated race detection (ThreadSanitizer)

The correct tool for finding a race is **ThreadSanitizer (TSan)**. You compile
the program with `-fsanitize=thread`, run it, and TSan intercepts every memory
access. When it sees two threads touching the same address concurrently it prints
the exact file and line numbers of both accesses.

We tried to enable TSan. It failed: `clang++` was not installed on the build
machine, and GCC's TSan could not locate the OpenCASCADE shared libraries at
link time. We had to find the race by reasoning instead.

---

## Step 2 — Identify the suspects

Looking at the code that ran in parallel, there were two candidates:

**Suspect 1 — `BRepBuilderAPI_Copy`**

Before each ray intersection we deep-copy the target face so each worker has
its own geometry object. This copy call runs in parallel, one per worker thread.

**Suspect 2 — `IntCurvesFace_Intersector` construction**

OCCT's internal ray-intersection engine. Its constructor allocates an internal
polyhedron — a triangulation of the face used to find approximate hit locations.
This constructor also ran in parallel, once per call to `Perform()`.

Both suspects executed concurrently across threads. Either one could be the race.

---

## Step 3 — The diagnostic mutex experiment

When TSan is unavailable, the standard technique is to **serialize one suspect
at a time** and observe whether the crash disappears. If serializing a suspect
makes the crash go away, that suspect contains the race.

We added a global mutex around suspect 2 (the OCCT constructor), but
deliberately left suspect 1 (our copy call) outside it:

```cpp
// BRepBuilderAPI_Copy runs here — still in parallel, NOT covered by mutex
BRepBuilderAPI_Copy copier(face, /*CopyGeom=*/Standard_True);
private_face_ = TopoDS::Face(copier.Shape());

// mutex covers only the OCCT internal allocator calls
{
    static std::mutex s_occt_intersect_mutex;
    std::lock_guard<std::mutex> lock(s_occt_intersect_mutex);
    intersector_.Load(private_face_, tolerance_);
    intersector_.Perform(line, param_min, param_max);
}
```

**Outcome:** The 200-iteration stress test passed with zero crashes.

**What this proves:** The mutex covered suspect 2 but not suspect 1. Since the
crash disappeared, suspect 2 (OCCT's internal constructor) is the race site.
`BRepBuilderAPI_Copy` is innocent.

**The slowdown was also informative.** The test took 22 minutes instead of the
normal 4 minutes. A global mutex forces all ray intersections in the entire
program to queue up one at a time, eliminating all parallelism. The severe
slowdown confirmed that the mutex was active and doing real serialization work —
it was not being optimised away.

---

## Step 4 — Understand why OCCT's allocator races

OCCT uses a custom memory allocator called `Standard_MMgrOptl`. To make
allocation fast, it maintains **per-size free-lists**: when you free an object of
a certain size, its memory goes onto a linked list for that size; the next
allocation of the same size pops from the list instead of calling the system
allocator.

This is a well-known optimisation, but it requires that each free-list operation
is atomic. `Standard_MMgrOptl` does not protect its free-lists with a lock.

When two threads call `new IntCurvesFace_Intersector(...)` simultaneously, both
try to pop the head pointer of the same free-list at the same time. Each thread
reads the current head, then both write a new head based on the value they read.
One write overwrites the other. The result is a corrupted pointer in the
free-list. The next allocation that uses this list receives a garbage address,
and any write through that address corrupts unrelated memory — eventually causing
`SIGILL` or `SEGFAULT`.

This is a bug in OCCT's allocator, not in our code. But it is triggered by our
code calling OCCT's constructor from multiple threads simultaneously.

---

## Fix — Remove all OCCT allocation from the parallel phase

The only reliable fix is to ensure that OCCT's allocator is never called from
more than one thread at a time. Rather than holding a global lock (which
eliminates all parallelism, as the 22-minute result showed), we restructured the
code so that all OCCT allocation happens **before** the parallel phase begins.

The pattern applied to both projection paths:

```
serial (before workers launch):
    for each high face:
        deep-copy the face geometry           ← BRepBuilderAPI_Copy
        evaluate surface on a UV grid         ← pre-warms BSpline caches
        sample all boundary PCurves           ← pre-warms 2D curve caches
        store results in std::vector          ← no OCCT Handle storage

parallel (workers running):
    for each sample:
        read std::vector data                 ← pure reads, no allocation
        run triangle test (Möller-Trumbore)   ← arithmetic only
        run Newton refinement via D1()        ← reads pre-warmed surface
        classify against boundary polygon     ← reads std::vector of gp_Pnt2d
```

After the serial setup phase, every object is **read-only**. Multiple threads
reading the same memory simultaneously is always safe — races only happen when
at least one thread writes. Since `Perform()` is declared `const` and returns
results by value with no output parameters, the compiler enforces that workers
cannot write to shared state.

### Why pre-warming matters

OCCT's BSpline surface evaluator writes a **Bezier conversion cache** the first
time any knot span is evaluated. On subsequent calls that span is read from the
cache. If two threads hit the same uncached span simultaneously, both try to
write the cache entry — another race.

By evaluating the full UV grid in the serial `Load()` phase, every knot span is
cached before any worker starts. Worker calls to `D1()` are guaranteed cache
hits, so they are pure reads.

The same applies to 2D PCurve caches on boundary edges: sampling them in
`Load()` pre-warms them, so `inside_face()` in the parallel phase only reads.

---

## Two classes were implemented

### `geom::RayFaceIntersector`

Replaces `IntCurvesFace_ShapeIntersector` for the `ray` and `ray_bidirectional`
projection methods.

- `Load(face, tolerance)` — deep-copies the face, builds a triangle mesh from a
  20×20 UV parameter grid, computes per-triangle AABBs, samples all boundary
  PCurves. All OCCT allocation here.
- `Perform(line, pmin, pmax) const` — coarse AABB slab test, Möller-Trumbore
  exact ray-triangle test, 2D Newton refinement on the true surface, even-odd
  boundary classification. No OCCT allocation.

### `geom::PointFaceProjector`

Replaces `BRepExtrema_DistShapeShape` for the `nearest` projection method.

- `Load(face, tolerance)` — same deep-copy and cache pre-warming as above.
- `Perform(point) const` — finds the K nearest grid points to the query (K=8),
  runs Gauss-Newton minimisation of `|S(u,v) − P|²` from each seed, classifies
  the result against the boundary polygon. Returns UV coordinates directly from
  Newton, with no `ShapeAnalysis_Surface` UV-inversion call needed. No OCCT
  allocation.

---

## Test results

| Test configuration | Result | Notes |
|---|---|---|
| Baseline (no fix) | crash (SIGILL/SEGFAULT) | 200 iterations, ray method |
| Diagnostic mutex | 14 passed in 22:36 | confirms race in OCCT allocator |
| `RayFaceIntersector` | 14 passed in 4:39 | no mutex needed |
| `PointFaceProjector` added | 14 passed in 4:32 | all three methods covered |

The 22-minute run with the mutex demonstrates both that the mutex fixed the crash
and that a global lock is unacceptably slow. The final 4-minute run with custom
implementations recovers full parallelism with no crashes.

---

## What remains

### Known limitations of the current implementation

**Fixed mesh resolution.** The UV grid used during `Load()` is always 20×20,
regardless of surface complexity. OCCT's own implementation queries the number
of B-spline knot spans and uses 4 sample points per span. A highly-curved
surface with many knot spans may have insufficient grid coverage, causing ray
hits near knot boundaries to be missed. This does not affect the accuracy of
hits that are found — Newton refinement achieves tolerance-level precision
regardless of grid size.

**No analytic surface dispatch.** For flat (plane), cylindrical, spherical, and
conical faces, exact intersection and nearest-point formulas exist that produce
the answer in a few arithmetic operations with no grid needed. The current
implementation uses the full grid path for all surface types.

**Linear candidate scan.** The coarse screen (AABB slab test for ray; nearest
grid points for projection) scans all N triangles or grid points in O(N) time.
A spatial index (BVH tree) would reduce this to O(log N), which matters if the
mesh grows with adaptive sizing.

**Sampled boundary polygon.** The PCurve boundaries are approximated by
discrete samples. For highly curved trimming curves a finely sampled polygon may
classify borderline UV points incorrectly.

### Extension points

Each limitation corresponds to a named private method in the class with a
detailed TODO comment explaining exactly what to replace it with. Improvements
can be made one method at a time without touching `Perform()` or any caller.
See `docs/ray_face_intersector.md` for the full improvement roadmap.

### ThreadSanitizer

TSan has not been run (no `clang++` on the build machine). The diagnostic mutex
experiment gives high confidence that the race is fixed, but TSan would provide
formal proof. When `clang++` becomes available, build with
`-DENABLE_TSAN=ON` (already wired into `CMakeLists.txt`) and run the thread
safety test suite.
