# Projection Fix Proposal

Status: proposal / not yet implemented.
Audience: the maintainer, and any agent picking this up cold.
Scope: the low-to-high mapping stage only (`cpp/src/projection/`,
`cpp/src/mapping.cpp`, `cpp/include/cad_uv_map/mapping_context.hpp`). Sampling,
surface evaluation, and the Python bridge are out of scope except where noted.

This document proposes how to fix correctness problems in the two current
projection methods (`nearest` and `ray`). It is written so it can be executed
in phases without re-deriving the analysis. The empirical evidence that motivated
it is in the appendix at the end — read that first if you doubt any claim here.

---

## 1. TL;DR

The two current methods are each blind to half the test cases, and the way they
select a hit (global nearest distance / nearest forward intersection) is the
wrong criterion for high-to-low baking.

- `nearest` is correct for coincident and **recessed** (carved-in) detail, but
  maps **raised** detail (ribs) onto the smooth base and silently drops it.
- `ray` is correct for **raised** outward detail, but no-hits coincident and
  recessed detail, has an **uncapped** forward search that can return far wrong
  hits, and has no back-face guard.

**Proposed fix:** replace the two peer methods with one **cage / envelope
projection** (the standard production high-to-low baking model), keep
nearest-point projection as a *fallback* (not a peer), and add a small set of
validation guards (distance cap, normal agreement, trim classification, honest
status) that every candidate must pass. Add a per-high-face bounding-box cull and
adaptor caching so the extra work stays affordable.

---

## 2. Why the current selection criterion is wrong

For high-to-low baking, each low-face sample must capture the high-surface detail
that the low face is *standing in for*. That is **not** the globally nearest high
surface.

Concrete failure (measured, see appendix): on `pedestal_ribs`, the high base
surface is **coincident** with the low side face (distance ~0), and the ribs
protrude ~0.58 mm outward as separate faces. Therefore:

- "globally nearest surface" (`nearest`) always picks the coincident base →
  ribs never selected.
- "nearest forward intersection from the surface" (`ray`) flies outward and only
  hits ribs where a rib is in front → everything else no-hits.
- "nearest hit searching both directions along the normal" would *also* pick the
  coincident base, because it is at distance ~0.

The criterion that actually distinguishes a rib from the base is **"first surface
hit by a ray that starts outside all high geometry and travels inward along the
low normal."** That is the cage/envelope model below.

---

## 3. Proposed method: cage / envelope projection

### 3.1 Algorithm (per low-face sample)

```
input:  low surface adaptor, low face orientation, sample uv,
        high faces (+ their cached bbox/adaptor), MappingContext ctx
output: MappingResult

1. P = low_surface.Value(u, v)
2. props = BRepLProp_SLProps(low_surface, u, v, 1, ctx.projection_tolerance)
   if !props.IsNormalDefined():   -> go to step 7 (nearest fallback)
   n = props.Normal(); if face REVERSED: n.Reverse()    // outward unit normal

3. // Build the cage segment along the low normal.
   O   = P + n * ctx.frontal_distance          // origin, outside all features
   dir = -n                                     // travel inward
   t_lo = 0
   t_hi = ctx.frontal_distance + ctx.rear_distance

4. // Find the FIRST hit along the segment, across candidate high faces.
   best = none
   for hf in high_faces culled by bbox-vs-segment (section 6):
       try:
           cand = first_forward_hit_on_face(O, dir, t_lo, t_hi, hf, ctx)
       catch: record failed-for-this-face; continue
       if cand is none: continue
       if !passes_guards(cand, n, hf, ctx): continue   // section 4
       if best is none or cand.t < best.t - ctx.projection_tolerance:
           best = cand; ambiguous = false
       elif |cand.t - best.t| <= ctx.ambiguity_epsilon:
           ambiguous = true

5. if best exists: return make_hit_result(..., best, ambiguous,
                                          source = cage_ray)
6. // normal line missed every high face along the cage segment
   -> go to step 7 (nearest fallback) before giving up

7. // Fallback: nearest-point projection (silhouettes, grazing, undefined normal)
   cand = nearest_point_across_faces(P, high_faces, ctx)   // bounded by max_search
   if cand and passes_guards(cand, n, hf, ctx):
       return make_hit_result(..., cand, ..., source = nearest_fallback)
   return make_no_hit_result(...)   // honest no_hit
```

`first_forward_hit_on_face` is today's `project_ray_to_face`
(`mapping_projection.cpp`) with two changes: (a) the line bounds are the finite
cage segment `[t_lo, t_hi]` instead of `[-max, +max]`, and (b) it returns the hit
with the **smallest** `t` from `O` (outermost surface), not the smallest distance
from `P`.

### 3.2 Why this gets every case right

| Case            | First inward hit from cage              | Outcome                  |
|-----------------|-----------------------------------------|--------------------------|
| identity        | coincident surface at `t ≈ frontal`     | hit, correct UV          |
| u/v groove      | top outside groove; wall/floor inside   | hit, captures recess     |
| pedestal ribs   | **rib** over a rib, base elsewhere      | hit, captures rib        |
| no coverage     | segment exits with no intersection      | honest `no_hit`          |
| silhouette/edge | normal line slips past → fallback fires | hit via nearest fallback |

The two current methods become degenerate configurations of this one:
`frontal_distance = 0` reduces it toward the inward/nearest behavior;
cage-from-surface with no rear search reproduces today's `ray`.

### 3.3 Public API

Keep the existing entry points working as thin wrappers so nothing breaks:

- `..._nearest(...)`  -> cage with `frontal_distance = 0` (pure inward + fallback)
- `..._ray(...)`      -> cage with default frontal/rear (outward envelope)
- `..._hybrid(...)` (new, becomes the default `map_single_low_face_samples_to_high_faces`)
  -> full cage + fallback as in 3.1

Implement the core once in `cpp/src/projection/`; the three public names differ
only by the `MappingContext` they pass down.

---

## 4. Validation guards (apply to every candidate, ray or fallback)

These live in the shared helper file `cpp/src/projection/mapping_projection.cpp`
as a single `passes_guards(candidate, low_normal, high_face, ctx)` predicate.
Each guard fixes a specific measured bug.

1. **Search-distance cap.** Reject `|candidate.distance| > max_search`
   (`max_search = frontal_distance + rear_distance`). Fixes `ray`'s uncapped
   `Perform(-max, +max)` returning far, unrelated hits on non-convex shapes.

2. **Normal-agreement gate.** Compute `high_n` at the hit UV; reject if
   `low_n · high_n < ctx.normal_agreement_min` (default `0.0` → reject any
   back-face; set to `cos(theta)` for a looser cone). Today nothing rejects
   back-faces — current `ray` hits are front-facing only by luck of geometry.

3. **Trim classification.** Classify the recovered `high_uv` against the face
   trim with `BRepTopAdaptor_FClass2d`. If outside → return status
   `outside_trim` rather than a fake `hit`. The cage ray via
   `IntCurvesFace_ShapeIntersector` is already on-trim; the **nearest fallback is
   not** (`ShapeAnalysis_Surface::ValueOfUV` works on the *untrimmed* surface),
   so this guard matters most there.

4. **Honest status / stop swallowing errors.** The current
   `catch (const std::exception&) { continue; }` blocks in
   `projection_nearest.cpp` and `projection_ray.cpp` make every failure
   invisible. Record `failed` for the offending face; only return `no_hit` when
   all faces legitimately produced no candidate.

5. **Meaningful ambiguity epsilon.** Replace the `1e-7` tie threshold (which is
   "exact tie", so it fires on noise) with `ctx.ambiguity_epsilon` in model
   units. `ambiguous` should mean "two candidates genuinely within epsilon",
   typically at a shared face boundary or a true fold.

---

## 5. Config and data-model changes

### 5.1 `MappingContext` (`cpp/include/cad_uv_map/mapping_context.hpp`)

```cpp
struct MappingContext {
    double projection_tolerance = 1e-7;   // geometry/OCCT tol (was: tolerance)
    double frontal_distance     = -1.0;   // cage offset outward; <0 => auto
    double rear_distance        = -1.0;   // inward search depth; <0 => auto
    double ambiguity_epsilon    = -1.0;   // tie threshold, model units; <0 => auto
    double normal_agreement_min = 0.0;    // cos(theta); reject back-faces
    bool   enable_parallel      = false;
    bool   preserve_input_order = true;
};
```

**Auto-derive defaults from geometry, not magic constants.** When a distance is
`< 0`, compute it once from the high shape's `Bnd_Box` diagonal `D`
(`BRepBndLib::Add`): e.g. `frontal_distance = rear_distance = 0.05 * D`,
`ambiguity_epsilon = 1e-4 * D`. Tune against the fixtures (ribs are ~0.58 mm on a
~30 mm-diagonal part, so 5% ≈ 1.5 mm clears the feature).

Note: renaming `tolerance` -> `projection_tolerance` touches the pybind binding
in `cpp/src/bindings.cpp` and any Python caller (`MappingContext().tolerance`).
Either rename everywhere or keep `tolerance` as an alias. Prefer keeping an alias
to avoid breaking `tests/python/test_native_mapping.py`.

### 5.2 `MappingResult` (`cpp/include/cad_uv_map/mapping.hpp`)

- Define `distance` precisely: **signed distance along the low outward normal**
  for cage hits (positive = outward of `P`); Euclidean for nearest-fallback hits.
  Document this in the header — consumers currently cannot tell the two apart.
- Add `enum class MappingSource { cage_ray, nearest_fallback };` and a `source`
  field, so downstream code/tests know which path produced each sample.
- Optional (enables README future-work "one low sample -> multiple high
  regions"): carry a second-best candidate when the cage ray pierces multiple
  layers, so a fold can be reported rather than silently collapsed.

Adding fields means updating the NumPy structured-array exporter
(`cpp/src/numpy_exports.cpp`) and the wrapper dtype in
`python/cad_uv_map/types.py` (`mapping_batch_to_structured_array` field list).

---

## 6. Performance (required, not optional)

The cage ray plus guards touch more faces per sample than today. Without these
two changes the cost blows up from the current `O(samples x faces)` full queries
with per-(sample, face) OCCT object rebuilds. Both are already named as intended
in `docs/structure.md` but unused.

1. **Per-high-face `Bnd_Box`,** built once via `BRepBndLib::Add`. Cull any high
   face whose box the cage segment cannot reach (segment-vs-box test) or whose
   box is farther than the current best `t`. This changes the complexity class
   for parts with many high faces.

2. **Cache `BRepAdaptor_Surface` / `ShapeAnalysis_Surface` per high face** and
   reuse across all samples. These setups are the documented hot spot
   (`docs/mapping_model.md`: "the real cost comes from OCCT surface adaptor setup
   and per-sample geometry evaluation"). Today `evaluate_high_face_sample`
   (`mapping.cpp`) and `project_point_to_face` rebuild them per sample/face.

Keep the existing chunked parallelism, but be careful about the nested
parallelism in `map_multiple_low_face_sample_groups_to_high_faces` x the
per-sample worker split — coordinate them or cap total threads to avoid
oversubscription.

---

## 7. Suggested execution order (each phase independently shippable)

1. **Regression fixture first.** Promote the appendix probes into a real test:
   an all-cases table asserting per-case hit/`no_hit`/`status` counts and the set
   of selected high-face ids (the strongest signal — e.g. ray on pedestal must
   select `{9,17,25,33}`; nearest must select only the base). This locks current
   behavior before any change. Put it in `tests/python/` and add the cases to
   `tests/fixtures/cad_cases.py` if missing.

2. **Phase 1 — guards on the existing methods** (section 4): distance cap,
   normal gate, trim -> `outside_trim`, honest `failed`, meaningful ambiguity
   epsilon. No new algorithm; pure correctness. Stops the silent-wrong-hit
   failures immediately.

3. **Phase 2 — cage hybrid** (section 3) with nearest as fallback. Re-run the
   table; identity/grooves/ribs should all pass with one method.

4. **Phase 3 — bbox cull + adaptor caching** (section 6). Benchmark on a dense
   pedestal grid before/after.

5. **Unrelated but blocking:** bind the missing default constructors so the
   surface-eval round trip works. `IndexedSurfaceEvalResult` (and check the other
   `Indexed*`/batch types) currently raises
   `TypeError: ...IndexedSurfaceEvalResult: No constructor defined!`, failing 3
   tests in `tests/python/test_native_mapping.py`. Fix in `cpp/src/bindings.cpp`
   the same way the batch constructors were fixed (see
   `report/native_numpy_batch_round_trip_report.md`).

---

## 8. Risks / open questions

- **Cage distance defaults.** Too small -> misses tall features (regresses to
  `ray`'s no-hit); too large -> the inward ray may pass through a thin neighbour
  before reaching the intended surface. Deriving from the bbox diagonal is a
  starting heuristic; expose it and test per fixture.
- **Parallax from using the low normal as cast direction.** When low and high
  diverge sharply in orientation the cage ray lands off the true corresponding
  point. The nearest fallback partly covers this; a future refinement is a
  smoothed/averaged cage normal.
- **Distance-field meaning now varies by `source`.** Documented in 5.2, but any
  consumer that thresholds on `distance` must read `source` too.
- **Folds still collapse** unless the optional second-candidate record (5.2) is
  implemented; this proposal makes folds *detectable* (ambiguous + multi-hit) but
  does not resolve which layer a sample "should" belong to. That is the README's
  open envelope/cage prepass item.

---

## Appendix A — How this was tested and what was found

### A.1 Environment / build

The installed native module was stale (built before the `numpy_exports` split;
`undefined symbol: cad_uv_map::to_numpy_vec3_array`), so nothing imported. Rebuilt
against the `cad-uv-map` conda env's OCCT:

```bash
conda activate cad-uv-map      # /mnt/D/packages/miniconda3/envs/cad-uv-map
cmake -S . -B build-fresh -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -Dpybind11_DIR=$(python -c "import pybind11; print(pybind11.get_cmake_dir())")
cmake --build build-fresh
cp build-fresh/_native*.so python/cad_uv_map/        # and into the installed pkg dir
```

Note: the package is installed editable pointing at a second identical copy of
this repo; the loaded `.so` is the one in the env's `site-packages/cad_uv_map/`,
so that copy must be overwritten too when rebuilding.

### A.2 Test suite

`pytest tests/python` -> **14 passed, 3 failed**. The 3 failures are the
`IndexedSurfaceEvalResult: No constructor defined!` pybind plumbing bug
(section 7.5), **not** the mapping algorithm. All mapping-correctness tests pass.

### A.3 Probe: default `nearest` over every case

12x12 grid per low face, `enable_parallel=True`. Reported per case:
hit/ambiguous/no_hit counts, distance min/median/max, and the **set of high-face
ids actually selected** (the key signal).

```
identity_box      hit=144 amb=  0 nohit= 0  dist 0.000/0.000/0.000  faces={0}
flat_to_u_groove  hit=136 amb=  8 nohit= 0  dist 0.000/0.000/0.359  faces={2,23,24,27,30,31,34,35,38}
flat_to_v_groove  hit=144 amb=  0 nohit= 0  dist 0.000/0.000/0.254  faces={1,4,13,15,17,18,24,27,29,30,33,35}
pedestal_ribs     hit= 96 amb= 48 nohit= 0  dist 0.000/0.000/0.369  faces={3}     <-- ribs never selected
pedestal_nofold   hit= 96 amb= 48 nohit= 0  dist 0.000/0.000/0.162  faces={2}     <-- ribs never selected
```

Finding: `nearest` is exact on identity, correctly captures recessed grooves
(multiple wall faces, distance = groove depth), but on the raised-rib cases it
maps **all** samples onto the single smooth base face and selects **zero** rib
faces — the raised detail is silently dropped while every sample reports
`hit`/`ambiguous`.

### A.4 Probe: `ray` across coincident / recessed / raised cases

```
identity_box      hit=  0 amb=0 no_hit=144   (coincident hit filtered by t<=tol)
u_groove(top)     hit=  0 amb=0 no_hit=144   (outward normal points away from recess)
v_groove(top)     hit=  0 amb=0 no_hit=144   (same)
pedestal(side)    hit= 48 amb=0 no_hit= 96   faces={9,17,25,33}  dist 0.548/0.581/0.592
```

Finding: `ray` is the complement of `nearest` — it captures raised ribs (and only
ribs) and no-hits coincident and recessed detail. Confirms neither method is
correct alone.

### A.5 Probe: back-face check on `ray` hits (corrected)

First attempt used a single face-center normal as a proxy for the low side
face's normal and reported a misleading 24/24 same/back split — **wrong**,
because the side face is a revolved surface whose normal rotates around Z. Redone
with the true per-sample low normal (via surface-evaluating the low face) and the
reconstructed ray direction `(high_point - low_point)`:

```
side face_id=1  total=144  hits=48
low_n . high_n : same-side(>0)=48  opposed(<0)=0   [min/med/max]=1.00/1.00/1.00
raydir . high_n: outer-face(>0)=48 back/inner(<0)=0 [min/med/max]=1.00/1.00/1.00
```

Finding: on this fixture every `ray` hit is a clean front-face hit. But nothing
in the code enforces that — it is a property of the geometry, not a guard. Hence
the normal-agreement gate (section 4.2) and, since the ray search is uncapped
(`Perform(-max, +max)`), the distance cap (section 4.1) are needed for
non-convex shapes where these would not hold.

### A.6 Code locations referenced

- `cpp/src/projection/projection_nearest.cpp` — nearest loop, swallows errors.
- `cpp/src/projection/projection_ray.cpp` — ray loop, `t<=tol` filter.
- `cpp/src/projection/mapping_projection.cpp` — shared helpers
  (`project_point_to_face`, `build_low_face_ray`, `project_ray_to_face`,
  `make_hit_result`, `make_no_hit_result`, `mapping_tolerance`); the uncapped
  `Perform(-max, +max)` is here.
- `cpp/src/mapping.cpp` — orchestration; `evaluate_high_face_sample` rebuilds
  adaptors per sample.
- `cpp/include/cad_uv_map/mapping_context.hpp` — `MappingContext`.
- `cpp/include/cad_uv_map/mapping.hpp` — `MappingResult`, `MappingStatus`.
