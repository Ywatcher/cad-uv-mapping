# Thread Safety Fix: Plan

## Problem

Running `ray` or `ray_bidirectional` projection with `enable_parallel = true` causes
a SIGILL crash (confirmed: PID 173372, `libTKGeomAlgo.so.7.8.1`,
`IntCurvesFace_ShapeIntersector`). The `nearest` method shares the same root cause
and is also unsafe, just with a narrower race window.

### Root cause

`TopoDS_Face` is a handle (reference-counted pointer). Copying it by value copies the
pointer, not the underlying `BRep_TFace` or its `Geom_Surface`. OCCT geometry objects
(especially `Geom_BSplineSurface`) lazily write internal evaluation caches on first
access. When multiple threads call OCCT algorithms on the same face handle
simultaneously, they race to initialize those caches → memory corruption → SIGILL.

Current parallel axis in `projection_ray.cpp`: sample-chunks. Every thread iterates
over all `high_faces`, so all threads share all face handles simultaneously.

### Why `ray_bidirectional` crashed first

It calls the ray-intersection path twice per sample (outward + inward), doubling
the concurrent OCCT call rate and widening the race window. `nearest` is also
unsafe but less likely to manifest in practice.

### Why previous `nearest` runs did not crash

Different OCCT code path (`BRepExtrema_DistShapeShape` vs
`IntCurvesFace_ShapeIntersector`), which may traverse the lazy-cache path less
aggressively. Race conditions are probabilistic; `nearest` just has a narrower window.

---

## Chosen solution: Plan B — thin abstraction layer

### Core idea

Split projection into two phases:

1. **Prep phase (serial, M times):** convert each `TopoDS_Face` into a `PreparedFace`
   that owns all required data and has no shared mutable OCCT state.
2. **Parallel phase:** query functions (`intersect_ray`, `project_nearest`) operate
   only on `PreparedFace` — read-only, no OCCT handles shared between threads.

The orchestration code in `projection_ray.cpp` / `projection_nearest.cpp` gains one
prep step before the `std::async` loop. Everything else (chunking, futures, index
management) is unchanged.

### Why not just substitute at the leaf function

`PreparedFace` construction must happen before the parallel phase — not inside the
per-`(sample × face)` leaf function, or it gets rebuilt `N × M` times. Any fix
requires lifting prep to before the `std::async` call; Plan B makes that explicit
and purposeful rather than implicit.

### Consistency with existing code

`evaluate_multiple_high_face_samples` in `mapping.cpp` already uses this pattern:
group by face (serial), then parallelize over face groups. Plan B applies the same
structure to the projection layer.

---

## PreparedFace abstraction

```
PreparedFace                        — owns face data, read-only after construction
prepare(TopoDS_Face) → PreparedFace — serial, called M times before parallel phase
intersect_ray(ray, PreparedFace)    — parallel-safe, replaces IntCurvesFace_*
project_nearest(pt, PreparedFace)   — parallel-safe, replaces BRepExtrema_*
evaluate(uv, PreparedFace)          — parallel-safe, replaces BRepAdaptor_Surface
```

No virtual dispatch, no templates. Different implementation plans are different
versions of `PreparedFace` and the three query functions.

---

## Implementation options for PreparedFace (pick one)

### Plan 1 — geometry deep copy (simplest, lowest risk)

Prep: call `Geom_Surface::Copy()` for each face → private geometry object with its
own uninitialized cache. Parallel threads construct `GeomAdaptor_Surface` from the
private copy. OCCT algorithms used as-is, but referencing private geometry.

- Risk: lowest — uses OCCT exactly as designed, just with independent objects.
- Cost: M geometry copies in memory (one per high face).

### Plan 2 — stateless evaluator (intermediate)

Prep: extract poles, knots, mults, weights, trim wires into plain C++ arrays owned
by `PreparedFace`. Parallel query functions call `BSplSLib::DN()` directly on those
arrays — stateless free function, no OCCT objects, no cache.

Newton iteration for ray intersection: solve `S(u,v) = origin + t·dir` with
derivatives from `BSplSLib::DN()`.

- Risk: none from OCCT — parallel code has no OCCT objects.
- Cost: implement Newton iteration for ray intersection and nearest-point projection.
         Fall back to OCCT for analytical surfaces (plane, cylinder, etc.) which
         have no lazy cache issues.

### Plan 3 — local-cache evaluator (full solution)

Same as Plan 2, but add an explicit `LocalEvalCache` (stack-local per call) that
stores the evaluated Bezier segment for the last knot span — mirrors what OCCT's
cache does, but private to the caller.

```cpp
gp_Pnt evaluate(const PreparedFace& f, double u, double v, LocalEvalCache& cache);
```

Cache is passed in by the caller, never stored in `PreparedFace`. For face-outer
parallelism, one thread processes all N samples for one face with one cache → high
span-hit rate for adjacent UV samples, same speedup OCCT's cache was designed for.

- Risk: none from OCCT.
- Cost: Plan 2 plus cache struct and knot-span lookup logic.

---

## Recommended sequence

1. **Plan 1** to fix the crash quickly with minimal code risk.
2. **Plan 3** as a follow-on, pairs naturally with the `PreparedShape` performance
   work (prep extraction is the same step; reuse across multiple API calls is free).

---

## What is not changed

- Algorithm logic inside `project_ray_to_face_impl`, `project_nearest_impl` — untouched.
- Parallel orchestration shape (futures, chunking, index management).
- Python API and test suite.
- The `evaluate_multiple_high_face_samples` path — already safe (face-outer grouping).
