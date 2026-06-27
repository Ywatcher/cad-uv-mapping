from __future__ import annotations

from collections.abc import Mapping, Sequence

import numpy as np

from . import _native


def to_native_uv_coord(u: float, v: float):
    uv = _native.UvCoord()
    uv.u = float(u)
    uv.v = float(v)
    return uv


def to_native_indexed_uv_coord(index: int, u: float, v: float):
    indexed = _native.IndexedUvCoord()
    indexed.index = int(index)
    indexed.value = to_native_uv_coord(u, v)
    return indexed


def to_native_uv_coords(samples) -> list[object]:
    coords = []
    for sample in samples:
        if hasattr(sample, "u") and hasattr(sample, "v"):
            coords.append(to_native_uv_coord(sample.u, sample.v))
        elif hasattr(sample, "value") and hasattr(sample.value, "u") and hasattr(sample.value, "v"):
            coords.append(to_native_uv_coord(sample.value.u, sample.value.v))
        elif isinstance(sample, Sequence) and len(sample) == 2:
            u, v = sample
            coords.append(to_native_uv_coord(u, v))
        else:
            raise TypeError(f"unsupported low-face UV sample: {sample!r}")
    return coords


def to_native_indexed_uv_samples(samples: Sequence[object]):
    native_samples = []
    for index, sample in enumerate(samples):
        if hasattr(sample, "index") and hasattr(sample, "value"):
            native_samples.append(sample)
        elif hasattr(sample, "u") and hasattr(sample, "v"):
            native_samples.append(to_native_indexed_uv_coord(index, sample.u, sample.v))
        elif isinstance(sample, Sequence) and len(sample) == 2:
            u, v = sample
            native_samples.append(to_native_indexed_uv_coord(index, u, v))
        else:
            raise TypeError(f"unsupported grouped UV sample: {sample!r}")
    return native_samples


def to_native_face_uv_samples(face_id: int, samples: Sequence[object]):
    group = _native.FaceUvSamples()
    group.face_id = int(face_id)
    group.samples = to_native_indexed_uv_samples(samples)
    return group


def normalize_face_uv_samples(samples) -> object:
    """Convert flat or grouped Python UV samples into the native batch type."""
    if isinstance(samples, _native.FaceUvSampleBatch):
        return samples

    batch = _native.FaceUvSampleBatch()

    if isinstance(samples, Mapping):
        batch.faces = [
            to_native_face_uv_samples(int(face_id), list(face_samples))
            for face_id, face_samples in samples.items()
        ]
        return batch

    flat_samples = list(samples)
    if not flat_samples:
        return batch

    first = flat_samples[0]
    if hasattr(first, "face_id") or hasattr(first, "low_face_id"):
        grouped: dict[int, list[object]] = {}
        for sample_index, sample in enumerate(flat_samples):
            if hasattr(sample, "face_id") and hasattr(sample, "uv"):
                face_id = int(sample.face_id)
                u = float(sample.uv.u)
                v = float(sample.uv.v)
            elif hasattr(sample, "low_face_id") and hasattr(sample, "low_u") and hasattr(sample, "low_v"):
                face_id = int(sample.low_face_id)
                u = float(sample.low_u)
                v = float(sample.low_v)
            elif isinstance(sample, Sequence) and len(sample) == 3:
                face_id, u, v = sample
                face_id = int(face_id)
                u = float(u)
                v = float(v)
            else:
                raise TypeError(f"unsupported UV sample record: {sample!r}")
            grouped.setdefault(face_id, []).append(to_native_indexed_uv_coord(sample_index, u, v))
        batch.faces = [to_native_face_uv_samples(face_id, face_samples) for face_id, face_samples in grouped.items()]
        return batch

    if isinstance(first, Sequence) and len(first) == 2:
        grouped: dict[int, list[object]] = {}
        for face_id, face_samples in flat_samples:
            grouped.setdefault(int(face_id), []).extend(to_native_indexed_uv_samples(face_samples))
        batch.faces = [to_native_face_uv_samples(face_id, face_samples) for face_id, face_samples in grouped.items()]
        return batch

    if isinstance(first, Sequence) and len(first) == 3:
        grouped: dict[int, list[object]] = {}
        for sample_index, sample in enumerate(flat_samples):
            face_id, u, v = sample
            grouped.setdefault(int(face_id), []).append(to_native_indexed_uv_coord(sample_index, u, v))
        batch.faces = [to_native_face_uv_samples(face_id, face_samples) for face_id, face_samples in grouped.items()]
        return batch

    raise TypeError(f"unsupported UV sample collection: {samples!r}")


def mapping_batch_to_structured_array(batch):
    dtype = np.dtype(
        [
            ("index", np.int64),
            ("low_face_id", np.int32),
            ("low_u", np.float64),
            ("low_v", np.float64),
            ("high_face_id", np.int32),
            ("high_u", np.float64),
            ("high_v", np.float64),
            ("point_x", np.float64),
            ("point_y", np.float64),
            ("point_z", np.float64),
            ("distance", np.float64),
            ("status", np.int32),
        ]
    )
    mapped = np.empty(len(batch.results), dtype=dtype)
    for output_index, result in enumerate(batch.results):
        value = result.value
        mapped[output_index] = (
            int(result.index),
            int(value.low_face_id),
            float(value.low_u),
            float(value.low_v),
            int(value.high_face_id),
            float(value.high_u),
            float(value.high_v),
            float(value.point_x),
            float(value.point_y),
            float(value.point_z),
            float(value.distance),
            int(value.status.value if hasattr(value.status, "value") else value.status),
        )
    return mapped


def mapping_batch_to_numpy_grid(batch, grid_shape):
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
        mapped[row] = (
            int(value.high_face_id),
            float(value.high_u),
            float(value.high_v),
        )
    return mapped
