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


@dataclass(frozen=True, slots=True)
class MappingResult:
    low_face_id: int
    low_uv: UvCoord
    high_face_id: int
    high_uv: UvCoord
    point: Vec3
    distance: float
    status: Any

    @classmethod
    def from_python(cls, value: Any) -> "MappingResult":
        if isinstance(value, cls):
            return value
        if hasattr(value, "low_face_id"):
            low_uv = getattr(value, "low_uv", None)
            high_uv = getattr(value, "high_uv", None)
            point = getattr(value, "point", None)
            if low_uv is None:
                low_uv = UvCoord(float(value.low_u), float(value.low_v))
            else:
                low_uv = UvCoord.from_python(low_uv)
            if high_uv is None:
                high_uv = UvCoord(float(value.high_u), float(value.high_v))
            else:
                high_uv = UvCoord.from_python(high_uv)
            if point is None:
                point = Vec3(float(value.point_x), float(value.point_y), float(value.point_z))
            else:
                point = Vec3.from_python(point)
            return cls(
                int(value.low_face_id),
                low_uv,
                int(value.high_face_id),
                high_uv,
                point,
                float(value.distance),
                _native_status_value(value.status),
            )
        if isinstance(value, Mapping):
            return cls(
                int(value["low_face_id"]),
                UvCoord.from_python(value.get("low_uv", (value["low_u"], value["low_v"]))),
                int(value["high_face_id"]),
                UvCoord.from_python(value.get("high_uv", (value["high_u"], value["high_v"]))),
                Vec3.from_python(value.get("point", (value["point_x"], value["point_y"], value["point_z"]))),
                float(value["distance"]),
                _native_status_value(value["status"]),
            )
        raise TypeError(f"unsupported mapping result: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "MappingResult":
        return cls(
            int(value.low_face_id),
            UvCoord.from_native(value.low_uv),
            int(value.high_face_id),
            UvCoord.from_native(value.high_uv),
            Vec3.from_native(value.point),
            float(value.distance),
            _native_status_value(value.status),
        )

    def to_native(self):
        native = _native.MappingResult()
        native.low_face_id = int(self.low_face_id)
        native.low_uv = self.low_uv.to_native()
        native.high_face_id = int(self.high_face_id)
        native.high_uv = self.high_uv.to_native()
        native.point = self.point.to_native()
        native.distance = float(self.distance)
        native.status = _to_native_status(self.status)
        return native

    def to_python(self):
        return {
            "low_face_id": int(self.low_face_id),
            "low_uv": self.low_uv.to_python(),
            "high_face_id": int(self.high_face_id),
            "high_uv": self.high_uv.to_python(),
            "point": self.point.to_python(),
            "distance": float(self.distance),
            "status": _native_status_value(self.status),
        }

    def to_numpy_low_uv(self) -> np.ndarray:
        return self.low_uv.to_numpy()

    def to_numpy_high_uv(self) -> np.ndarray:
        return self.high_uv.to_numpy()

    def to_numpy_point(self) -> np.ndarray:
        return self.point.to_numpy()

    def to_numpy_status(self) -> np.ndarray:
        return np.asarray([int(_native_status_value(self.status))], dtype=np.int32)


@dataclass(frozen=True, slots=True)
class IndexedMappingResult:
    index: int
    value: MappingResult

    @classmethod
    def from_python(cls, value: Any) -> "IndexedMappingResult":
        if isinstance(value, cls):
            return value
        if hasattr(value, "index") and hasattr(value, "value"):
            return cls(int(value.index), MappingResult.from_python(value.value))
        if isinstance(value, Mapping):
            return cls(int(value["index"]), MappingResult.from_python(value["value"]))
        if _is_sequence(value) and len(value) == 2:
            index, mapped = value
            return cls(int(index), MappingResult.from_python(mapped))
        raise TypeError(f"unsupported indexed mapping result: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "IndexedMappingResult":
        return cls(int(value.index), MappingResult.from_native(value.value))

    def to_native(self):
        native = _native.IndexedMappingResult()
        native.index = int(self.index)
        native.value = self.value.to_native()
        return native

    def to_python(self):
        return {"index": int(self.index), "value": self.value.to_python()}


@dataclass(slots=True)
class MappingResultBatch:
    results: list[IndexedMappingResult] = field(default_factory=list)

    @classmethod
    def from_python(cls, value: Any) -> "MappingResultBatch":
        if isinstance(value, cls):
            return value
        if hasattr(value, "results"):
            return cls([IndexedMappingResult.from_python(result) for result in value.results])
        if isinstance(value, np.ndarray) and value.dtype.names:
            results = []
            for row in value:
                results.append(
                    IndexedMappingResult(
                        int(row["index"]),
                        MappingResult(
                            int(row["low_face_id"]),
                            UvCoord(float(row["low_u"]), float(row["low_v"])),
                            int(row["high_face_id"]),
                            UvCoord(float(row["high_u"]), float(row["high_v"])),
                            Vec3(float(row["point_x"]), float(row["point_y"]), float(row["point_z"])),
                            float(row["distance"]),
                            int(row["status"]),
                        ),
                    )
                )
            return cls(results)

        return cls([IndexedMappingResult.from_python(result) for result in value])

    @classmethod
    def from_native(cls, value: Any) -> "MappingResultBatch":
        return cls([IndexedMappingResult.from_native(result) for result in value.results])

    def to_native(self):
        """Convert to native `_native.MappingResultBatch`."""
        native = _native.MappingResultBatch()
        native.results = [result.to_native() for result in self.results]
        return native

    def to_python(self):
        return [result.to_python() for result in self.results]

    def to_numpy_low_uv_array(self) -> np.ndarray:
        return _stack_uv([result.value.low_uv for result in self.results])

    def to_numpy_high_uv_array(self) -> np.ndarray:
        return _stack_uv([result.value.high_uv for result in self.results])

    def to_numpy_point_array(self) -> np.ndarray:
        return _stack_vec3([result.value.point for result in self.results])

    def to_numpy_distance_array(self) -> np.ndarray:
        return np.asarray([float(result.value.distance) for result in self.results], dtype=np.float64)

    def to_numpy_high_face_id_array(self) -> np.ndarray:
        return np.asarray([int(result.value.high_face_id) for result in self.results], dtype=np.int32)

    def to_numpy_low_face_id_array(self) -> np.ndarray:
        return np.asarray([int(result.value.low_face_id) for result in self.results], dtype=np.int32)

    def to_numpy_status_array(self) -> np.ndarray:
        return _status_array([result.value.status for result in self.results])

    def to_numpy_high_uv_and_face_id(self) -> tuple[np.ndarray, np.ndarray]:
        return self.to_numpy_high_face_id_array(), self.to_numpy_high_uv_array()

    def to_numpy_low_uv_and_high_uv(self) -> tuple[np.ndarray, np.ndarray]:
        return self.to_numpy_low_uv_array(), self.to_numpy_high_uv_array()

    def to_numpy_structured_array(self):
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
        mapped = np.empty(len(self.results), dtype=dtype)
        for output_index, result in enumerate(self.results):
            value = result.value
            mapped[output_index] = (
                int(result.index),
                int(value.low_face_id),
                float(value.low_uv.u),
                float(value.low_uv.v),
                int(value.high_face_id),
                float(value.high_uv.u),
                float(value.high_uv.v),
                float(value.point.x),
                float(value.point.y),
                float(value.point.z),
                float(value.distance),
                int(_native_status_value(value.status)),
            )
        return mapped

    def __len__(self):
        return len(self.results)

    def __iter__(self):
        return iter(self.results)

    def __getitem__(self, index):
        return self.results[index]


@dataclass(frozen=True, slots=True)
class SurfaceEvalResult:
    face_id: int
    uv: UvCoord
    point: Vec3
    normal: Vec3
    normal_defined: bool

    @classmethod
    def from_python(cls, value: Any) -> "SurfaceEvalResult":
        if isinstance(value, cls):
            return value
        if hasattr(value, "face_id"):
            return cls(
                int(value.face_id),
                UvCoord.from_python(value.uv),
                Vec3.from_python(value.point),
                Vec3.from_python(value.normal),
                bool(value.normal_defined),
            )
        if isinstance(value, Mapping):
            return cls(
                int(value["face_id"]),
                UvCoord.from_python(value["uv"]),
                Vec3.from_python(value["point"]),
                Vec3.from_python(value["normal"]),
                bool(value["normal_defined"]),
            )
        raise TypeError(f"unsupported surface evaluation result: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "SurfaceEvalResult":
        return cls(
            int(value.face_id),
            UvCoord.from_native(value.uv),
            Vec3.from_native(value.point),
            Vec3.from_native(value.normal),
            bool(value.normal_defined),
        )

    def to_native(self):
        native = _native.SurfaceEvalResult()
        native.face_id = int(self.face_id)
        native.uv = self.uv.to_native()
        native.point = self.point.to_native()
        native.normal = self.normal.to_native()
        native.normal_defined = bool(self.normal_defined)
        return native

    def to_python(self):
        return {
            "face_id": int(self.face_id),
            "uv": self.uv.to_python(),
            "point": self.point.to_python(),
            "normal": self.normal.to_python(),
            "normal_defined": bool(self.normal_defined),
        }

    def to_numpy_uv(self) -> np.ndarray:
        return self.uv.to_numpy()

    def to_numpy_point(self) -> np.ndarray:
        return self.point.to_numpy()

    def to_numpy_normal(self) -> np.ndarray:
        return self.normal.to_numpy()

    def to_numpy_face_id(self) -> np.ndarray:
        return np.asarray([int(self.face_id)], dtype=np.int32)

    def to_numpy_normal_defined(self) -> np.ndarray:
        return np.asarray([bool(self.normal_defined)], dtype=bool)


@dataclass(frozen=True, slots=True)
class IndexedSurfaceEvalResult:
    index: int
    value: SurfaceEvalResult

    @classmethod
    def from_python(cls, value: Any) -> "IndexedSurfaceEvalResult":
        if isinstance(value, cls):
            return value
        if hasattr(value, "index") and hasattr(value, "value"):
            return cls(int(value.index), SurfaceEvalResult.from_python(value.value))
        if isinstance(value, Mapping):
            return cls(int(value["index"]), SurfaceEvalResult.from_python(value["value"]))
        if _is_sequence(value) and len(value) == 2:
            index, record = value
            return cls(int(index), SurfaceEvalResult.from_python(record))
        raise TypeError(f"unsupported indexed surface evaluation result: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "IndexedSurfaceEvalResult":
        return cls(int(value.index), SurfaceEvalResult.from_native(value.value))

    def to_native(self):
        native = _native.IndexedSurfaceEvalResult()
        native.index = int(self.index)
        native.value = self.value.to_native()
        return native

    def to_python(self):
        return {"index": int(self.index), "value": self.value.to_python()}


@dataclass(slots=True)
class SurfaceEvalResultBatch:
    results: list[IndexedSurfaceEvalResult] = field(default_factory=list)

    @classmethod
    def from_python(cls, value: Any) -> "SurfaceEvalResultBatch":
        if isinstance(value, cls):
            return value
        if hasattr(value, "results"):
            return cls([IndexedSurfaceEvalResult.from_python(result) for result in value.results])
        return cls([IndexedSurfaceEvalResult.from_python(result) for result in value])

    @classmethod
    def from_native(cls, value: Any) -> "SurfaceEvalResultBatch":
        return cls([IndexedSurfaceEvalResult.from_native(result) for result in value.results])

    def to_native(self):
        """Convert to native `_native.SurfaceEvalResultBatch`."""
        native = _native.SurfaceEvalResultBatch()
        native.results = [result.to_native() for result in self.results]
        return native

    def to_python(self):
        return [result.to_python() for result in self.results]

    def to_numpy_uv_array(self) -> np.ndarray:
        return self.to_native().to_numpy_uv_array()

    def to_numpy_point_array(self) -> np.ndarray:
        return self.to_native().to_numpy_point_array()

    def to_numpy_normal_array(self) -> np.ndarray:
        return self.to_native().to_numpy_normal_array()

    def to_numpy_face_id_array(self) -> np.ndarray:
        return self.to_native().to_numpy_face_id_array()

    def to_numpy_normal_defined_mask(self) -> np.ndarray:
        return self.to_native().to_numpy_normal_defined_mask()

    def __len__(self):
        return len(self.results)

    def __iter__(self):
        return iter(self.results)

    def __getitem__(self, index):
        return self.results[index]


@dataclass(frozen=True, slots=True)
class MappedSampleRecord:
    sample: FlatUvSample
    mapping: MappingResult
    surface: SurfaceEvalResult

    @classmethod
    def from_python(cls, value: Any) -> "MappedSampleRecord":
        if isinstance(value, cls):
            return value
        if hasattr(value, "sample") and hasattr(value, "mapping") and hasattr(value, "surface"):
            return cls(
                FlatUvSample.from_python(value.sample),
                MappingResult.from_python(value.mapping),
                SurfaceEvalResult.from_python(value.surface),
            )
        if isinstance(value, Mapping):
            return cls(
                FlatUvSample.from_python(value["sample"]),
                MappingResult.from_python(value["mapping"]),
                SurfaceEvalResult.from_python(value["surface"]),
            )
        raise TypeError(f"unsupported mapped sample record: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "MappedSampleRecord":
        return cls(
            FlatUvSample.from_native(value.sample),
            MappingResult.from_native(value.mapping),
            SurfaceEvalResult.from_native(value.surface),
        )

    def to_native(self):
        native = _native.MappedSampleRecord()
        native.sample = self.sample.to_native()
        native.mapping = self.mapping.to_native()
        native.surface = self.surface.to_native()
        return native

    def to_python(self):
        return {
            "sample": self.sample.to_python(),
            "mapping": self.mapping.to_python(),
            "surface": self.surface.to_python(),
        }

    def to_numpy_low_uv(self) -> np.ndarray:
        return self.to_native().to_numpy_low_uv()

    def to_numpy_high_uv(self) -> np.ndarray:
        return self.to_native().to_numpy_high_uv()

    def to_numpy_point(self) -> np.ndarray:
        return self.to_native().to_numpy_point()

    def to_numpy_normal(self) -> np.ndarray:
        return self.to_native().to_numpy_normal()


@dataclass(frozen=True, slots=True)
class IndexedMappedSampleRecord:
    index: int
    value: MappedSampleRecord

    @classmethod
    def from_python(cls, value: Any) -> "IndexedMappedSampleRecord":
        if isinstance(value, cls):
            return value
        if hasattr(value, "index") and hasattr(value, "value"):
            return cls(int(value.index), MappedSampleRecord.from_python(value.value))
        if isinstance(value, Mapping):
            return cls(int(value["index"]), MappedSampleRecord.from_python(value["value"]))
        if _is_sequence(value) and len(value) == 2:
            index, record = value
            return cls(int(index), MappedSampleRecord.from_python(record))
        raise TypeError(f"unsupported indexed mapped sample record: {value!r}")

    @classmethod
    def from_native(cls, value: Any) -> "IndexedMappedSampleRecord":
        return cls(int(value.index), MappedSampleRecord.from_native(value.value))

    def to_native(self):
        native = _native.IndexedMappedSampleRecord()
        native.index = int(self.index)
        native.value = self.value.to_native()
        return native

    def to_python(self):
        return {"index": int(self.index), "value": self.value.to_python()}


@dataclass(slots=True)
class MappedSampleBatch:
    records: list[IndexedMappedSampleRecord] = field(default_factory=list)

    @classmethod
    def from_python(cls, value: Any) -> "MappedSampleBatch":
        if isinstance(value, cls):
            return value
        if hasattr(value, "records"):
            return cls([IndexedMappedSampleRecord.from_python(record) for record in value.records])
        return cls([IndexedMappedSampleRecord.from_python(record) for record in value])

    @classmethod
    def from_native(cls, value: Any) -> "MappedSampleBatch":
        return cls([IndexedMappedSampleRecord.from_native(record) for record in value.records])

    def to_native(self):
        """Convert to native `_native.MappedSampleBatch`."""
        native = _native.MappedSampleBatch()
        native.records = [record.to_native() for record in self.records]
        return native

    def to_python(self):
        return [record.to_python() for record in self.records]

    def to_numpy_low_uv_array(self) -> np.ndarray:
        return self.to_native().to_numpy_low_uv_array()

    def to_numpy_high_uv_array(self) -> np.ndarray:
        return self.to_native().to_numpy_high_uv_array()

    def to_numpy_point_array(self) -> np.ndarray:
        return self.to_native().to_numpy_point_array()

    def to_numpy_normal_array(self) -> np.ndarray:
        return self.to_native().to_numpy_normal_array()

    def to_numpy_face_id_array(self) -> np.ndarray:
        return self.to_native().to_numpy_face_id_array()

    def to_numpy_status_array(self) -> np.ndarray:
        return self.to_native().to_numpy_status_array()

    def to_numpy_normal_defined_mask(self) -> np.ndarray:
        return self.to_native().to_numpy_normal_defined_mask()

    def __len__(self):
        return len(self.records)

    def __iter__(self):
        return iter(self.records)

    def __getitem__(self, index):
        return self.records[index]
