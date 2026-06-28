from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO
from collections.abc import Sequence
from typing import Any
from pathlib import Path

import numpy as np

from . import _native
from .conversions import (
    mapping_batch_to_numpy_grid,
    mapping_batch_to_structured_array,
    normalize_face_uv_samples,
    to_native_uv_coords,
)

MappingContext = _native.MappingContext
UniformUvGrid = _native.UniformUvGrid
UniformUvToleranceGrid = _native.UniformUvToleranceGrid


@dataclass(frozen=True)
class FaceInfo:
    face_id: int
    u_min: float
    u_max: float
    v_min: float
    v_max: float


def _is_face_like(value) -> bool:
    return hasattr(value, "wrapped") or value.__class__.__name__.endswith("Face")


def _is_face_sequence(value) -> bool:
    return isinstance(value, Sequence) and not isinstance(value, (str, bytes, bytearray)) and bool(value) and all(
        _is_face_like(face) for face in value
    )


def _coerce_shape_like(value):
    if _is_face_sequence(value):
        from build123d import Compound, Face

        faces = [face if hasattr(face, "wrapped") else Face(face) for face in value]
        return Compound(faces)

    if _is_face_like(value) and not hasattr(value, "faces"):
        from build123d import Face

        return Face(value)

    return value


def describe_brep_faces(path: str | Path) -> list[FaceInfo]:
    """Return face ids and native UV bounds for a BREP shape file."""
    native_faces = _native.describe_brep_faces(str(path))
    return [
        FaceInfo(
            face_id=int(face.face_id),
            u_min=float(face.u_min),
            u_max=float(face.u_max),
            v_min=float(face.v_min),
            v_max=float(face.v_max),
        )
        for face in native_faces
    ]


def _native_faces_to_python(native_faces) -> list[FaceInfo]:
    return [
        FaceInfo(
            face_id=int(face.face_id),
            u_min=float(face.u_min),
            u_max=float(face.u_max),
            v_min=float(face.v_min),
            v_max=float(face.v_max),
        )
        for face in native_faces
    ]


def shape_to_brep_bytes(shape: Any) -> bytes:
    """Serialize a build123d/OCP shape or face list to BREP bytes in memory.

    Accepts:
    - a build123d shape / OCP shape with `.faces()`
    - a non-empty sequence of build123d faces or OCP faces
    """
    from build123d import export_brep

    shape = _coerce_shape_like(shape)
    buffer = BytesIO()
    if not export_brep(shape, buffer):
        raise RuntimeError("failed to export shape to BREP bytes")
    return buffer.getvalue()


def describe_shape_faces(shape: Any) -> list[FaceInfo]:
    """Return face ids and native UV bounds for a build123d shape or face list.

    Accepts the same shape-like inputs as `shape_to_brep_bytes`.
    """
    return _native_faces_to_python(_native.describe_brep_bytes(shape_to_brep_bytes(shape)))


def debug_print_brep_faces(path: str | Path) -> None:
    """Print native C++ face debug information for a BREP shape file."""
    _native.debug_print_brep_faces(str(path))


def debug_print_shape_faces(shape: Any, label: str = "build123d shape") -> None:
    """Print native C++ face debug information for a shape or face list without writing files.

    Accepts the same shape-like inputs as `shape_to_brep_bytes`.
    """
    _native.debug_print_brep_bytes(shape_to_brep_bytes(shape), label)


def debug_print_shape_uv_sample_batch(shape: Any, samples: Any, label: str = "build123d shape + UV samples") -> None:
    """Print a shape or face list together with a native UV sample batch.

    `samples` may be a flat UV sample batch, grouped face samples, or any input
    accepted by `normalize_face_uv_samples`.
    """
    _native.debug_print_brep_uv_sample_batch(
        shape_to_brep_bytes(shape),
        normalize_face_uv_samples(samples).to_native(),
        label,
    )


def debug_print_shape_uv_samples(shape: Any, samples: Any, label: str = "build123d shape + UV samples") -> None:
    """Print a shape or face list together with UV samples accepted in multiple Python forms.

    `samples` may be any input accepted by `normalize_face_uv_samples`.
    """
    _native.debug_print_brep_uv_sample_batch(
        shape_to_brep_bytes(shape),
        normalize_face_uv_samples(samples).to_native(),
        label,
    )


def sample_shape_face_uniform_uv_grid(
    shape: Any,
    face_id: int,
    u_count: int,
    v_count: int,
    margin: float = 0.5,
) -> object:
    """Generate a grouped uniform UV sample grid for one face from a shape or face list.

    Returns a native `FaceUvSampleGroup`.
    """
    grid = _native.UniformUvGrid()
    grid.face_id = int(face_id)
    grid.u_count = int(u_count)
    grid.v_count = int(v_count)
    grid.margin = float(margin)
    return _native.sample_brep_face_uniform_uv_grid(shape_to_brep_bytes(shape), grid)


def sample_shape_face_uniform_uv_grid_batch(shape: Any, grids: Any) -> object:
    """Generate grouped uniform UV sample grids for multiple faces from a shape or face list.

    Returns a native `FaceUvSampleGroupBatch`.
    """
    native_grids = []
    for grid in grids:
        if isinstance(grid, _native.UniformUvGrid):
            native_grids.append(grid)
            continue
        native_grid = _native.UniformUvGrid()
        if hasattr(grid, "face_id"):
            native_grid.face_id = int(grid.face_id)
            native_grid.u_count = int(grid.u_count)
            native_grid.v_count = int(grid.v_count)
            native_grid.margin = float(grid.margin)
        else:
            face_id, u_count, v_count, margin = grid
            native_grid.face_id = int(face_id)
            native_grid.u_count = int(u_count)
            native_grid.v_count = int(v_count)
            native_grid.margin = float(margin)
        native_grids.append(native_grid)
    return _native.sample_brep_face_uniform_uv_grid_batch(shape_to_brep_bytes(shape), native_grids)


def sample_shape_face_uniform_uv_tolerance_grid(
    shape: Any,
    face_id: int,
    tolerance: float,
    margin: float = 0.5,
) -> object:
    """Generate a grouped uniform UV sample grid for one face from a shape or face list.

    Returns a native `FaceUvSampleGroup`.
    """
    grid = _native.UniformUvToleranceGrid()
    grid.face_id = int(face_id)
    grid.tolerance = float(tolerance)
    grid.margin = float(margin)
    return _native.sample_brep_face_uniform_uv_tolerance_grid(shape_to_brep_bytes(shape), grid)


def sample_shape_face_uniform_uv_tolerance_grid_batch(shape: Any, grids: Any) -> object:
    """Generate grouped tolerance-driven UV sample grids for multiple faces from a shape or face list.

    Returns a native `FaceUvSampleGroupBatch`.
    """
    native_grids = []
    for grid in grids:
        if isinstance(grid, _native.UniformUvToleranceGrid):
            native_grids.append(grid)
            continue
        native_grid = _native.UniformUvToleranceGrid()
        if hasattr(grid, "face_id"):
            native_grid.face_id = int(grid.face_id)
            native_grid.tolerance = float(grid.tolerance)
            native_grid.margin = float(grid.margin)
        else:
            face_id, tolerance, margin = grid
            native_grid.face_id = int(face_id)
            native_grid.tolerance = float(tolerance)
            native_grid.margin = float(margin)
        native_grids.append(native_grid)
    return _native.sample_brep_face_uniform_uv_tolerance_grid_batch(shape_to_brep_bytes(shape), native_grids)


def map_shape_single_low_face_samples_to_high_faces_nearest(
    low_shape: Any,
    high_shape: Any,
    low_face_id: int,
    low_uv_samples: Any,
    shared_context: Any = None,
) -> object:
    """Map UV samples on one low face to nearest high faces from shape or face-list inputs.

    `low_uv_samples` may be a Python sequence, NumPy UV array, or native UV sample
    container accepted by `to_native_uv_coords`.
    Returns a native `MappingResultBatch`.
    """
    return _native.map_brep_single_low_face_samples_to_high_faces_nearest(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        int(low_face_id),
        to_native_uv_coords(low_uv_samples),
        shared_context,
    )


def map_shape_single_low_face_samples_to_high_faces_ray(
    low_shape: Any,
    high_shape: Any,
    low_face_id: int,
    low_uv_samples: Any,
    shared_context: Any = None,
) -> object:
    """Map UV samples on one low face along the low-face normal rays from shape or face-list inputs.

    `low_uv_samples` may be a Python sequence, NumPy UV array, or native UV sample
    container accepted by `to_native_uv_coords`.
    Returns a native `MappingResultBatch`.
    """
    return _native.map_brep_single_low_face_samples_to_high_faces_ray(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        int(low_face_id),
        to_native_uv_coords(low_uv_samples),
        shared_context,
    )


def map_shape_single_low_face_samples_to_high_faces(
    low_shape: Any,
    high_shape: Any,
    low_face_id: int,
    low_uv_samples: Any,
    shared_context: Any = None,
) -> object:
    """Compatibility alias for nearest-surface mapping.

    Returns a native `MappingResultBatch`.
    """
    return map_shape_single_low_face_samples_to_high_faces_nearest(
        low_shape,
        high_shape,
        low_face_id,
        low_uv_samples,
        shared_context,
    )


def map_shape_single_low_face_uv_grid_to_high_face_uv_grid_nearest(
    low_shape: Any,
    high_shape: Any,
    low_face_id: int,
    low_uv_grid: Any,
    shared_context: Any = None,
) -> np.ndarray:
    """Map a UV grid to a structured NumPy grid of high-face ids and UVs from shape or face-list inputs.

    `low_uv_grid` may be a NumPy array with trailing dimension 2 or any nested
    Python sequence of UV pairs. Returns a NumPy structured array.
    """
    low_uv_grid_array = np.asarray(low_uv_grid, dtype=np.float64)
    if low_uv_grid_array.ndim == 1:
        if low_uv_grid_array.shape[0] != 2:
            raise ValueError("low_uv_grid must end with a UV pair")
        grid_shape = ()
        flat_uvs = [tuple(low_uv_grid_array.tolist())]
    else:
        if low_uv_grid_array.shape[-1] != 2:
            raise ValueError("low_uv_grid must have a trailing dimension of size 2")
        grid_shape = low_uv_grid_array.shape[:-1]
        flat_uvs = low_uv_grid_array.reshape(-1, 2).tolist()

    batch = _native.map_brep_single_low_face_samples_to_high_faces_nearest(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        int(low_face_id),
        to_native_uv_coords(flat_uvs),
        shared_context,
    )
    return mapping_batch_to_numpy_grid(batch, grid_shape)


def map_shape_single_low_face_uv_grid_to_high_face_uv_grid_ray(
    low_shape: Any,
    high_shape: Any,
    low_face_id: int,
    low_uv_grid: Any,
    shared_context: Any = None,
) -> np.ndarray:
    """Map a UV grid with the ray method to a structured NumPy grid from shape or face-list inputs.

    `low_uv_grid` may be a NumPy array with trailing dimension 2 or any nested
    Python sequence of UV pairs. Returns a NumPy structured array.
    """
    low_uv_grid_array = np.asarray(low_uv_grid, dtype=np.float64)
    if low_uv_grid_array.ndim == 1:
        if low_uv_grid_array.shape[0] != 2:
            raise ValueError("low_uv_grid must end with a UV pair")
        grid_shape = ()
        flat_uvs = [tuple(low_uv_grid_array.tolist())]
    else:
        if low_uv_grid_array.shape[-1] != 2:
            raise ValueError("low_uv_grid must have a trailing dimension of size 2")
        grid_shape = low_uv_grid_array.shape[:-1]
        flat_uvs = low_uv_grid_array.reshape(-1, 2).tolist()

    batch = _native.map_brep_single_low_face_samples_to_high_faces_ray(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        int(low_face_id),
        to_native_uv_coords(flat_uvs),
        shared_context,
    )
    return mapping_batch_to_numpy_grid(batch, grid_shape)


def map_shape_single_low_face_uv_grid_to_high_face_uv_grid(
    low_shape: Any,
    high_shape: Any,
    low_face_id: int,
    low_uv_grid: Any,
    shared_context: Any = None,
) -> np.ndarray:
    """Compatibility alias for nearest-surface UV grid mapping.

    Returns a NumPy structured array.
    """
    return map_shape_single_low_face_uv_grid_to_high_face_uv_grid_nearest(
        low_shape,
        high_shape,
        low_face_id,
        low_uv_grid,
        shared_context,
    )


def mapping_batch_to_numpy_structured_array(batch: Any) -> np.ndarray:
    """Convert a `MappingResultBatch` into a flat structured NumPy array.

    `batch` may be a native `_native.MappingResultBatch` or a Python wrapper
    `cad_uv_map.types.MappingResultBatch`.
    """
    return mapping_batch_to_structured_array(batch)


def evaluate_shape_single_high_face_samples(
    high_shape: Any,
    high_face_id: int,
    high_uv_samples: Any,
    shared_context: Any = None,
) -> object:
    """Evaluate point and normal data for one high face.

    Accepts a single UV pair, a NumPy array with trailing dimension 2, or any
    iterable of UV pairs.
    Returns a native `SurfaceEvalResultBatch`.
    """
    return _native.evaluate_brep_single_high_face_samples(
        shape_to_brep_bytes(high_shape),
        int(high_face_id),
        to_native_uv_coords(high_uv_samples),
        shared_context,
    )


def evaluate_shape_multiple_high_face_samples(high_shape: Any, mapping: Any, shared_context: Any = None) -> object:
    """Evaluate a mapping batch against a high shape or face list and return point/normal data.

    `mapping` may be a native `_native.MappingResultBatch` or a Python wrapper
    `cad_uv_map.types.MappingResultBatch`.
    Returns a native `SurfaceEvalResultBatch`.
    """
    return _native.evaluate_brep_multiple_high_face_samples(shape_to_brep_bytes(high_shape), mapping, shared_context)


def map_and_evaluate_shape_multiple_low_face_samples(
    low_shape: Any,
    high_shape: Any,
    low_face_samples: Any,
    shared_context: Any = None,
) -> object:
    """Run the low-sample to mapping to surface-eval pipeline in one call from shape or face-list inputs.

    `low_face_samples` may be any input accepted by `normalize_face_uv_samples`.
    Returns a native `MappedSampleBatch`.
    """
    return _native.map_and_evaluate_brep_multiple_low_face_samples(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        normalize_face_uv_samples(low_face_samples).to_native(),
        shared_context,
    )
