import numpy as np

from cad_uv_map import describe_shape_faces, map_shape_low_face_samples_to_high_faces
from tests.fixtures.cad_cases import identity_box_pair


def _sample_uv_grid(face_info, u_count: int, v_count: int, margin: float = 0.5):
    samples = []
    for v_index in range(v_count):
        v_t = (v_index + margin) / v_count
        v = face_info.v_min + (face_info.v_max - face_info.v_min) * v_t
        for u_index in range(u_count):
            u_t = (u_index + margin) / u_count
            u = face_info.u_min + (face_info.u_max - face_info.u_min) * u_t
            samples.append((u, v))
    return samples


def _status_name(status) -> str:
    return getattr(status, "name", str(status))


def test_native_single_low_face_mapping_identity_box_returns_zero_distance_hits():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = _sample_uv_grid(low_face, 2, 2)

    batch = map_shape_low_face_samples_to_high_faces(pair.low, pair.high, low_face.face_id, samples)

    assert len(batch.results) == len(samples)
    for expected_index, result in enumerate(batch.results):
        value = result.value
        assert result.index == expected_index
        assert _status_name(value.status) == "hit"
        assert value.low_face_id == low_face.face_id
        assert value.high_face_id == low_face.face_id
        assert np.isclose(value.high_u, value.low_u)
        assert np.isclose(value.high_v, value.low_v)
        assert np.isclose(value.distance, 0.0)
