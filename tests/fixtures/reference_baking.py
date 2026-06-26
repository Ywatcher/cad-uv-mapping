from __future__ import annotations

import numpy as np

from tests.fixtures.cad_cases import flat_to_u_groove_pair, flat_to_v_groove_pair, pedestal_ribs_pair
from tests.fixtures.reference_mapping import nearest_reference_hits, sample_face_uv_grid


DEFAULT_SIZE = 96
PEDESTAL_HEIGHT = 15.0
CASE_FACTORIES = {
    "flat_to_u_groove": flat_to_u_groove_pair,
    "flat_to_v_groove": flat_to_v_groove_pair,
    "pedestal_ribs": pedestal_ribs_pair,
}


def _normal_to_rgb(normal: np.ndarray) -> np.ndarray:
    return np.clip((normal * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)


def _top_face_index(faces) -> int:
    for idx, face in enumerate(faces):
        center = face.center()
        normal = face.normal_at(center)
        if normal.Z > 0.9:
            return idx
    raise RuntimeError("could not find upward-facing top face")


def _pedestal_radius_at_z(z: float) -> float:
    return (2.0 / 75.0) * z * z - (11.0 / 15.0) * z + 10.0


def _pedestal_uv_tangent_basis(theta: float, dr_dz: float) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    t = np.array([-np.sin(theta), np.cos(theta), 0.0], dtype=np.float64)
    b = np.array([dr_dz * np.cos(theta), dr_dz * np.sin(theta), 1.0], dtype=np.float64)
    b = b / (np.linalg.norm(b) + 1e-12)
    n = np.cross(t, b)
    n = n / (np.linalg.norm(n) + 1e-12)
    return t, b, n


def bake_reference_normal_map(
    case: str = "flat_to_u_groove",
    width: int = DEFAULT_SIZE,
    height: int = DEFAULT_SIZE,
) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    """Bake a deterministic reference normal map and ground-truth arrays.

    The cuboid cases use the slow nearest-surface OCP reference mapper. The
    pedestal case uses a synthetic raised-rib oracle with the same UV convention
    as the preview shader: U is increasing theta and V is increasing height.
    """
    try:
        pair_factory = CASE_FACTORIES[case]
    except KeyError as exc:
        raise ValueError(f"unknown case {case!r}; expected one of {sorted(CASE_FACTORIES)}") from exc

    pair = pair_factory()
    if case == "pedestal_ribs":
        return bake_pedestal_reference_normal_map(width=width, height=height)

    low_faces = pair.low.faces()
    high_faces = [face.wrapped for face in pair.high.faces()]

    low_face_id = _top_face_index(low_faces)
    low_top_face = low_faces[low_face_id].wrapped

    samples = sample_face_uv_grid(low_top_face, low_face_id, width, height)
    hits = nearest_reference_hits(samples, high_faces)

    normal_map = np.zeros((height, width, 3), dtype=np.uint8)
    high_face_id = np.full((height, width), -1, dtype=np.int32)
    high_uv = np.full((height, width, 2), np.nan, dtype=np.float64)
    position = np.full((height, width, 3), np.nan, dtype=np.float64)
    normal_world = np.full((height, width, 3), np.nan, dtype=np.float64)
    normal_tangent = np.full((height, width, 3), np.nan, dtype=np.float64)
    distance = np.full((height, width), np.inf, dtype=np.float64)
    hit_mask = np.zeros((height, width), dtype=np.bool_)

    for idx, hit in enumerate(hits):
        row = idx // width
        col = idx % width
        if hit.status != "hit":
            tangent_normal = np.array([0.0, 0.0, 1.0], dtype=np.float64)
        else:
            tangent_normal = np.array(hit.normal, dtype=np.float64)
            norm_len = np.linalg.norm(tangent_normal)
            if norm_len > 1e-12:
                tangent_normal = tangent_normal / norm_len
            else:
                tangent_normal = np.array([0.0, 0.0, 1.0], dtype=np.float64)

            high_face_id[row, col] = hit.high_face_id
            high_uv[row, col] = np.array([hit.high_u, hit.high_v], dtype=np.float64)
            position[row, col] = np.array(hit.point, dtype=np.float64)
            normal_world[row, col] = tangent_normal
            distance[row, col] = hit.distance
            hit_mask[row, col] = True

        normal_tangent[row, col] = tangent_normal
        normal_map[row, col] = _normal_to_rgb(tangent_normal)

    ground_truth = {
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


def bake_pedestal_reference_normal_map(
    width: int = DEFAULT_SIZE,
    height: int = DEFAULT_SIZE,
) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    normal_map = np.zeros((height, width, 3), dtype=np.uint8)
    high_face_id = np.full((height, width), -1, dtype=np.int32)
    high_uv = np.full((height, width, 2), np.nan, dtype=np.float64)
    position = np.full((height, width, 3), np.nan, dtype=np.float64)
    normal_world = np.full((height, width, 3), np.nan, dtype=np.float64)
    normal_tangent = np.full((height, width, 3), np.nan, dtype=np.float64)
    distance = np.full((height, width), np.inf, dtype=np.float64)
    hit_mask = np.zeros((height, width), dtype=np.bool_)

    rib_count = 16
    angle_step = 2.0 * np.pi / rib_count
    rib_half_width = 0.42
    rib_height = 0.22

    for row in range(height):
        v_norm = (row + 0.5) / height
        z = v_norm * PEDESTAL_HEIGHT
        radius = _pedestal_radius_at_z(z)
        dr_dz = (4.0 / 75.0) * z - (11.0 / 15.0)

        for col in range(width):
            u_norm = (col + 0.5) / width
            theta = u_norm * 2.0 * np.pi - np.pi
            low_point = np.array([radius * np.cos(theta), radius * np.sin(theta), z], dtype=np.float64)
            t, b, low_normal = _pedestal_uv_tangent_basis(theta, dr_dz)

            theta_center = np.round(theta / angle_step) * angle_step
            delta_theta = theta - theta_center
            tangential_distance = radius * np.sin(delta_theta)

            tangent_normal = np.array([0.0, 0.0, 1.0], dtype=np.float64)
            displacement = 0.0
            rib_index = int(np.round(theta / angle_step)) % rib_count

            if abs(tangential_distance) < rib_half_width:
                x = tangential_distance / rib_half_width
                root = np.sqrt(max(1.0 - x * x, 1e-4))
                displacement = rib_height * root
                slope = -(rib_height * x) / (rib_half_width * root)
                tangent_normal = np.array([slope, 0.0, 1.0], dtype=np.float64)
                tangent_normal = tangent_normal / (np.linalg.norm(tangent_normal) + 1e-12)

            high_normal = tangent_normal[0] * t + tangent_normal[1] * b + tangent_normal[2] * low_normal
            high_normal = high_normal / (np.linalg.norm(high_normal) + 1e-12)
            high_point = low_point + low_normal * displacement

            normal_map[row, col] = _normal_to_rgb(tangent_normal)
            high_face_id[row, col] = rib_index
            high_uv[row, col] = np.array([u_norm, v_norm], dtype=np.float64)
            position[row, col] = high_point
            normal_world[row, col] = high_normal
            normal_tangent[row, col] = tangent_normal
            distance[row, col] = displacement
            hit_mask[row, col] = True

    ground_truth = {
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
