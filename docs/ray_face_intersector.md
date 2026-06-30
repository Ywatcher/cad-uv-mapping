# RayFaceIntersector — Algorithm, Design, and Improvement Roadmap

## Background

The original ray-face intersection in this codebase used OCCT's
`IntCurvesFace_ShapeIntersector`. Under multi-threaded execution this class
races on OCCT's internal memory allocator (`Standard_MMgrOptl`), which uses
per-size free-lists without locking. Concurrent calls from different worker
threads to `IntCurvesFace_Intersector`'s constructor — which allocates the
internal polyhedron as a `Handle`-based heap object — corrupt those free-lists,
leading to a `SIGILL` / `SIGSEGV` crash within `libTKGeomAlgo`.

The fix was confirmed by a diagnostic: wrapping the OCCT calls in a global
`std::mutex` eliminated the crash, proving the race is inside OCCT's allocator
rather than in our face-copy logic.

`geom::RayFaceIntersector` is a thread-safe replacement. It implements the same
four-phase algorithm using only `std::vector` storage and direct surface
evaluation, with no OCCT `Handle` allocations in the hot path.

---

## Algorithm

The four phases run in sequence for each call to `Perform()`.

### Phase 1 — Analytic dispatch (`try_analytic`)

*Currently a stub that returns `nullopt`.*

For analytic surface types (plane, cylinder, sphere, cone, torus) a closed-form
intersection can be computed without a mesh at all. This phase is the hook for
those cases.

### Phase 2 — Coarse screen (`find_candidates`)

A UV parameter grid is sampled during `Load()` and connected into triangles (two
per quad cell). Each triangle gets an axis-aligned bounding box (AABB) stored as
six contiguous doubles in `boxes_`. `find_candidates()` runs a slab test (the
standard ray-AABB algorithm) against every triangle box and returns the indices
of triangles that the ray cannot be ruled out of hitting.

This phase is O(n) in the number of triangles.

### Phase 3 — Exact triangle test (Möller–Trumbore)

For each candidate triangle from phase 2, the Möller–Trumbore algorithm computes
the exact ray-triangle intersection. The algorithm operates on the three 3D
vertex positions of the triangle and returns:

- `w` — the signed ray parameter (distance along the ray direction from the
  origin to the intersection)
- `s`, `t` — barycentric coordinates of the hit within the triangle
  (vertex weights are `1-s-t`, `s`, `t` for vertices `i0`, `i1`, `i2`)

The barycentric coordinates are then used to interpolate the UV parameters at
the three vertices of the triangle, giving an approximate `(u_approx, v_approx)`
on the true surface.

### Phase 4 — Newton refinement (`refine`)

The approximate `(u_approx, v_approx)` is the starting point for a 2D Newton
iteration on the true surface. At each step:

1. Evaluate `D1(u, v)` to get the 3D point `P` and tangent vectors `Pu`, `Pv`.
2. Project `P` onto the ray: `t = (P − origin) · direction`.
3. Compute the residual `r = (P − origin) − t · direction` — the component of
   `(P − origin)` perpendicular to the ray direction. At the true intersection
   this is zero.
4. Project `r` onto two basis vectors `e1`, `e2` perpendicular to the ray to get
   the 2D residual `(g1, g2)`.
5. Solve the 2×2 Newton system `J [Δu, Δv]ᵀ = −[g1, g2]ᵀ` where
   `J = [[Pu·e1, Pv·e1], [Pu·e2, Pv·e2]]`.
6. Update `(u, v)` and clamp to the face's UV bounds.

Newton's method has quadratic convergence near the true solution. The starting
point from Möller–Trumbore is close enough that convergence typically takes 5–10
iterations. The final `(u, v, w, pt)` is as accurate as the `tolerance_`
parameter regardless of mesh resolution.

### Phase 5 — Boundary classification (`inside_face`)

The refined `(u, v)` must lie inside the face's trimming boundary. During
`Load()`, every PCurve on every boundary edge is sampled to build a 2D polygon
per wire. `inside_face()` runs an even-odd crossing-number test: a horizontal
ray is cast from `(u, v)` in the `+u` direction and the number of crossings
with the polygon edges is counted. An odd count means the point is inside.

The even-odd rule correctly handles faces with holes (inner wires) without
needing to distinguish outer from inner wires, because each hole adds another
crossing layer that flips the parity.

---

## Thread-safety guarantee

`Load()` builds all mutable state: surface mesh, bounding boxes, and boundary
polygon. After `Load()` returns, the object is read-only.

`Perform()` is `const` and returns a `RayResult` by value. It calls only:

- `find_candidates()` — reads `boxes_` (read-only after Load)
- `moller_trumbore()` — reads `points_` and `triangles_` (read-only)
- `adaptor_.D1()` — reads the private face's pre-warmed surface (read-only)
- `inside_face()` — reads `boundary_wires_` (read-only)

No OCCT `Handle` is constructed, no `Standard::Allocate` is called. Multiple
threads can call `Perform()` on the same loaded instance simultaneously with no
shared mutable state.

The one contract the caller must observe: `Load()` must not be called
concurrently on the same instance. It is setup, called once in serial before
workers launch.

---

## Gaps relative to OCCT's original implementation

### 1. Fixed mesh resolution

OCCT's `IntCurvesFace_Intersector` calls
`BRepTopAdaptor_TopolTool::SamplePnts()` which queries `NbUIntervals(GeomAbs_C1)`
and `NbVIntervals(GeomAbs_C1)` — the number of C1-continuity knot spans — and
places approximately 4 sample points per span. For a BSpline surface with
`m × n` knot spans this produces `4m × 4n` sample points, ensuring every span
is covered by several triangles.

Our `mesh_resolution()` currently returns a fixed `{20, 20}` regardless of
surface complexity. For a face with 2×3 knot spans this is generous (10 cells
per span). For a face with 15×8 knot spans this gives only 1–2 cells per span,
which can miss hits when a ray clips near a knot boundary.

**Impact**: missed intersections on surfaces with many knot spans. The final
accuracy of hits that are found is unaffected (Newton refinement is independent
of mesh resolution).

### 2. No analytic surface dispatch

OCCT detects the surface type before building any mesh. For planes, cylinders,
spheres, cones, and tori it calls dedicated analytic intersection algorithms
(`IntAna_IntConicQuad`, `IntAna_IntLinTorus`) that produce exact results in O(1)
arithmetic, bypassing the mesh entirely.

Our `try_analytic()` always returns `nullopt`, so every face goes through the
full mesh path — including planar and cylindrical faces, which are the most
common types in mechanical CAD.

**Impact**: unnecessary computation on analytic faces. A flat face needs exactly
one plane-line equation to intersect; we instead sample 441 points, build 800
triangles, run 800 slab tests, and run Newton refinement.

### 3. Linear spatial scan

OCCT's `Bnd_BoundSortBox` is a sorted bounding-box structure that can answer
"which boxes does this ray intersect?" in O(log n) comparisons. Our
`find_candidates()` scans all triangles linearly in O(n).

**Impact**: for a fixed 20×20 mesh (800 triangles) this is negligible. With an
adaptive mesh on a complex surface the triangle count could reach 10,000+, at
which point O(log n) vs O(n) becomes significant.

### 4. Sampled boundary polygon vs topology classifier

OCCT's `BRepTopAdaptor_TopolTool::Classify()` evaluates the exact PCurve
geometry to classify a UV point relative to the face's trimming boundary.

Our `inside_face()` uses a 2D polygon built by sampling the PCurves at discrete
points. For boundary curves that are exactly linear or circular the polygon
approximation is accurate. For BSpline PCurves with high curvature the sampled
polygon may misclassify a point near the boundary.

**Impact**: borderline hits — UV points very close to the trimming boundary —
may be classified incorrectly (included when they should be rejected, or
vice versa). Interior hits are unaffected.

---

## Planned improvements

Each improvement is a single-method replacement with no changes to `Perform()` or
any caller. The methods are already structured as extension points in the code.

### `mesh_resolution()` → adaptive sizing

Replace the fixed `{20, 20}` with:

```cpp
const int nu = std::max(8, adaptor_.NbUIntervals(GeomAbs_C1) * 4);
const int nv = std::max(8, adaptor_.NbVIntervals(GeomAbs_C1) * 4);
return {nu, nv};
```

This matches OCCT's sampling strategy. `NbUIntervals` and `NbVIntervals` are
already available through `BRepAdaptor_Surface`, which is the underlying member
of `SurfaceAdaptor`. `SurfaceAdaptor` would need to expose those methods (a
two-line addition). The rest of `build_mesh()` already reads resolution through
`mesh_resolution()` and needs no further change.

### `try_analytic()` → closed-form dispatch

Check `adaptor_.GetType()` and handle the common cases first:

- `GeomAbs_Plane` via `IntAna_IntConicQuad` — one linear equation
- `GeomAbs_Cylinder` via `IntAna_IntConicQuad` — quadratic
- `GeomAbs_Sphere` via `IntAna_IntConicQuad` — quadratic
- `GeomAbs_Cone` via `IntAna_IntConicQuad` — quadratic

For each: solve analytically, evaluate the surface at the hit parameter(s) to
get `(u, v, pt)`, filter by `pmin`/`pmax` and `inside_face()`, and return a
`RayResult`. Planes and cylinders alone cover the majority of faces in typical
mechanical assemblies.

### `find_candidates()` → BVH

Build a binary BVH during `Load()` after `build_mesh()`. Store it as a flat
array (`2n − 1` nodes for `n` leaves). In `find_candidates()`, traverse the
tree: slab-test each node's AABB against the ray and skip the subtree on a miss.
The interface is unchanged — the method still returns `std::vector<std::size_t>`.

### `inside_face()` → topology classifier

Replace the crossing-number test with
`BRepTopAdaptor_TopolTool::Classify(uv, tolerance_)`. The `TopolTool` would be
constructed once during `Load()` (from `private_face_`) and stored as a member.
Since all PCurves are pre-warmed in `build_boundary()`, concurrent calls to
`Classify()` would be pure reads provided `TopolTool` holds no mutable cache
of its own — this needs to be confirmed before the replacement is made.
