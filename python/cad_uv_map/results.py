"""Columnar (struct-of-arrays) result containers for the mapping pipeline.

These wrap the native batch objects and expose results as per-field NumPy arrays
instead of per-record Python objects. The canonical form is columnar:

- per-field array access:   ``batch.high_uv`` -> ``ndarray[N, 2]``
- nested-of-arrays dict:    ``batch.to_dict()``
- one structured array:     ``batch.to_numpy()``
- flat per-row debug view:  ``batch.row(i)``  (O(1), zero-copy vector slices)

There is intentionally no per-record object mirror and no ``from_native`` /
``to_native`` round-trip: a wrapper holds the native batch and reads its
``columns()`` lazily, so chaining (e.g. mapping -> surface evaluation) reuses the
native object without rebuilding Python records.
"""

from __future__ import annotations

from typing import Any

import numpy as np


class _RowView:
    """Lightweight flat view of one row across a batch's columns.

    Built on demand by ``batch.row(i)``; vector fields are zero-copy slices.
    """

    __slots__ = ("_columns", "_index")

    def __init__(self, columns: dict[str, np.ndarray], index: int):
        self._columns = columns
        self._index = index

    def __getattr__(self, name: str):
        columns = object.__getattribute__(self, "_columns")
        if name in columns:
            return columns[name][object.__getattribute__(self, "_index")]
        raise AttributeError(name)

    def __getitem__(self, name: str):
        return self._columns[name][self._index]

    def to_dict(self) -> dict[str, Any]:
        i = self._index
        return {name: column[i] for name, column in self._columns.items()}

    def __repr__(self) -> str:
        return f"Row(index={self._index}, {self.to_dict()!r})"


class _ColumnBatch:
    """Base columnar batch: a dict of name -> NumPy array, all length N."""

    def __init__(self, *, native: Any = None, columns: dict[str, np.ndarray] | None = None):
        object.__setattr__(self, "_native", native)
        object.__setattr__(self, "_cols", columns)

    @classmethod
    def from_native(cls, native: Any) -> "_ColumnBatch":
        return cls(native=native)

    @property
    def columns(self) -> dict[str, np.ndarray]:
        cols = object.__getattribute__(self, "_cols")
        if cols is None:
            native = object.__getattribute__(self, "_native")
            cols = native.columns() if native is not None else {}
            object.__setattr__(self, "_cols", cols)
        return cols

    def to_native(self) -> Any:
        native = object.__getattribute__(self, "_native")
        if native is None:
            raise TypeError(f"{type(self).__name__} has no backing native batch")
        return native

    def column_names(self) -> list[str]:
        return list(self.columns.keys())

    def to_dict(self) -> dict[str, np.ndarray]:
        """Flat dict of column arrays (zero-copy references)."""
        return dict(self.columns)

    def to_numpy(self) -> np.ndarray:
        """One NumPy structured array; vector fields become subarray fields."""
        cols = self.columns
        n = len(self)
        fields = []
        for name, arr in cols.items():
            arr = np.asarray(arr)
            if arr.ndim <= 1:
                fields.append((name, arr.dtype))
            else:
                fields.append((name, arr.dtype, arr.shape[1:]))
        out = np.empty(n, dtype=np.dtype(fields))
        for name, arr in cols.items():
            out[name] = arr
        return out

    def row(self, index: int) -> _RowView:
        return _RowView(self.columns, index)

    def __len__(self) -> int:
        native = object.__getattribute__(self, "_native")
        if native is not None:
            return len(native)
        for arr in self.columns.values():
            return int(np.asarray(arr).shape[0])
        return 0

    def __getattr__(self, name: str):
        # Only reached when normal attribute lookup fails. Guard the private
        # slots so an access before __init__ can't recurse through `columns`.
        if name in ("_native", "_cols"):
            raise AttributeError(name)
        cols = self.columns
        if name in cols:
            return cols[name]
        raise AttributeError(f"{type(self).__name__!r} has no field {name!r}")

    def __repr__(self) -> str:
        return f"{type(self).__name__}(n={len(self)}, fields={self.column_names()})"


class MappingResultBatch(_ColumnBatch):
    """Low-to-high mapping results.

    Columns: index, low_face_id, low_uv[N,2], high_face_id, high_uv[N,2],
    point[N,3], distance, status.
    """


class SurfaceEvalResultBatch(_ColumnBatch):
    """High-face surface-evaluation results.

    Columns: index, face_id, uv[N,2], point[N,3], normal[N,3], normal_defined.
    """


class _CompositeRow:
    __slots__ = ("mapping", "surface")

    def __init__(self, mapping: _RowView, surface: _RowView):
        self.mapping = mapping
        self.surface = surface

    def to_dict(self) -> dict[str, Any]:
        return {"mapping": self.mapping.to_dict(), "surface": self.surface.to_dict()}

    def __repr__(self) -> str:
        return f"Row({self.to_dict()!r})"


class MappedSampleBatch:
    """Combined pipeline result: composes mapping + surface columnar batches.

    The two sub-batches share row order (sample index). Access via
    ``batch.mapping`` / ``batch.surface``; ``to_dict()`` nests by stage.
    """

    def __init__(self, *, native: Any = None,
                 mapping: MappingResultBatch | None = None,
                 surface: SurfaceEvalResultBatch | None = None):
        self._native = native
        if native is not None:
            if mapping is None:
                mapping = MappingResultBatch(columns=native.mapping_columns())
            if surface is None:
                surface = SurfaceEvalResultBatch(columns=native.surface_columns())
        if mapping is not None and surface is not None and len(mapping) != len(surface):
            raise ValueError("mapping and surface sub-batches must have equal length")
        self.mapping = mapping
        self.surface = surface

    @classmethod
    def from_native(cls, native: Any) -> "MappedSampleBatch":
        return cls(native=native)

    def to_native(self) -> Any:
        if self._native is None:
            raise TypeError("MappedSampleBatch has no backing native batch")
        return self._native

    def to_dict(self) -> dict[str, dict[str, np.ndarray]]:
        return {"mapping": self.mapping.to_dict(), "surface": self.surface.to_dict()}

    def row(self, index: int) -> _CompositeRow:
        return _CompositeRow(self.mapping.row(index), self.surface.row(index))

    def __len__(self) -> int:
        return len(self.mapping) if self.mapping is not None else 0

    def __repr__(self) -> str:
        return f"MappedSampleBatch(n={len(self)})"


def mapping_batch_to_structured_array(batch: MappingResultBatch) -> np.ndarray:
    """Convenience: the columnar mapping batch as one structured array."""
    return batch.to_numpy()
