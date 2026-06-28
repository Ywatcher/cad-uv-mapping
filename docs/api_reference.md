# API Reference

This page lists the current step-oriented APIs, their Python signatures, the
matching unwrapped/native entry points, and the key datatypes used by each
stage.

The Python layer in `cad_uv_map.api` is the stable entry surface for notebooks,
tests, and demos. The native module is exposed as `cad_uv_map._native`.

## Step Overview

| Step | Python wrapper | Native bridge | What it returns |
| --- | --- | --- | --- |
| Face inspection | `describe_brep_faces`, `describe_shape_faces` | `_native.describe_brep_faces`, `_native.describe_brep_bytes` | `list[FaceInfo]` |
| Face debug print | `debug_print_brep_faces`, `debug_print_shape_faces` | `_native.debug_print_brep_faces`, `_native.debug_print_brep_bytes` | `None` |
| UV sampling | `sample_shape_face_uniform_uv_grid`, `sample_shape_face_uniform_uv_grid_batch`, `sample_shape_face_uniform_uv_tolerance_grid`, `sample_shape_face_uniform_uv_tolerance_grid_batch` | `_native.sample_brep_face_uniform_uv_grid`, `_native.sample_brep_face_uniform_uv_grid_batch`, `_native.sample_brep_face_uniform_uv_tolerance_grid`, `_native.sample_brep_face_uniform_uv_tolerance_grid_batch` | `FaceUvSampleGroup` or `FaceUvSampleGroupBatch` |
| Low-to-high mapping | `map_shape_single_low_face_samples_to_high_faces`, `map_shape_single_low_face_samples_to_high_faces_nearest`, `map_shape_single_low_face_samples_to_high_faces_ray` | `_native.map_brep_single_low_face_samples_to_high_faces`, `_native.map_brep_single_low_face_samples_to_high_faces_nearest`, `_native.map_brep_single_low_face_samples_to_high_faces_ray` | `MappingResultBatch` |
| Grid mapping | `map_shape_single_low_face_uv_grid_to_high_face_uv_grid`, `map_shape_single_low_face_uv_grid_to_high_face_uv_grid_nearest`, `map_shape_single_low_face_uv_grid_to_high_face_uv_grid_ray` | same native mapping functions as above, followed by NumPy reshaping | `np.ndarray` structured grid |
| Mapping export | `mapping_batch_to_numpy_structured_array` | `MappingResultBatch.to_numpy_structured_array()` via conversion helper | `np.ndarray` structured array |
| Surface evaluation | `evaluate_shape_single_high_face_samples`, `evaluate_shape_multiple_high_face_samples` | `_native.evaluate_brep_single_high_face_samples`, `_native.evaluate_brep_multiple_high_face_samples` | `SurfaceEvalResultBatch` |
| Full pipeline | `map_and_evaluate_shape_multiple_low_face_samples` | `_native.map_and_evaluate_brep_multiple_low_face_samples` | `MappedSampleBatch` |
| Debug sample print | `debug_print_shape_uv_sample_batch`, `debug_print_shape_uv_samples` | `_native.debug_print_brep_uv_sample_batch` | `None` |

## Step APIs

### Face Inspection

#### `describe_brep_faces(path: str | Path) -> list[FaceInfo]`

Native bridge:

```python
_native.describe_brep_faces(str(path))
```

Input:

- a BREP file path

Output:

- `FaceInfo(face_id, u_min, u_max, v_min, v_max)` records

#### `describe_shape_faces(shape: Any) -> list[FaceInfo]`

Native bridge:

```python
_native.describe_brep_bytes(shape_to_brep_bytes(shape))
```

Accepted Python inputs:

- a build123d / OCP shape
- a single face
- a non-empty sequence of faces

The helper first converts the shape-like input to BREP bytes in memory, then
calls the native face-description function.

### UV Sampling

#### `sample_shape_face_uniform_uv_grid(shape, face_id, u_count, v_count, margin=0.5) -> FaceUvSampleGroup`

Native bridge:

```python
_native.sample_brep_face_uniform_uv_grid(brep_bytes, grid)
```

Input:

- shape-like input accepted by `shape_to_brep_bytes`
- face id and explicit grid counts

Output:

- one `FaceUvSampleGroup`

#### `sample_shape_face_uniform_uv_grid_batch(shape, grids) -> FaceUvSampleGroupBatch`

Native bridge:

```python
_native.sample_brep_face_uniform_uv_grid_batch(brep_bytes, native_grids)
```

Input:

- shape-like input
- a sequence of grid requests

Accepted grid forms:

- native `_native.UniformUvGrid`
- Python objects with `face_id`, `u_count`, `v_count`, `margin`
- tuples of `(face_id, u_count, v_count, margin)`

Output:

- `FaceUvSampleGroupBatch`

#### `sample_shape_face_uniform_uv_tolerance_grid(shape, face_id, tolerance, margin=0.5) -> FaceUvSampleGroup`

Native bridge:

```python
_native.sample_brep_face_uniform_uv_tolerance_grid(brep_bytes, grid)
```

This is the tolerance-driven version of the sampler. It uses the face parameter
range and a target tolerance instead of explicit grid counts.

#### `sample_shape_face_uniform_uv_tolerance_grid_batch(shape, grids) -> FaceUvSampleGroupBatch`

Native bridge:

```python
_native.sample_brep_face_uniform_uv_tolerance_grid_batch(brep_bytes, native_grids)
```

Accepted grid forms:

- native `_native.UniformUvToleranceGrid`
- Python objects with `face_id`, `tolerance`, `margin`
- tuples of `(face_id, tolerance, margin)`

### Low-to-High Mapping

#### `map_shape_single_low_face_samples_to_high_faces_nearest(low_shape, high_shape, low_face_id, low_uv_samples, shared_context=None) -> MappingResultBatch`

Native bridge:

```python
_native.map_brep_single_low_face_samples_to_high_faces_nearest(...)
```

Input:

- `low_shape` and `high_shape`: shape-like inputs or face lists
- `low_face_id`: the source face id on the low shape
- `low_uv_samples`: any input accepted by `to_native_uv_coords`
- `shared_context`: optional native `MappingContext`

Accepted sample forms:

- a single UV pair
- a NumPy array with trailing dimension `2`
- any iterable of UV pairs

Output:

- `MappingResultBatch`

#### `map_shape_single_low_face_samples_to_high_faces_ray(...) -> MappingResultBatch`

Native bridge:

```python
_native.map_brep_single_low_face_samples_to_high_faces_ray(...)
```

This uses the low-face normal ray projection method instead of the nearest
surface projection.

#### `map_shape_single_low_face_samples_to_high_faces(...) -> MappingResultBatch`

This is the compatibility alias for the nearest-surface mapping path.

#### `map_shape_single_low_face_uv_grid_to_high_face_uv_grid_nearest(...) -> np.ndarray`

Native bridge:

```python
_native.map_brep_single_low_face_samples_to_high_faces_nearest(...)
```

The Python wrapper flattens the input UV grid, maps each sample, then reshapes
the result into a structured NumPy grid with fields:

- `high_face_id`
- `high_u`
- `high_v`

#### `map_shape_single_low_face_uv_grid_to_high_face_uv_grid_ray(...) -> np.ndarray`

Same flow as the nearest variant, but using ray projection.

#### `mapping_batch_to_numpy_structured_array(batch: Any) -> np.ndarray`

Native bridge:

```python
MappingResultBatch.from_python(batch).to_numpy_structured_array()
```

This is the direct export helper for flat mapping results.

### Surface Evaluation

#### `evaluate_shape_single_high_face_samples(high_shape, high_face_id, high_uv_samples, shared_context=None) -> SurfaceEvalResultBatch`

Native bridge:

```python
_native.evaluate_brep_single_high_face_samples(...)
```

Input:

- `high_shape`: shape-like input or face list
- `high_face_id`: face index in the high shape
- `high_uv_samples`: single UV, NumPy UV array, or iterable of UV pairs

Output:

- `SurfaceEvalResultBatch`

#### `evaluate_shape_multiple_high_face_samples(high_shape, mapping, shared_context=None) -> SurfaceEvalResultBatch`

Native bridge:

```python
_native.evaluate_brep_multiple_high_face_samples(...)
```

Input:

- `high_shape`: shape-like input or face list
- `mapping`: native or wrapped `MappingResultBatch`

Output:

- `SurfaceEvalResultBatch`

### Full Pipeline

#### `map_and_evaluate_shape_multiple_low_face_samples(low_shape, high_shape, low_face_samples, shared_context=None) -> MappedSampleBatch`

Native bridge:

```python
_native.map_and_evaluate_brep_multiple_low_face_samples(...)
```

Input:

- `low_shape` and `high_shape`: shape-like inputs or face lists
- `low_face_samples`: grouped sample input accepted by `normalize_face_uv_samples`

Output:

- `MappedSampleBatch`

## Key Datatypes

### Native datatypes

These are defined in C++ and exposed to Python through `cad_uv_map._native`.

- `UvCoord`: pure UV coordinate
- `Vec3`: 3D vector / point / normal container
- `FlatUvSample`: low-face id plus UV
- `FaceUvSampleGroup`: samples grouped by one face id
- `FaceUvSampleGroupBatch`: grouped low-face sample request
- `UniformUvGrid`: explicit UV grid description
- `UniformUvToleranceGrid`: tolerance-driven UV grid description
- `MappingContext`: execution policy and tolerance settings
- `MappingStatus`: per-sample mapping status
- `MappingResult`: one low-to-high correspondence
- `MappingResultBatch`: batch of mapping results
- `SurfaceEvalResult`: one high-face UV evaluation result
- `SurfaceEvalResultBatch`: batch of surface evaluation results
- `MappedSampleRecord`: combined sample + mapping + evaluation record
- `MappedSampleBatch`: batch of combined records
- `FaceInfo`: face metadata for inspection

### Wrapped Python datatypes

These live in `cad_uv_map.types` and mirror the native records with Python
construction, conversion, and NumPy helpers.

- `UvCoord`
- `Vec3`
- `IndexedUvCoord`
- `FlatUvSample`
- `IndexedFlatUvSample`
- `FaceUvSampleGroup`
- `IndexedFaceUvSampleGroup`
- `FaceUvSampleGroupBatch`
- `MappingResult`
- `IndexedMappingResult`
- `MappingResultBatch`
- `SurfaceEvalResult`
- `IndexedSurfaceEvalResult`
- `SurfaceEvalResultBatch`
- `MappedSampleRecord`
- `IndexedMappedSampleRecord`
- `MappedSampleBatch`

### Wrapper helpers

These are the conversion helpers used by the Python API:

- `shape_to_brep_bytes`
- `normalize_face_uv_samples`
- `to_native_uv_coords`
- `mapping_batch_to_structured_array`
- `mapping_batch_to_numpy_grid`

## Practical Rule

Use the Python wrapper layer when you want shape-like inputs, NumPy support, and
lightweight normalization. Use the native `_native` objects when you already
have the right batch type and want the thinnest bridge into C++.

