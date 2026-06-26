from dataclasses import dataclass

import numpy as np

from tests.fixtures.cad_cases import flat_to_u_groove_pair, flat_to_v_groove_pair, pedestal_ribs_pair
from tests.fixtures.cad_reference_baking import bake_cad_pair_face_reference
from tests.fixtures.reference_baking import bake_reference_normal_map


@dataclass(frozen=True)
class NormalComparison:
    sample_count: int
    mean_angle_deg: float
    max_angle_deg: float
    p95_angle_deg: float
    fraction_under_45_deg: float


def _top_face_index(faces) -> int:
    for idx, face in enumerate(faces):
        center = face.center()
        normal = face.normal_at(center)
        if normal.Z > 0.9:
            return idx
    raise RuntimeError("could not find top face")


def _pedestal_side_face_index(faces) -> int:
    for idx, face in enumerate(faces):
        center = face.center()
        if 0.1 < center.Z < 14.9:
            return idx
    raise RuntimeError("could not find pedestal side face")


def _compare_tangent_normals(
    oracle: dict[str, np.ndarray],
    candidate: dict[str, np.ndarray],
) -> NormalComparison:
    mask = oracle["hit_mask"] & candidate["hit_mask"]
    oracle_normal = oracle["normal_tangent"][mask]
    candidate_normal = candidate["normal_tangent"][mask]
    dots = np.sum(oracle_normal * candidate_normal, axis=-1)
    angles = np.degrees(np.arccos(np.clip(dots, -1.0, 1.0)))
    return NormalComparison(
        sample_count=int(mask.sum()),
        mean_angle_deg=float(np.mean(angles)),
        max_angle_deg=float(np.max(angles)),
        p95_angle_deg=float(np.percentile(angles, 95)),
        fraction_under_45_deg=float(np.mean(angles < 45.0)),
    )


def test_cuboid_cad_reference_matches_existing_reference_normals():
    cases = [
        ("flat_to_u_groove", flat_to_u_groove_pair()),
        ("flat_to_v_groove", flat_to_v_groove_pair()),
    ]

    for case_name, pair in cases:
        low_face_id = _top_face_index(pair.low.faces())
        _oracle_image, oracle = bake_reference_normal_map(case_name, width=16, height=16)
        _candidate_image, candidate = bake_cad_pair_face_reference(pair, low_face_id, width=16, height=16)

        comparison = _compare_tangent_normals(oracle, candidate)
        assert comparison.sample_count == 16 * 16
        assert comparison.max_angle_deg == 0.0
        assert np.allclose(oracle["normal_tangent"], candidate["normal_tangent"])
        assert np.allclose(oracle["high_uv"], candidate["high_uv"])


def test_pedestal_cad_reference_is_close_to_synthetic_reference_for_most_samples():
    pair = pedestal_ribs_pair()
    low_face_id = _pedestal_side_face_index(pair.low.faces())

    _oracle_image, oracle = bake_reference_normal_map("pedestal_ribs", width=64, height=16)
    _candidate_image, candidate = bake_cad_pair_face_reference(pair, low_face_id, width=64, height=16)

    comparison = _compare_tangent_normals(oracle, candidate)

    assert comparison.sample_count == 64 * 16
    assert comparison.mean_angle_deg < 25.0
    assert comparison.fraction_under_45_deg > 0.85
    # FIXME: Nearest-surface projection still creates rib-edge outliers. Replace
    # this loose p95 threshold after normal-ray/cage projection is implemented.
    assert comparison.p95_angle_deg < 90.0
