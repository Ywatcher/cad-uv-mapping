from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO
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


@dataclass(frozen=True)
class FaceInfo:
    face_id: int
    u_min: float
    u_max: float
    v_min: float
    v_max: float


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


def shape_to_brep_bytes(shape) -> bytes:
    """Serialize a build123d/OCP shape to BREP bytes in memory."""
    from build123d import export_brep

    buffer = BytesIO()
    if not export_brep(shape, buffer):
        raise RuntimeError("failed to export shape to BREP bytes")
    return buffer.getvalue()


def describe_shape_faces(shape) -> list[FaceInfo]:
    """Return face ids and native UV bounds for a build123d shape without writing files."""
    return _native_faces_to_python(_native.describe_brep_bytes(shape_to_brep_bytes(shape)))


def debug_print_brep_faces(path: str | Path) -> None:
    """Print native C++ face debug information for a BREP shape file."""
    _native.debug_print_brep_faces(str(path))


def debug_print_shape_faces(shape, label: str = "build123d shape") -> None:
    """Print native C++ face debug information for a build123d shape without writing files."""
    _native.debug_print_brep_bytes(shape_to_brep_bytes(shape), label)


def debug_print_shape_uv_sample_batch(shape, samples, label: str = "build123d shape + UV samples") -> None:
    """Print a native shape together with a normalized UV sample batch."""
    _native.debug_print_brep_uv_sample_batch(shape_to_brep_bytes(shape), normalize_face_uv_samples(samples), label)


def debug_print_shape_uv_samples(shape, samples, label: str = "build123d shape + UV samples") -> None:
    """Print a native shape together with UV samples accepted in multiple Python forms."""
    _native.debug_print_brep_uv_sample_batch(
        shape_to_brep_bytes(shape),
        normalize_face_uv_samples(samples),
        label,
    )


def map_shape_low_face_samples_to_high_faces(
    low_shape,
    high_shape,
    low_face_id: int,
    low_uv_samples,
    shared_context=None,
):
    """Map UV samples on one low face to nearest high faces."""
    return _native.map_brep_low_face_samples_to_high_faces(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        int(low_face_id),
        to_native_uv_coords(low_uv_samples),
        shared_context,
    )


def map_shape_low_face_uv_grid_to_high_face_uv_grid(
    low_shape,
    high_shape,
    low_face_id: int,
    low_uv_grid,
    shared_context=None,
):
    """Map a UV grid to a structured NumPy grid of high-face ids and UVs."""
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

    batch = _native.map_brep_low_face_samples_to_high_faces(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        int(low_face_id),
        to_native_uv_coords(flat_uvs),
        shared_context,
    )
    return mapping_batch_to_numpy_grid(batch, grid_shape)


def mapping_batch_to_numpy_structured_array(batch):
    """Convert a native MappingBatch into a flat structured NumPy array."""
    return mapping_batch_to_structured_array(batch)


def evaluate_shape_high_face_samples(
    high_shape,
    high_face_id: int,
    high_uv_samples,
    shared_context=None,
):
    """Evaluate point and normal data for UV samples on one high face."""
    return _native.evaluate_brep_high_face_samples(
        shape_to_brep_bytes(high_shape),
        int(high_face_id),
        to_native_uv_coords(high_uv_samples),
        shared_context,
    )


def evaluate_shape_mapped_high_uvs(high_shape, mapping, shared_context=None):
    """Evaluate a mapping batch against a high shape and return point/normal data."""
    return _native.evaluate_brep_mapped_high_uvs(shape_to_brep_bytes(high_shape), mapping, shared_context)


def map_and_evaluate_shape_samples(
    low_shape,
    high_shape,
    low_face_samples,
    shared_context=None,
):
    """Run the low-sample to mapping to surface-eval pipeline in one call."""
    return _native.map_and_evaluate_brep_samples(
        shape_to_brep_bytes(low_shape),
        shape_to_brep_bytes(high_shape),
        normalize_face_uv_samples(low_face_samples),
        shared_context,
    )
