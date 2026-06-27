from __future__ import annotations

from dataclasses import dataclass
from collections.abc import Mapping, Sequence
from io import BytesIO
from pathlib import Path

import numpy as np

from . import _native

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


def _make_native_uv_coord(u: float, v: float):
    uv = _native.UvCoord()
    uv.u = float(u)
    uv.v = float(v)
    return uv


def _make_native_indexed_uv_coord(index: int, u: float, v: float):
    indexed = _native.IndexedUvCoord()
    indexed.index = int(index)
    indexed.value = _make_native_uv_coord(u, v)
    return indexed


def _make_native_uv_coords(samples) -> list[object]:
    coords = []
    for sample in samples:
        if hasattr(sample, "u") and hasattr(sample, "v"):
            coords.append(_make_native_uv_coord(sample.u, sample.v))
        elif hasattr(sample, "value") and hasattr(sample.value, "u") and hasattr(sample.value, "v"):
            coords.append(_make_native_uv_coord(sample.value.u, sample.value.v))
        elif isinstance(sample, Sequence) and len(sample) == 2:
            u, v = sample
            coords.append(_make_native_uv_coord(u, v))
        else:
            raise TypeError(f"unsupported low-face UV sample: {sample!r}")
    return coords


def _make_native_face_uv_samples(face_id: int, samples: Sequence[object]):
    group = _native.FaceUvSamples()
    group.face_id = int(face_id)
    native_samples = []
    for index, sample in enumerate(samples):
        if hasattr(sample, "index") and hasattr(sample, "value"):
            native_samples.append(sample)
        elif hasattr(sample, "u") and hasattr(sample, "v"):
            native_samples.append(_make_native_indexed_uv_coord(index, sample.u, sample.v))
        elif isinstance(sample, Sequence) and len(sample) == 2:
            u, v = sample
            native_samples.append(_make_native_indexed_uv_coord(index, u, v))
        else:
            raise TypeError(f"unsupported grouped UV sample: {sample!r}")
    group.samples = native_samples
    return group


def _sample_record_to_face_uv(sample) -> tuple[int, float, float]:
    if hasattr(sample, "face_id") and hasattr(sample, "uv"):
        return int(sample.face_id), float(sample.uv.u), float(sample.uv.v)
    if hasattr(sample, "low_face_id") and hasattr(sample, "low_u") and hasattr(sample, "low_v"):
        return int(sample.low_face_id), float(sample.low_u), float(sample.low_v)
    if isinstance(sample, Sequence) and len(sample) == 3:
        face_id, u, v = sample
        return int(face_id), float(u), float(v)
    raise TypeError(f"unsupported UV sample record: {sample!r}")


def normalize_face_uv_samples(samples) -> object:
    """Convert flat or grouped Python UV samples into the same native batch type."""
    if isinstance(samples, _native.FaceUvSampleBatch):
        return samples

    batch = _native.FaceUvSampleBatch()

    if isinstance(samples, Mapping):
        face_groups = []
        items = samples.items()
        for face_id, face_samples in items:
            face_groups.append(_make_native_face_uv_samples(int(face_id), list(face_samples)))
        batch.faces = face_groups
        return batch

    flat_samples = list(samples)
    if not flat_samples:
        return batch

    first = flat_samples[0]
    if hasattr(first, "face_id") or hasattr(first, "low_face_id"):
        grouped: dict[int, list[object]] = {}
        for sample_index, sample in enumerate(flat_samples):
            face_id, u, v = _sample_record_to_face_uv(sample)
            grouped.setdefault(face_id, []).append(_make_native_indexed_uv_coord(sample_index, u, v))
        batch.faces = [
            _make_native_face_uv_samples(face_id, face_samples)
            for face_id, face_samples in grouped.items()
        ]
        return batch

    if isinstance(first, Sequence) and len(first) == 2:
        grouped: dict[int, list[object]] = {}
        for face_id, face_samples in flat_samples:
            indexed_uvs = []
            for index, sample in enumerate(face_samples):
                if hasattr(sample, "index") and hasattr(sample, "value"):
                    indexed_uvs.append(sample)
                elif hasattr(sample, "u") and hasattr(sample, "v"):
                    indexed_uvs.append(_make_native_indexed_uv_coord(index, sample.u, sample.v))
                elif isinstance(sample, Sequence) and len(sample) == 2:
                    u, v = sample
                    indexed_uvs.append(_make_native_indexed_uv_coord(index, u, v))
                else:
                    raise TypeError(f"unsupported grouped UV sample: {sample!r}")
            grouped.setdefault(int(face_id), []).extend(indexed_uvs)
        batch.faces = [
            _make_native_face_uv_samples(face_id, face_samples)
            for face_id, face_samples in grouped.items()
        ]
        return batch

    if isinstance(first, Sequence) and len(first) == 3:
        grouped: dict[int, list[object]] = {}
        for sample_index, sample in enumerate(flat_samples):
            face_id, u, v = _sample_record_to_face_uv(sample)
            grouped.setdefault(face_id, []).append(_make_native_indexed_uv_coord(sample_index, u, v))
        batch.faces = [
            _make_native_face_uv_samples(face_id, face_samples)
            for face_id, face_samples in grouped.items()
        ]
        return batch

    raise TypeError(f"unsupported UV sample collection: {samples!r}")


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
        _make_native_uv_coords(low_uv_samples),
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
        _make_native_uv_coords(flat_uvs),
        shared_context,
    )

    dtype = np.dtype(
        [
            ("high_face_id", np.int32),
            ("high_u", np.float64),
            ("high_v", np.float64),
        ]
    )
    mapped = np.empty(grid_shape, dtype=dtype)

    for flat_index, result in enumerate(batch.results):
        row = np.unravel_index(flat_index, grid_shape) if grid_shape else ()
        value = result.value
        mapped[row] = (
            int(value.high_face_id),
            float(value.high_u),
            float(value.high_v),
        )

    return mapped
