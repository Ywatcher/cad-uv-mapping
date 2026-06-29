from __future__ import annotations

from dataclasses import dataclass, field
from collections.abc import Mapping, Sequence
from typing import Any

import numpy as np

from . import _native


def _is_sequence(value: Any) -> bool:
    return isinstance(value, Sequence) and not isinstance(value, (str, bytes, bytearray))


def _field(value: Any, *names: str):
    for name in names:
        if hasattr(value, name):
            return getattr(value, name)
        if isinstance(value, Mapping) and name in value:
            return value[name]
        dtype = getattr(value, "dtype", None)
        if dtype is not None and getattr(dtype, "names", None) and name in dtype.names:
            return value[name]
    raise AttributeError(f"missing fields {names!r} on {value!r}")


def _native_status_value(status: Any):
    return getattr(status, "value", status)


def _to_native_status(status: Any):
    if hasattr(status, "value"):
        return status
    if isinstance(status, str):
        key = status
        if hasattr(_native.MappingStatus, key):
            return getattr(_native.MappingStatus, key)
        if hasattr(_native.MappingStatus, key.lower()):
            return getattr(_native.MappingStatus, key.lower())
    return status


def _stack_uv(values: Sequence[UvCoord]) -> np.ndarray:
    if not values:
        return np.empty((0, 2), dtype=np.float64)
    return np.asarray([(float(value.u), float(value.v)) for value in values], dtype=np.float64)


def _stack_vec3(values: Sequence[Vec3]) -> np.ndarray:
    if not values:
        return np.empty((0, 3), dtype=np.float64)
    return np.asarray([(float(value.x), float(value.y), float(value.z)) for value in values], dtype=np.float64)


def _status_array(values: Sequence[Any]) -> np.ndarray:
    return np.asarray([int(_native_status_value(value)) for value in values], dtype=np.int32)


@dataclass(frozen=True, slots=True)
class UvCoord:
    u: float
    v: float

    @classmethod
    def from_python(cls, value: Any) -> "UvCoord":
        if isinstance(value, cls):
            return value
        if hasattr(value, "value") and hasattr(value.value, "u") and hasattr(value.value, "v"):
            return cls.from_python(value.value)
        if hasattr(value, "u") and hasattr(value, "v"):
            return cls(float(value.u), float(value.v))
        if isinstance(value, np.ndarray):
            array = np.asarray(value, dtype=np.float64).reshape(-1)
            if array.size != 2:
                raise TypeError(f"expected a UV pair, got shape {value.shape!r}")
            return cls(float(array[0]), float(array[1]))
        if isinstance(value, Mapping):
            return cls(float(value["u"]), float(value["v"]))
        if _is_sequence(value) and len(value) == 2:
            u, v = value
            return cls(float(u), float(v))
        raise TypeError(f"unsupported UV coordinate: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "UvCoord":
        return cls(float(value.u), float(value.v))

    def to_native(self):
        uv = _native.UvCoord()
        uv.u = float(self.u)
        uv.v = float(self.v)
        return uv

    def to_python(self):
        return (float(self.u), float(self.v))

    def to_numpy(self) -> np.ndarray:
        return np.asarray([[float(self.u), float(self.v)]], dtype=np.float64)

    def __iter__(self):
        yield self.u
        yield self.v

    def __len__(self):
        return 2


@dataclass(frozen=True, slots=True)
class Vec3:
    x: float
    y: float
    z: float

    @classmethod
    def from_python(cls, value: Any) -> "Vec3":
        if isinstance(value, cls):
            return value
        if hasattr(value, "x") and hasattr(value, "y") and hasattr(value, "z"):
            return cls(float(value.x), float(value.y), float(value.z))
        if isinstance(value, np.ndarray):
            array = np.asarray(value, dtype=np.float64).reshape(-1)
            if array.size != 3:
                raise TypeError(f"expected a Vec3 triple, got shape {value.shape!r}")
            return cls(float(array[0]), float(array[1]), float(array[2]))
        if isinstance(value, Mapping):
            return cls(float(value["x"]), float(value["y"]), float(value["z"]))
        if _is_sequence(value) and len(value) == 3:
            x, y, z = value
            return cls(float(x), float(y), float(z))
        raise TypeError(f"unsupported Vec3 value: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "Vec3":
        return cls(float(value.x), float(value.y), float(value.z))

    def to_native(self):
        vec = _native.Vec3()
        vec.x = float(self.x)
        vec.y = float(self.y)
        vec.z = float(self.z)
        return vec

    def to_python(self):
        return (float(self.x), float(self.y), float(self.z))

    def to_numpy(self) -> np.ndarray:
        return np.asarray([[float(self.x), float(self.y), float(self.z)]], dtype=np.float64)

    def __iter__(self):
        yield self.x
        yield self.y
        yield self.z

    def __len__(self):
        return 3


@dataclass(frozen=True, slots=True)
class IndexedUvCoord:
    index: int
    value: UvCoord

    @classmethod
    def from_python(cls, value: Any) -> "IndexedUvCoord":
        if isinstance(value, cls):
            return value
        if hasattr(value, "index") and hasattr(value, "value"):
            return cls(int(value.index), UvCoord.from_python(value.value))
        if isinstance(value, Mapping):
            return cls(int(value["index"]), UvCoord.from_python(value["value"]))
        if _is_sequence(value) and len(value) == 2:
            index, uv = value
            return cls(int(index), UvCoord.from_python(uv))
        raise TypeError(f"unsupported indexed UV coordinate: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "IndexedUvCoord":
        return cls(int(value.index), UvCoord.from_native(value.value))

    def to_native(self):
        native = _native.IndexedUvCoord()
        native.index = int(self.index)
        native.value = self.value.to_native()
        return native

    def to_python(self):
        return {"index": int(self.index), "value": self.value.to_python()}

    def to_numpy_index_uv(self) -> tuple[np.ndarray, np.ndarray]:
        return (
            np.asarray([int(self.index)], dtype=np.int64),
            self.value.to_numpy(),
        )

    def to_numpy_indexed_uv(self) -> np.ndarray:
        return np.asarray([[int(self.index), float(self.value.u), float(self.value.v)]], dtype=np.float64)


@dataclass(frozen=True, slots=True)
class FlatUvSample:
    face_id: int
    uv: UvCoord

    @classmethod
    def from_python(cls, value: Any) -> "FlatUvSample":
        if isinstance(value, cls):
            return value
        if hasattr(value, "face_id") and hasattr(value, "uv"):
            return cls(int(value.face_id), UvCoord.from_python(value.uv))
        if hasattr(value, "low_face_id") and hasattr(value, "low_uv"):
            return cls(int(value.low_face_id), UvCoord.from_python(value.low_uv))
        if isinstance(value, Mapping):
            face_id = value.get("face_id", value.get("low_face_id"))
            uv = value.get("uv", value.get("low_uv"))
            if face_id is None or uv is None:
                raise TypeError(f"unsupported flat UV sample mapping: {value!r}")
            return cls(int(face_id), UvCoord.from_python(uv))
        if isinstance(value, np.ndarray):
            array = np.asarray(value, dtype=np.float64).reshape(-1)
            if array.size != 3:
                raise TypeError(f"expected face-id + UV triple, got shape {value.shape!r}")
            return cls(int(array[0]), UvCoord(float(array[1]), float(array[2])))
        if _is_sequence(value) and len(value) == 3:
            face_id, u, v = value
            return cls(int(face_id), UvCoord(float(u), float(v)))
        raise TypeError(f"unsupported flat UV sample: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "FlatUvSample":
        return cls(int(value.face_id), UvCoord.from_native(value.uv))

    def to_native(self):
        native = _native.FlatUvSample()
        native.face_id = int(self.face_id)
        native.uv = self.uv.to_native()
        return native

    def to_python(self):
        return {"face_id": int(self.face_id), "uv": self.uv.to_python()}

    def to_numpy_uv(self) -> np.ndarray:
        return self.uv.to_numpy()

    def to_numpy_face_uv(self) -> np.ndarray:
        return np.asarray([[float(self.face_id), float(self.uv.u), float(self.uv.v)]], dtype=np.float64)


@dataclass(frozen=True, slots=True)
class IndexedFlatUvSample:
    index: int
    value: FlatUvSample

    @classmethod
    def from_python(cls, value: Any) -> "IndexedFlatUvSample":
        if isinstance(value, cls):
            return value
        if hasattr(value, "index") and hasattr(value, "value"):
            return cls(int(value.index), FlatUvSample.from_python(value.value))
        if isinstance(value, Mapping):
            return cls(int(value["index"]), FlatUvSample.from_python(value["value"]))
        if _is_sequence(value) and len(value) == 2:
            index, sample = value
            return cls(int(index), FlatUvSample.from_python(sample))
        raise TypeError(f"unsupported indexed flat UV sample: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "IndexedFlatUvSample":
        return cls(int(value.index), FlatUvSample.from_native(value.value))

    def to_native(self):
        native = _native.IndexedFlatUvSample()
        native.index = int(self.index)
        native.value = self.value.to_native()
        return native

    def to_python(self):
        return {"index": int(self.index), "value": self.value.to_python()}


@dataclass(frozen=True, slots=True)
class FaceUvSampleGroup:
    face_id: int
    samples: list[IndexedUvCoord] = field(default_factory=list)

    @classmethod
    def from_python(cls, value: Any) -> "FaceUvSampleGroup":
        if isinstance(value, cls):
            return value
        if hasattr(value, "face_id") and hasattr(value, "samples"):
            samples = list(value.samples)
            if samples and hasattr(samples[0], "index") and hasattr(samples[0], "value"):
                indexed_samples = [IndexedUvCoord.from_python(sample) for sample in samples]
            else:
                indexed_samples = [IndexedUvCoord(index, UvCoord.from_python(sample)) for index, sample in enumerate(samples)]
            return cls(int(value.face_id), indexed_samples)
        if isinstance(value, Mapping):
            samples = list(value["samples"])
            if samples and hasattr(samples[0], "index") and hasattr(samples[0], "value"):
                indexed_samples = [IndexedUvCoord.from_python(sample) for sample in samples]
            else:
                indexed_samples = [IndexedUvCoord(index, UvCoord.from_python(sample)) for index, sample in enumerate(samples)]
            return cls(int(value["face_id"]), indexed_samples)
        if _is_sequence(value) and len(value) == 2:
            face_id, samples = value
            samples = list(samples)
            if samples and hasattr(samples[0], "index") and hasattr(samples[0], "value"):
                indexed_samples = [IndexedUvCoord.from_python(sample) for sample in samples]
            else:
                indexed_samples = [IndexedUvCoord(index, UvCoord.from_python(sample)) for index, sample in enumerate(samples)]
            return cls(int(face_id), indexed_samples)
        raise TypeError(f"unsupported face UV sample group: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "FaceUvSampleGroup":
        return cls(int(value.face_id), [IndexedUvCoord.from_native(sample) for sample in value.samples])

    def to_native(self):
        native = _native.FaceUvSampleGroup()
        native.face_id = int(self.face_id)
        native.samples = [sample.to_native() for sample in self.samples]
        return native

    def to_python(self):
        return {"face_id": int(self.face_id), "samples": [sample.to_python() for sample in self.samples]}

    def to_numpy_uv_array(self) -> np.ndarray:
        return _stack_uv([sample.value for sample in self.samples])

    def to_numpy_index_array(self) -> np.ndarray:
        return np.asarray([int(sample.index) for sample in self.samples], dtype=np.int64)

    def to_numpy_index_uv_arrays(self) -> tuple[np.ndarray, np.ndarray]:
        return self.to_numpy_index_array(), self.to_numpy_uv_array()

    def to_numpy_indexed_uv_array(self) -> np.ndarray:
        if not self.samples:
            return np.empty((0, 3), dtype=np.float64)
        return np.asarray(
            [(float(sample.index), float(sample.value.u), float(sample.value.v)) for sample in self.samples],
            dtype=np.float64,
        )

    def __len__(self):
        return len(self.samples)

    def __iter__(self):
        return iter(self.samples)


@dataclass(frozen=True, slots=True)
class IndexedFaceUvSampleGroup:
    index: int
    value: FaceUvSampleGroup

    @classmethod
    def from_python(cls, value: Any) -> "IndexedFaceUvSampleGroup":
        if isinstance(value, cls):
            return value
        if hasattr(value, "index") and hasattr(value, "value"):
            return cls(int(value.index), FaceUvSampleGroup.from_python(value.value))
        if isinstance(value, Mapping):
            return cls(int(value["index"]), FaceUvSampleGroup.from_python(value["value"]))
        if _is_sequence(value) and len(value) == 2:
            index, group = value
            return cls(int(index), FaceUvSampleGroup.from_python(group))
        raise TypeError(f"unsupported indexed face UV sample group: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "IndexedFaceUvSampleGroup":
        return cls(int(value.index), FaceUvSampleGroup.from_native(value.value))

    def to_native(self):
        native = _native.IndexedFaceUvSampleGroup()
        native.index = int(self.index)
        native.value = self.value.to_native()
        return native

    def to_python(self):
        return {"index": int(self.index), "value": self.value.to_python()}


@dataclass(slots=True)
class FaceUvSampleGroupBatch:
    faces: list[FaceUvSampleGroup] = field(default_factory=list)

    @classmethod
    def from_python(cls, value: Any) -> "FaceUvSampleGroupBatch":
        if isinstance(value, cls):
            return value
        if hasattr(value, "faces"):
            return cls([FaceUvSampleGroup.from_python(group) for group in value.faces])
        if isinstance(value, Mapping):
            return cls(
                [
                    FaceUvSampleGroup.from_python({"face_id": int(face_id), "samples": face_samples})
                    for face_id, face_samples in value.items()
                ]
            )

        flat_samples = list(value)
        if not flat_samples:
            return cls()

        first = flat_samples[0]
        if hasattr(first, "samples"):
            return cls([FaceUvSampleGroup.from_python(group) for group in flat_samples])

        if hasattr(first, "face_id") or hasattr(first, "low_face_id"):
            grouped: dict[int, list[Any]] = {}
            order: list[int] = []
            for sample_index, sample in enumerate(flat_samples):
                flat_sample = FlatUvSample.from_python(sample)
                if flat_sample.face_id not in grouped:
                    grouped[flat_sample.face_id] = []
                    order.append(flat_sample.face_id)
                grouped[flat_sample.face_id].append(IndexedUvCoord(sample_index, flat_sample.uv))
            return cls([FaceUvSampleGroup(face_id, grouped[face_id]) for face_id in order])

        if _is_sequence(first) and len(first) == 2:
            groups: list[FaceUvSampleGroup] = []
            for face_id, face_samples in flat_samples:
                groups.append(FaceUvSampleGroup(int(face_id), [IndexedUvCoord.from_python(sample) for sample in face_samples]))
            return cls(groups)

        if _is_sequence(first) and len(first) == 3:
            grouped: dict[int, list[IndexedUvCoord]] = {}
            order: list[int] = []
            for sample_index, sample in enumerate(flat_samples):
                flat_sample = FlatUvSample.from_python(sample)
                if flat_sample.face_id not in grouped:
                    grouped[flat_sample.face_id] = []
                    order.append(flat_sample.face_id)
                grouped[flat_sample.face_id].append(IndexedUvCoord(sample_index, flat_sample.uv))
            return cls([FaceUvSampleGroup(face_id, grouped[face_id]) for face_id in order])

        raise TypeError(f"unsupported face UV sample batch: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "FaceUvSampleGroupBatch":
        return cls([FaceUvSampleGroup.from_native(group) for group in value.faces])

    def to_native(self):
        """Convert to native `_native.FaceUvSampleGroupBatch`."""
        native = _native.FaceUvSampleGroupBatch()
        native.faces = [group.to_native() for group in self.faces]
        return native

    def to_python(self):
        return [group.to_python() for group in self.faces]

    def to_numpy_group_list(self) -> list[np.ndarray]:
        return self.to_native().to_numpy_group_list()

    def to_numpy_flat_face_ids(self) -> np.ndarray:
        return self.to_native().to_numpy_flat_face_ids()

    def to_numpy_flat_counts(self) -> np.ndarray:
        return self.to_native().to_numpy_flat_counts()

    def to_numpy_flat_arrays(self) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        return self.to_native().to_numpy_flat_arrays()

    def to_numpy_grouped_indexed_uv_array(self) -> np.ndarray:
        return self.to_native().to_numpy_grouped_indexed_uv_array()

    def __len__(self):
        return len(self.faces)

    def __iter__(self):
        return iter(self.faces)

    def __getitem__(self, index):
        return self.faces[index]

