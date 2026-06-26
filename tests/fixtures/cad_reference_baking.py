from __future__ import annotations

import numpy as np
from OCP.BRepAdaptor import BRepAdaptor_Surface
from OCP.TopoDS import TopoDS_Face

from tests.fixtures.cad_cases import CadPair
from tests.fixtures.reference_baking import _normal_to_rgb
from tests.fixtures.reference_mapping import face_uv_bounds, nearest_reference_hits, sample_face_uv_grid


def _normalize(vec: np.ndarray, fallback: np.ndarray) -> np.ndarray:
    norm = np.linalg.norm(vec)
    if norm <= 1e-12:
        return fallback.astype(np.float64)
    return vec / norm


def _point_tuple_to_array(point: tuple[float, float, float]) -> np.ndarray:
    return np.array(point, dtype=np.float64)


def low_face_uv_tangent_basis(
    low_face: TopoDS_Face,
    u: float,
    v: float,
    normal: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return an approximate low-face TBN basis at a native UV coordinate.

    FIXME: This uses finite differences on the underlying surface and does not
    account for texture seams, degenerate UV directions, or user-authored UVs
    that differ from native OCCT parameters. It is good enough as a reference
    scaffold until the project has an explicit UV/tangent contract.
    """
    adaptor = BRepAdaptor_Surface(low_face)
    u_min, u_max, v_min, v_max = face_uv_bounds(low_face)
    u_span = max(abs(u_max - u_min), 1.0)
    v_span = max(abs(v_max - v_min), 1.0)
    du = u_span * 1e-5
    dv = v_span * 1e-5

    p = adaptor.Value(u, v)
    u_probe = min(max(u + du, u_min), u_max)
    v_probe = min(max(v + dv, v_min), v_max)
    if abs(u_probe - u) <= 1e-12:
        u_probe = min(max(u - du, u_min), u_max)
    if abs(v_probe - v) <= 1e-12:
        v_probe = min(max(v - dv, v_min), v_max)

    p_u = adaptor.Value(u_probe, v)
    p_v = adaptor.Value(u, v_probe)
    n = _normalize(normal, np.array([0.0, 0.0, 1.0], dtype=np.float64))

    tangent = np.array([p_u.X() - p.X(), p_u.Y() - p.Y(), p_u.Z() - p.Z()], dtype=np.float64)
    bitangent_hint = np.array([p_v.X() - p.X(), p_v.Y() - p.Y(), p_v.Z() - p.Z()], dtype=np.float64)

    tangent = tangent - n * float(np.dot(tangent, n))
    tangent = _normalize(tangent, _orthogonal_fallback(n))
    bitangent = np.cross(n, tangent)
    bitangent = _normalize(bitangent, _orthogonal_fallback(n))

    # Preserve the native V direction when possible.
    if np.dot(bitangent, bitangent_hint) < 0.0:
        bitangent = -bitangent
        tangent = -tangent

    return tangent, bitangent, n


def _orthogonal_fallback(normal: np.ndarray) -> np.ndarray:
    axis = np.array([1.0, 0.0, 0.0], dtype=np.float64)
    if abs(float(np.dot(normal, axis))) > 0.9:
        axis = np.array([0.0, 1.0, 0.0], dtype=np.float64)
    tangent = axis - normal * float(np.dot(axis, normal))
    return _normalize(tangent, np.array([1.0, 0.0, 0.0], dtype=np.float64))


def world_normal_to_tangent(normal: np.ndarray, basis: tuple[np.ndarray, np.ndarray, np.ndarray]) -> np.ndarray:
    tangent, bitangent, low_normal = basis
    normal = _normalize(normal, low_normal)
    tangent_normal = np.array(
        [
            np.dot(normal, tangent),
            np.dot(normal, bitangent),
            np.dot(normal, low_normal),
        ],
        dtype=np.float64,
    )
    return _normalize(tangent_normal, np.array([0.0, 0.0, 1.0], dtype=np.float64))


def bake_low_face_cad_reference(
    low_face: TopoDS_Face,
    high_faces: list[TopoDS_Face],
    low_face_id: int = 0,
    width: int = 96,
    height: int = 96,
) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    """Bake one low CAD face by projecting samples to high CAD faces.

    FIXME: Projection is currently nearest-surface only. A production reference
    should also support normal-ray/cage projection and explicit hit filtering so
    raised details do not accidentally map to a nearby side wall.
    """
    samples = sample_face_uv_grid(low_face, low_face_id, width, height)
    hits = nearest_reference_hits(samples, high_faces)

    normal_map = np.zeros((height, width, 3), dtype=np.uint8)
    status = np.full((height, width), "failed", dtype=object)
    low_uv = np.full((height, width, 2), np.nan, dtype=np.float64)
    low_position = np.full((height, width, 3), np.nan, dtype=np.float64)
    low_normal = np.full((height, width, 3), np.nan, dtype=np.float64)
    high_face_id = np.full((height, width), -1, dtype=np.int32)
    high_uv = np.full((height, width, 2), np.nan, dtype=np.float64)
    position = np.full((height, width, 3), np.nan, dtype=np.float64)
    normal_world = np.full((height, width, 3), np.nan, dtype=np.float64)
    normal_tangent = np.full((height, width, 3), np.nan, dtype=np.float64)
    distance = np.full((height, width), np.inf, dtype=np.float64)
    hit_mask = np.zeros((height, width), dtype=np.bool_)

    for idx, sample in enumerate(samples):
        row = idx // width
        col = idx % width
        sample_normal = _point_tuple_to_array(sample.normal)
        basis = low_face_uv_tangent_basis(low_face, sample.low_u, sample.low_v, sample_normal)
        flat_tangent_normal = np.array([0.0, 0.0, 1.0], dtype=np.float64)

        low_uv[row, col] = np.array([sample.low_u, sample.low_v], dtype=np.float64)
        low_position[row, col] = _point_tuple_to_array(sample.point)
        low_normal[row, col] = basis[2]

        hit = hits[idx]
        status[row, col] = hit.status
        if hit.status == "hit":
            world_normal = _point_tuple_to_array(hit.normal)
            tangent_normal = world_normal_to_tangent(world_normal, basis)

            high_face_id[row, col] = hit.high_face_id
            high_uv[row, col] = np.array([hit.high_u, hit.high_v], dtype=np.float64)
            position[row, col] = _point_tuple_to_array(hit.point)
            normal_world[row, col] = _normalize(world_normal, basis[2])
            normal_tangent[row, col] = tangent_normal
            distance[row, col] = hit.distance
            hit_mask[row, col] = True
            normal_map[row, col] = _normal_to_rgb(tangent_normal)
        else:
            normal_tangent[row, col] = flat_tangent_normal
            normal_map[row, col] = _normal_to_rgb(flat_tangent_normal)

    ground_truth = {
        "status": status,
        "low_face_id": np.full((height, width), low_face_id, dtype=np.int32),
        "low_uv": low_uv,
        "low_position": low_position,
        "low_normal": low_normal,
        "high_face_id": high_face_id,
        "high_uv": high_uv,
        "position": position,
        "normal": normal_world,
        "normal_world": normal_world,
        "normal_tangent": normal_tangent,
        "distance": distance,
        "hit_mask": hit_mask,
    }
    return normal_map, ground_truth


def bake_cad_pair_face_reference(
    pair: CadPair,
    low_face_id: int,
    width: int = 96,
    height: int = 96,
) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    low_face = pair.low.faces()[low_face_id].wrapped
    high_faces = [face.wrapped for face in pair.high.faces()]
    return bake_low_face_cad_reference(low_face, high_faces, low_face_id=low_face_id, width=width, height=height)


def bake_cad_pair_all_faces_reference(
    pair: CadPair,
    width: int = 96,
    height: int = 96,
) -> dict[int, tuple[np.ndarray, dict[str, np.ndarray]]]:
    """Bake every low face independently.

    FIXME: This returns one image per low face. It does not yet pack multiple
    CAD faces into one atlas or honor a game mesh's external UV unwrap.
    """
    return {
        low_face_id: bake_cad_pair_face_reference(pair, low_face_id, width=width, height=height)
        for low_face_id, _face in enumerate(pair.low.faces())
    }


def summarize_reference_hit_counts(result: dict[int, tuple[np.ndarray, dict[str, np.ndarray]]]) -> dict[int, int]:
    return {face_id: int(ground_truth["hit_mask"].sum()) for face_id, (_image, ground_truth) in result.items()}
