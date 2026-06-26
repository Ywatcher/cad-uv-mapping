import numpy as np

from tests.fixtures.cad_cases import flat_to_v_groove_pair, identity_box_pair
from tests.fixtures.cad_reference_baking import bake_cad_pair_face_reference


def _top_face_index(faces) -> int:
    for idx, face in enumerate(faces):
        center = face.center()
        normal = face.normal_at(center)
        if normal.Z > 0.9:
            return idx
    raise RuntimeError("could not find top face")


def test_identity_box_cad_reference_is_flat_tangent_normal():
    pair = identity_box_pair()
    low_face_id = _top_face_index(pair.low.faces())

    normal_map, ground_truth = bake_cad_pair_face_reference(pair, low_face_id, width=8, height=8)

    assert normal_map.shape == (8, 8, 3)
    assert ground_truth["hit_mask"].all()
    assert np.allclose(ground_truth["normal_tangent"], np.array([0.0, 0.0, 1.0]))
    assert np.allclose(np.linalg.norm(ground_truth["normal_world"], axis=-1), 1.0)


def test_v_groove_cad_reference_retrieves_high_cad_normals():
    pair = flat_to_v_groove_pair()
    low_face_id = _top_face_index(pair.low.faces())

    _normal_map, ground_truth = bake_cad_pair_face_reference(pair, low_face_id, width=32, height=32)

    assert ground_truth["hit_mask"].all()
    assert np.any(np.abs(ground_truth["normal_tangent"][..., :2]) > 0.1)
    assert np.allclose(np.linalg.norm(ground_truth["normal_tangent"], axis=-1), 1.0)
