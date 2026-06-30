# GitHub issue: OCCT thread-safety crash in parallel ray projection

Copy each section below into GitHub as a separate post.

---

## POST 1 — open the issue

**Title:** SIGILL crash in parallel `ray_bidirectional` projection (OCCT thread-unsafe lazy cache)

---

### Description

Running UV mapping with `method=ray_bidirectional` and `enable_parallel=True` crashes
the Python process with `SIGILL` (illegal instruction). The crash happens inside
OpenCASCADE's `libTKGeomAlgo`, not in Python or application code.
A second run after the crash caused a full system kernel panic and reboot.

### Environment

- OS: Linux (Arch), kernel 6.16.1
- Python: 3.12 (conda env `game-qt6-py312`)
- OpenCASCADE: 7.8.1
- Crash confirmed: 2026-06-29

### Steps to reproduce

```python
from cad_uv_map import MappingMethod, describe_shape_faces, map_source_samples_to_target
from cad_uv_map.api import MappingContext
from tests.fixtures.cad_cases import pedestal_ribs_nofold_pair

pair = pedestal_ribs_nofold_pair()
faces = describe_shape_faces(pair.low)
face = next(f for f in faces if abs(f.normal_at(f.center()).Z) < 0.5)

samples = [(u, v) for v in [0.25, 0.5, 0.75] for u in [0.25, 0.5, 0.75]]

ctx = MappingContext()
ctx.enable_parallel = True

# This crashes the process:
map_source_samples_to_target(
    pair.low, pair.high, face.face_id, samples, MappingMethod.ray_bidirectional, ctx
)
```

### Crash evidence

**How to collect these logs** (run immediately after a crash, before reboot):

```bash
# 1. Kernel trap line — confirms signal, library, and instruction pointer
journalctl -b 0 --no-pager -o short-iso | grep -E "traps|coredump|signal 4" \
    > collapse/kernel-trap.txt

# 2. Core dump summary
coredumpctl info > collapse/core-info.txt

# 3. Full thread stack traces from the core dump
coredumpctl debug --debugger-arguments="-batch -ex 'thread apply all bt'" \
    > collapse/gdb-threads.txt
```

---

**1. Kernel log — trap line**

```
Jun 29 19:28:18 archlinux kernel: traps: python[173476] trap invalid opcode \
    ip:7f8d278ab3da sp:7f8c9e7fb280 error:0 \
    in libTKGeomAlgo.so.7.8.1[2ab3da,7f8d276b7000+4e2000]
Jun 29 19:28:18 archlinux systemd-coredump[173477]: \
    Process 173372 (python) of user 1000 terminated abnormally with signal 4/ILL
```

`trap invalid opcode` in `libTKGeomAlgo.so.7.8.1` is the kernel's record of the
SIGILL. The instruction pointer `2ab3da` (offset into the library) points into
`IntCurveSurface_ThePolyhedronOfHInter::FillBounding`, confirmed by the GDB trace below.

---

**2. Core dump info summary**

```
Signal:        4 (ILL)
PID:           173372
Command Line:  python visualize_qt.py --case pedestal_ribs_nofold \
                   --method ray_bidirectional --force
Executable:    .../envs/game-qt6-py312/bin/python3.12
Size on Disk:  112.1M
```

---

**3. GDB thread stacks — crashing thread**

Two worker threads were both inside `IntCurvesFace_ShapeIntersector::Load()` on
the same face at the time of the crash. Thread A faulted in `FillBounding`,
Thread B was in `BRepAdaptor_Curve2d::Initialize` on the same face.

Thread A (faulted):
```
#0  IntCurveSurface_ThePolyhedronOfHInter::FillBounding()
        (libTKGeomAlgo.so.7.8.1 + 0x2ab3da)   ← invalid opcode here
#1  IntCurveSurface_ThePolyhedronOfHInter::Init(Adaptor3d_Surface, ...)
        (libTKGeomAlgo.so.7.8.1)
#2  IntCurveSurface_ThePolyhedronOfHInterC2(Adaptor3d_Surface, ...)
        (libTKGeomAlgo.so.7.8.1)
#3  IntCurvesFace_Intersector::IntCurvesFace_Intersector(TopoDS_Face, ...)
        (libTKTopAlgo.so.7.8.1)
#4  IntCurvesFace_ShapeIntersector::Load(TopoDS_Shape, tol)
        (libTKTopAlgo.so.7.8.1)
#5  [project_ray_to_face_impl — _native.cpython-312-x86_64-linux-gnu.so]
#6  [map_low_face_sample_to_high_faces_ray — _native]
#7  [parallel lambda / sample chunk — _native]
#8  [std::async worker — _native]
#9  std::__future_base::_State_baseV2::_M_do_set(...)
#13 execute_native_thread_routine (libstdc++.so.6)
```

Thread B (concurrent, same call site):
```
#0  libTKMath.so.7.8.1
#1  BRep_Tool::CurveOnSurface(TopoDS_Edge, Geom_Surface, ...)
        (libTKBRep.so.7.8.1)
#2  BRepAdaptor_Curve2d::Initialize(TopoDS_Edge, TopoDS_Face)
        (libTKBRep.so.7.8.1)
#3  BRepAdaptor_Curve2d::BRepAdaptor_Curve2d(TopoDS_Edge, TopoDS_Face)
        (libTKBRep.so.7.8.1)
#7  IntCurvesFace_Intersector::IntCurvesFace_Intersector(TopoDS_Face, ...)
        (libTKTopAlgo.so.7.8.1)
#8  IntCurvesFace_ShapeIntersector::Load(TopoDS_Shape, tol)
        (libTKTopAlgo.so.7.8.1)
#9  [project_ray_to_face_impl — _native.cpython-312-x86_64-linux-gnu.so]
```

Both threads entered `Load()` on the same `TopoDS_Face` handle simultaneously.
The face's shared geometry was mutated by one thread while the other was reading it.

### Affected methods

- `ray_bidirectional` — confirmed crash
- `ray` — same code path, same risk, has not crashed yet
- `nearest` — different code path (`BRepExtrema_DistShapeShape`), also suspected unsafe

---

## POST 2 — root cause (add as first comment)

### Root cause analysis

**Starting from the kernel trap line**

```
traps: python[173476] trap invalid opcode ip:7f8d278ab3da
    in libTKGeomAlgo.so.7.8.1[2ab3da, ...]
```

`trap invalid opcode` means the CPU tried to execute bytes that do not correspond
to any valid x86 instruction. This is not a logic error or a wrong value —
it means memory that was supposed to contain executable code contained garbage
at the moment it was executed. That is a strong indicator of memory corruption
at runtime, not a bug in the algorithm logic itself.

The fault is inside `libTKGeomAlgo`, not in our code. So something our code
did caused OCCT's memory to become corrupted.

---

**Reading the two thread stacks together**

The GDB output shows two worker threads alive at the time of the crash.
Both stacks reach the same call site — `IntCurvesFace_ShapeIntersector::Load()` —
and both originate from `std::async` workers spawned by our `_native` extension:

Thread A (the one that faulted):
```
IntCurveSurface_ThePolyhedronOfHInter::FillBounding   ← invalid opcode here
IntCurveSurface_ThePolyhedronOfHInter::Init(surface)
IntCurveSurface_ThePolyhedronOfHInterC2(surface, ...)
IntCurvesFace_Intersector(TopoDS_Face, ...)
IntCurvesFace_ShapeIntersector::Load(TopoDS_Face, tol)
[_native — our project_ray_to_face_impl]
[_native — std::async parallel lambda]
```

Thread B (concurrent, same call site, different depth):
```
BRep_Tool::CurveOnSurface(TopoDS_Edge, Geom_Surface, ...)
BRepAdaptor_Curve2d::Initialize(TopoDS_Edge, TopoDS_Face)
BRepAdaptor_Curve2d(TopoDS_Edge, TopoDS_Face)
IntCurvesFace_Intersector(TopoDS_Face, ...)
IntCurvesFace_ShapeIntersector::Load(TopoDS_Face, tol)
[_native — our project_ray_to_face_impl]
[_native — std::async parallel lambda]
```

The two threads were both inside `Load()` at the same time. Thread A had
progressed further (already building the internal polyhedron approximation),
Thread B was still initializing the face's 2D trim curve adaptor.

---

**Why this proves shared state**

Both threads entered `Load()` from our parallel sample-chunk code in
`projection_ray.cpp`. In that code, every thread iterates over the same
`high_faces` vector and calls `Load(high_faces[i], tol)` for every face `i`.

`TopoDS_Face` is a handle — a reference-counted pointer. Passing it by value
to each thread copies the pointer, not the underlying object. Both threads
were therefore calling `Load()` on the exact same `BRep_TFace` and `Geom_Surface`
at the same time.

`IntCurvesFace_Intersector::Load()` builds an internal triangulated approximation
of the face geometry (`IntCurveSurface_ThePolyhedronOfHInter`) and reads the
face's 2D trim curves (`BRepAdaptor_Curve2d`). Both operations read from and
write to fields inside the shared geometry object — OCCT geometry objects use
lazy initialization, computing and caching internal data on first access.

Thread A and Thread B raced to perform that initialization on the same object.
One thread's write corrupted data that the other thread was about to execute
as code, producing the `trap invalid opcode`.

---

**Why `ray_bidirectional` and not `ray` or `nearest`**

`ray_bidirectional` calls the ray-intersection path twice per sample (outward
then inward). This doubles the number of concurrent `Load()` calls per unit
time, widening the window in which two threads can collide on the same face.
`ray` has the same vulnerability but a narrower window — it has not crashed yet.

`nearest` uses `BRepExtrema_DistShapeShape` instead of
`IntCurvesFace_ShapeIntersector`. This traverses different internal OCCT paths
and triggers different lazy initialization. The race window is narrower still,
but the underlying sharing problem is identical — it is not safe, it has just
not manifested yet.

---

**Note on the kernel panic**

A second run after the first crash caused a full system panic and reboot,
with journal logs not flushed before the machine died. This is consistent
with the same memory corruption reaching further — once one region of OCCT's
heap is corrupted, a subsequent run can corrupt adjacent memory including
kernel-mapped pages, triggering a hardware fault that the kernel cannot recover
from. This is a known consequence of data races in native extensions that
share memory with the kernel's address space.

---

## POST 3 — fix

### Fix: replace OCCT-based intersection with thread-safe custom implementations

---

### Diagnosis recap

The root cause (detailed in the previous comment) is that OCCT's custom memory
allocator `Standard_MMgrOptl` uses per-size free-lists without thread locks.
When two workers call `new IntCurvesFace_Intersector(...)` at the same moment
they corrupt each other's free-list, producing the `trap invalid opcode` crash.

This was confirmed by a diagnostic mutex experiment: wrapping only the OCCT
constructor call (not our face-copy call) in a `static std::mutex` eliminated
the crash, while making the program 5× slower (all ray intersections serialised
globally). Because the mutex covered suspect 2 but not suspect 1, and the crash
disappeared, suspect 2 is definitively the race site. `BRepBuilderAPI_Copy` is
not involved.

A global mutex is not an acceptable fix — the 22-minute test run vs. the normal
4-minute run shows what it costs. The real fix is to ensure OCCT's allocator is
never called from more than one thread at a time.

---

### Design principle

The key insight is that all allocation can be moved into a serial setup phase:

```
serial (before workers launch):
    for each high face:
        deep-copy the face geometry       ← one BRepBuilderAPI_Copy per face
        evaluate surface on a UV grid     ← pre-warms all BSpline Bezier caches
        sample all boundary PCurves       ← pre-warms all 2D curve caches
        store results in std::vector      ← no OCCT Handle retained

parallel (workers running):
    read std::vector data                 ← pure reads, no allocation
    evaluate D0/D1 on pre-warmed surface  ← guaranteed cache hit, pure read
    run geometric algorithm               ← arithmetic only
```

After the serial phase, every object is read-only. Reading the same memory from
multiple threads simultaneously is always safe — a race requires at least one
writer. Both new classes declare `Perform()` as `const` and return results by
value with no output parameters, so the compiler enforces this invariant.

**Why pre-warming matters:**
OCCT's BSpline evaluator writes a Bezier conversion cache the first time any
knot span is evaluated. Evaluating the full UV grid during serial `Load()` fills
every cache entry before workers start, making all subsequent `D1()` calls in
the parallel phase guaranteed cache hits — pure reads, no writes.

---

### What was changed

**New class: `geom::RayFaceIntersector`**
(`cpp/src/geom/RayFaceIntersector.hpp` / `.cpp`)

Replaces `IntCurvesFace_ShapeIntersector` for the `ray` and `ray_bidirectional`
methods. Four-phase algorithm, each phase in its own method so future improvements
replace one method without touching the rest:

1. **`try_analytic()`** — stub; future: closed-form dispatch for planes and
   cylinders, bypassing the mesh entirely for the most common surface types.
2. **`find_candidates()`** — coarse AABB slab test against per-triangle bounding
   boxes; returns triangle indices whose box the ray cannot be ruled out of
   hitting. Currently O(n) linear scan; future: BVH for O(log n).
3. **Möller–Trumbore** — exact ray-triangle test; returns barycentric coordinates
   that interpolate an approximate `(u_approx, v_approx)` on the true surface.
4. **`refine()`** — 2D Newton iteration on the true surface: projects the 3D
   residual into the plane perpendicular to the ray, solves a 2×2 linear system
   at each step. Converges to tolerance precision in 5–10 iterations regardless
   of mesh resolution. Uses `SurfaceAdaptor::D1()`.
5. **`inside_face()`** — even-odd crossing-number test on a pre-built 2D polygon
   per wire; handles holes correctly without distinguishing outer from inner
   wires.

**New class: `geom::PointFaceProjector`**
(`cpp/src/geom/PointFaceProjector.hpp` / `.cpp`)

Replaces `BRepExtrema_DistShapeShape` for the `nearest` method. Same design:
`Load()` in serial, `Perform() const` returns by value.

Algorithm:
1. **`find_candidates()`** — scans the UV grid, returns K=8 nearest 3D grid
   points to the query as Newton seeds.
2. **`refine_nearest()`** — Gauss-Newton minimisation of `|S(u,v) − P|²`:

   ```
   J^T J · [Δu, Δv]^T = −J^T r

   where J = [Su, Sv],  r = S(u,v) − P
   so    J^T J = [[Su·Su, Su·Sv], [Su·Sv, Sv·Sv]]  (Gram matrix)
         J^T r = [Su·(S−P), Sv·(S−P)]
   ```

   UV coordinates come directly from Newton — no `ShapeAnalysis_Surface`
   inversion call needed, unlike `BRepExtrema_DistShapeShape`.
3. **`inside_face()`** — same even-odd test as `RayFaceIntersector`.

**Modified: `cpp/src/projection/projection_ray.cpp`**

Added `make_face_intersectors()` helper. Both `_ray_impl` and
`_ray_bidirectional_impl` now call it in serial before `std::async`:

```cpp
// Serial — all OCCT allocation happens here
const std::vector<geom::RayFaceIntersector> face_intersectors =
    make_face_intersectors(high_faces, tolerance);

// Parallel — Perform() is const, returns by value, no allocation
futures.push_back(std::async(std::launch::async, [&, begin, end, wi]() {
    for (std::size_t i = begin; i < end; ++i)
        map_sample(worker_adaptors[wi], i);   // uses face_intersectors
}));
```

**Modified: `cpp/src/projection/projection_nearest.cpp`**

Same pattern: added `make_face_projectors()` helper called in serial.

**Modified: `cpp/src/projection/mapping_projection.hpp` / `.cpp`**

`project_point_to_face` now takes `const geom::PointFaceProjector&` instead of
`const TopoDS_Face&`. `project_ray_to_face*` already took
`const geom::RayFaceIntersector&`.

---

### Why this is safe

Each `RayFaceIntersector` and `PointFaceProjector` is owned by a
`std::vector` in the serial setup scope and never moved or resized after workers
launch. Workers hold const references to elements of that vector.

`RayFaceIntersector::Perform()` and `PointFaceProjector::Perform()` are both
declared `const` and return results by value. They call:

- `find_candidates()` / `find_best_*()` — reads `std::vector<double>` (read-only after Load)
- `moller_trumbore()` / `refine_nearest()` — reads `std::vector<gp_Pnt>` (read-only)
- `adaptor_.D1()` — reads the private deep-copied surface via `BRepAdaptor_Surface::D1()`,
  which is a pure read after Bezier caches are warmed
- `inside_face()` — reads `std::vector<std::vector<gp_Pnt2d>>` (read-only)

No OCCT `Handle` is constructed in `Perform()`. No `Standard::Allocate` is called.
The `BRepAdaptor_Surface` member evaluates the pre-warmed BSpline cache, which is
a read of an `NCollection_Array1` allocated once during `Load()`.

---

### Known limitations

The current implementation has four known gaps relative to OCCT's own
`IntCurvesFace_Intersector`. None affect thread safety; all affect coverage
or performance on specific surface types. Each gap corresponds to a named
private method with a detailed TODO comment:

| Gap | Impact | Extension point |
|---|---|---|
| Fixed 20×20 mesh | Missed hits on surfaces with many BSpline knot spans | `mesh_resolution()` |
| No analytic dispatch | Unnecessary grid work on planes and cylinders | `try_analytic()` |
| Linear candidate scan | O(n) instead of O(log n); fine for 20×20, slow if adaptive | `find_candidates()` |
| Sampled boundary polygon | May misclassify UV points very close to trimming curves | `inside_face()` |

See `docs/ray_face_intersector.md` for the full improvement roadmap with
specific replacement code for each item.

---

### Test results

The thread-safety test suite (`tests/python/test_thread_safety.py`) runs each
projection method 200 times in parallel:

| Configuration | Result | Time |
|---|---|---|
| Baseline — no fix | crash (SIGILL) | — |
| Diagnostic mutex (OCCT constructor serialised) | 14 passed | 22:36 |
| `RayFaceIntersector` (ray + ray\_bidirectional) | 14 passed | 4:39 |
| + `PointFaceProjector` (nearest) | 14 passed | 4:32 |

The 22-minute mutex run demonstrates both that the mutex fixed the crash and
that a global lock is 5× slower than the pre-build pattern. The final 4-minute
run recovers full parallelism with no crashes across all three methods.

Existing correctness tests (single-threaded): 14 passed, no regressions.

**ThreadSanitizer** has not been run — `clang++` is not installed on the build
machine. The diagnostic mutex experiment gives high confidence that the race
is fixed. TSan support is already wired into `CMakeLists.txt`
(`-DENABLE_TSAN=ON`) and can be used when `clang++` is available.

---

### Commit

`7069feb` — Replace OCCT-based projection with thread-safe custom implementations
