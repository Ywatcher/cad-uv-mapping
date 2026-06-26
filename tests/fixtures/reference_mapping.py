from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable

import numpy as np
from OCP.BRep import BRep_Tool
from OCP.BRepAdaptor import BRepAdaptor_Surface
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeVertex
from OCP.BRepExtrema import BRepExtrema_DistShapeShape
from OCP.BRepLProp import BRepLProp_SLProps
from OCP.ShapeAnalysis import ShapeAnalysis_Surface
from OCP.TopAbs import TopAbs_REVERSED
from OCP.TopoDS import TopoDS_Face
from OCP.gp import gp_Dir, gp_Pnt


@dataclass(frozen=True)
class LowSample:
    low_face_id: int
    low_u: float
    low_v: float
    point: tuple[float, float, float]
    normal: tuple[float, float, float]


@dataclass(frozen=True)
class ReferenceHit:
    low_face_id: int
    low_u: float
    low_v: float
    high_face_id: int
    high_u: float
    high_v: float
    point: tuple[float, float, float]
    normal: tuple[float, float, float]
    distance: float
    status: str


def face_uv_bounds(face: TopoDS_Face) -> tuple[float, float, float, float]:
    adaptor = BRepAdaptor_Surface(face)
    return (
        adaptor.FirstUParameter(),
        adaptor.LastUParameter(),
        adaptor.FirstVParameter(),
        adaptor.LastVParameter(),
    )


def eval_face(face: TopoDS_Face, u: float, v: float) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    adaptor = BRepAdaptor_Surface(face)
    point = adaptor.Value(u, v)
    props = BRepLProp_SLProps(adaptor, u, v, 1, 1e-7)

    normal = gp_Dir(0.0, 0.0, 1.0)
    if props.IsNormalDefined():
        normal = props.Normal()

    nx, ny, nz = normal.X(), normal.Y(), normal.Z()
    if face.Orientation() == TopAbs_REVERSED:
        nx, ny, nz = -nx, -ny, -nz

    return (point.X(), point.Y(), point.Z()), (nx, ny, nz)


def sample_face_uv_grid(
    face: TopoDS_Face,
    low_face_id: int,
    u_count: int,
    v_count: int,
    margin: float = 0.5,
) -> list[LowSample]:
    """Sample a face in native UV space.

    `margin` is a normalized inset from each UV cell edge. The default samples cell
    centers and avoids many exact-boundary tolerance cases in reference data.
    """
    u_min, u_max, v_min, v_max = face_uv_bounds(face)
    samples: list[LowSample] = []

    for vi in range(v_count):
        v_t = (vi + margin) / v_count
        v = v_min + (v_max - v_min) * v_t
        for ui in range(u_count):
            u_t = (ui + margin) / u_count
            u = u_min + (u_max - u_min) * u_t
            point, normal = eval_face(face, u, v)
            samples.append(LowSample(low_face_id, u, v, point, normal))

    return samples


def _point_to_vertex(point: tuple[float, float, float]):
    return BRepBuilderAPI_MakeVertex(gp_Pnt(*point)).Vertex()


def _project_point_to_face(
    point: tuple[float, float, float],
    high_face_id: int,
    high_face: TopoDS_Face,
) -> ReferenceHit | None:
    vertex = _point_to_vertex(point)
    dist = BRepExtrema_DistShapeShape(vertex, high_face)
    dist.Perform()
    if not dist.IsDone() or dist.NbSolution() == 0:
        return None

    hit_point = dist.PointOnShape2(1)
    sas = ShapeAnalysis_Surface(BRep_Tool.Surface_s(high_face))
    uv_point = sas.ValueOfUV(hit_point, 1e-7)
    high_u, high_v = uv_point.X(), uv_point.Y()
    _, normal = eval_face(high_face, high_u, high_v)

    return ReferenceHit(
        low_face_id=-1,
        low_u=np.nan,
        low_v=np.nan,
        high_face_id=high_face_id,
        high_u=high_u,
        high_v=high_v,
        point=(hit_point.X(), hit_point.Y(), hit_point.Z()),
        normal=normal,
        distance=dist.Value(),
        status="hit",
    )


def nearest_reference_hits(
    low_samples: Iterable[LowSample],
    high_faces: list[TopoDS_Face],
) -> list[ReferenceHit]:
    """Slow single-threaded ground truth by nearest OCCT face distance.

    This is intentionally simple and deterministic. It is not the production
    algorithm; it is a reference oracle for small fixture grids.
    """
    hits: list[ReferenceHit] = []

    for sample in low_samples:
        best: ReferenceHit | None = None
        for high_face_id, high_face in enumerate(high_faces):
            candidate = _project_point_to_face(sample.point, high_face_id, high_face)
            if candidate is None:
                continue
            if best is None or candidate.distance < best.distance:
                best = candidate

        if best is None:
            hits.append(
                ReferenceHit(
                    low_face_id=sample.low_face_id,
                    low_u=sample.low_u,
                    low_v=sample.low_v,
                    high_face_id=-1,
                    high_u=np.nan,
                    high_v=np.nan,
                    point=(np.nan, np.nan, np.nan),
                    normal=(np.nan, np.nan, np.nan),
                    distance=np.inf,
                    status="no_hit",
                )
            )
        else:
            hits.append(
                ReferenceHit(
                    low_face_id=sample.low_face_id,
                    low_u=sample.low_u,
                    low_v=sample.low_v,
                    high_face_id=best.high_face_id,
                    high_u=best.high_u,
                    high_v=best.high_v,
                    point=best.point,
                    normal=best.normal,
                    distance=best.distance,
                    status=best.status,
                )
            )

    return hits
