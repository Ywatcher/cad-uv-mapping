import numpy as np

from tests.fixtures.reference_baking import bake_reference_normal_map


def _pedestal_radius_at_z(z: np.ndarray) -> np.ndarray:
    return (2.0 / 75.0) * z * z - (11.0 / 15.0) * z + 10.0


def test_flat_v_groove_reference_shapes():
    normal_map, ground_truth = bake_reference_normal_map("flat_to_v_groove", width=16, height=16)

    assert normal_map.shape == (16, 16, 3)
    assert normal_map.dtype == np.uint8
    assert ground_truth["high_face_id"].shape == (16, 16)
    assert ground_truth["high_uv"].shape == (16, 16, 2)
    assert ground_truth["normal_world"].shape == (16, 16, 3)
    assert ground_truth["normal_tangent"].shape == (16, 16, 3)
    assert ground_truth["hit_mask"].all()


def test_pedestal_reference_ribs_are_outward():
    width = 96
    height = 32
    normal_map, ground_truth = bake_reference_normal_map("pedestal_ribs", width=width, height=height)

    distance = ground_truth["distance"]
    position = ground_truth["position"]
    normal_tangent = ground_truth["normal_tangent"]
    normal_world = ground_truth["normal_world"]

    assert ground_truth["hit_mask"].all()
    assert np.max(distance) > 0.20
    assert np.min(distance) >= 0.0
    assert np.allclose(np.linalg.norm(normal_world, axis=-1), 1.0)
    assert np.allclose(np.linalg.norm(normal_tangent, axis=-1), 1.0)

    expected_radius = _pedestal_radius_at_z(ground_truth["high_uv"][..., 1] * 15.0)
    radial_distance = np.linalg.norm(position[..., :2], axis=-1)
    assert np.all(radial_distance + 1e-9 >= expected_radius)

    row = height // 2
    red = normal_map[row, :, 0].astype(np.int16)
    assert red[47] > 127
    assert red[48] < 127
