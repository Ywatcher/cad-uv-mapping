from __future__ import annotations

from collections.abc import Mapping, Sequence

import numpy as np

from . import _native
from .types import (
    FaceUvSampleGroup,
    FaceUvSampleGroupBatch,
    FlatUvSample,
    IndexedFlatUvSample,
    IndexedMappingResult,
    IndexedSurfaceEvalResult,
    IndexedUvCoord,
    MappingResultBatch,
    SurfaceEvalResultBatch,
    UvCoord,
)


def to_native_uv_coord(u: float, v: float):
    return UvCoord(float(u), float(v)).to_native()


def to_native_indexed_uv_coord(index: int, u: float, v: float):
    return IndexedUvCoord(int(index), UvCoord(float(u), float(v))).to_native()


def to_native_uv_coords(samples) -> list[object]:
    coords = []
    for sample in samples:
        coords.append(UvCoord.from_python(sample).to_native())
    return coords


def to_native_indexed_uv_samples(samples: Sequence[object]):
    native_samples = []
    for index, sample in enumerate(samples):
        if hasattr(sample, "index") and hasattr(sample, "value"):
            native_samples.append(IndexedUvCoord.from_python(sample).to_native())
        else:
            native_samples.append(to_native_indexed_uv_coord(index, *UvCoord.from_python(sample).to_python()))
    return native_samples


def to_native_face_uv_samples(face_id: int, samples: Sequence[object]):
    return FaceUvSampleGroup.from_python({"face_id": int(face_id), "samples": list(samples)}).to_native()


def normalize_face_uv_samples(samples) -> object:
    """Convert flat or grouped Python UV samples into the wrapper batch type."""
    return FaceUvSampleGroupBatch.from_python(samples)


def mapping_batch_to_structured_array(batch):
    return MappingResultBatch.from_python(batch).to_numpy_structured_array()


def mapping_batch_to_numpy_grid(batch, grid_shape):
    batch = MappingResultBatch.from_python(batch)
    grid_shape = tuple(grid_shape)
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
        if hasattr(value, "high_uv"):
            high_u, high_v = float(value.high_uv.u), float(value.high_uv.v)
        else:
            high_u, high_v = float(value.high_u), float(value.high_v)
        mapped[row] = (
            int(value.high_face_id),
            high_u,
            high_v,
        )
    return mapped
