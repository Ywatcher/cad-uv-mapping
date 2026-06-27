import numpy as np

from cad_uv_map import (
    describe_shape_faces,
    mapping_batch_to_structured_array,
    map_shape_low_face_samples_to_high_faces,
    map_shape_low_face_uv_grid_to_high_face_uv_grid,
    normalize_face_uv_samples,
)
from cad_uv_map.api import MappingContext
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
    samples = _sample_uv_grid(low_face, 9, 9)
    context = MappingContext()
    context.enable_parallel = True

    batch = map_shape_low_face_samples_to_high_faces(
        pair.low,
        pair.high,
        low_face.face_id,
        samples,
        context,
    )

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


def test_native_single_low_face_uv_grid_mapping_returns_structured_numpy_grid():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = np.array(_sample_uv_grid(low_face, 2, 2), dtype=np.float64).reshape(2, 2, 2)
    context = MappingContext()
    context.enable_parallel = True

    mapped = map_shape_low_face_uv_grid_to_high_face_uv_grid(
        pair.low,
        pair.high,
        low_face.face_id,
        samples,
        context,
    )

    assert mapped.shape == (2, 2)
    assert mapped.dtype.names == ("high_face_id", "high_u", "high_v")
    assert np.all(mapped["high_face_id"] == low_face.face_id)
    assert np.allclose(mapped["high_u"], samples[..., 0])
    assert np.allclose(mapped["high_v"], samples[..., 1])


def test_python_conversion_helpers_keep_grouped_face_samples_and_mapping_batches_usable():
    batch = normalize_face_uv_samples({3: [(0.25, 0.5), (0.75, 0.5)]})
    assert len(batch.faces) == 1
    assert batch.faces[0].face_id == 3
    assert [sample.index for sample in batch.faces[0].samples] == [0, 1]

    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = _sample_uv_grid(low_face, 1, 2)
    context = MappingContext()
    context.enable_parallel = True
    batch = map_shape_low_face_samples_to_high_faces(
        pair.low,
        pair.high,
        low_face.face_id,
        samples,
        context,
    )
    structured = mapping_batch_to_structured_array(batch)
    assert structured.shape == (len(samples),)
    assert structured.dtype.names == (
        "index",
        "low_face_id",
        "low_u",
        "low_v",
        "high_face_id",
        "high_u",
        "high_v",
        "point_x",
        "point_y",
        "point_z",
        "distance",
        "status",
    )
