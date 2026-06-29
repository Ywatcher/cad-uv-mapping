from __future__ import annotations

from collections.abc import Mapping, Sequence
from typing import Any

import numpy as np

from . import _native
from .types import (
    FaceUvSampleGroup,
    FaceUvSampleGroupBatch,
    IndexedUvCoord,
    UvCoord,
)


def to_native_uv_coord(u: float, v: float) -> object:
    """Create a native `_native.UvCoord` from scalar UV values."""
    return UvCoord(float(u), float(v)).to_native()


def to_native_indexed_uv_coord(index: int, u: float, v: float) -> object:
    """Create a native `_native.IndexedUvCoord` from an index and UV values."""
    return IndexedUvCoord(int(index), UvCoord(float(u), float(v))).to_native()


def to_native_uv_coords(samples: Any) -> list[object]:
    """Convert UV samples into native `_native.UvCoord` objects.

    Accepts:
    - a single UV pair
    - a NumPy array with trailing dimension 2
    - any iterable of UV pairs
    """
    if isinstance(samples, np.ndarray):
        uv_array = np.asarray(samples, dtype=np.float64)
        if uv_array.ndim == 1:
            if uv_array.size != 2:
                raise TypeError(f"expected a UV pair or UV array, got shape {uv_array.shape!r}")
            return [UvCoord(float(uv_array[0]), float(uv_array[1])).to_native()]
        if uv_array.shape[-1] != 2:
            raise TypeError(f"expected a UV array with trailing dimension 2, got shape {uv_array.shape!r}")
        flat_uvs = uv_array.reshape(-1, 2)
        return [UvCoord(float(u), float(v)).to_native() for u, v in flat_uvs]

    coords: list[object] = []
    for sample in samples:
        coords.append(UvCoord.from_python(sample).to_native())
    return coords


def to_native_indexed_uv_samples(samples: Sequence[object]) -> list[object]:
    """Convert UV samples into native `_native.IndexedUvCoord` objects."""
    native_samples: list[object] = []
    for index, sample in enumerate(samples):
        if hasattr(sample, "index") and hasattr(sample, "value"):
            native_samples.append(IndexedUvCoord.from_python(sample).to_native())
        else:
            native_samples.append(to_native_indexed_uv_coord(index, *UvCoord.from_python(sample).to_python()))
    return native_samples


def to_native_face_uv_samples(face_id: int, samples: Sequence[object]) -> object:
    """Create a native `_native.FaceUvSampleGroup` from one face id and UV samples."""
    return FaceUvSampleGroup.from_python({"face_id": int(face_id), "samples": list(samples)}).to_native()


def normalize_face_uv_samples(samples: Any) -> object:
    """Convert flat or grouped Python UV samples into a wrapper batch type.

    Accepts:
    - flat low-face samples
    - grouped face sample batches
    - wrapper batch objects
    - native batch objects
    """
    return FaceUvSampleGroupBatch.from_python(samples)
