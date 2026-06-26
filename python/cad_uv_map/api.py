from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO
from pathlib import Path

from . import _native


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
