import numpy as np

from cad_uv_map import (
    FaceUvSampleGroupBatch,
    describe_shape_faces,
    evaluate_shape_multiple_high_face_samples,
    evaluate_shape_single_high_face_samples,
    MappingResultBatch,
    MappedSampleBatch,
    SurfaceEvalResultBatch,
    map_and_evaluate_shape_multiple_low_face_samples,
    mapping_batch_to_structured_array,
    map_shape_single_low_face_samples_to_high_faces,
    map_shape_single_low_face_samples_to_high_faces_ray,
    map_shape_single_low_face_uv_grid_to_high_face_uv_grid,
    sample_shape_face_uniform_uv_grid,
    sample_shape_face_uniform_uv_tolerance_grid,
    normalize_face_uv_samples,
    UvCoord,
)
from cad_uv_map.api import MappingContext
from tests.fixtures.cad_cases import identity_box_pair, pedestal_ribs_pair


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

    batch = map_shape_single_low_face_samples_to_high_faces(
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
        assert np.isclose(value.high_uv.u, value.low_uv.u)
        assert np.isclose(value.high_uv.v, value.low_uv.v)
        assert np.isclose(value.distance, 0.0)


def test_native_single_low_face_uv_grid_mapping_returns_structured_numpy_grid():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = np.array(_sample_uv_grid(low_face, 2, 2), dtype=np.float64).reshape(2, 2, 2)
    context = MappingContext()
    context.enable_parallel = True

    mapped = map_shape_single_low_face_uv_grid_to_high_face_uv_grid(
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
    assert isinstance(batch, FaceUvSampleGroupBatch)
    assert len(batch.faces) == 1
    assert batch.faces[0].face_id == 3
    assert [sample.index for sample in batch.faces[0].samples] == [0, 1]
    assert batch.to_python()[0]["face_id"] == 3
    assert batch[0].to_numpy_uv_array().shape == (2, 2)
    assert batch[0].to_numpy_indexed_uv_array().shape == (2, 3)
    face_ids, indices, uvs = batch.to_numpy_flat_arrays()
    assert face_ids.shape == (2,)
    assert indices.shape == (2,)
    assert uvs.shape == (2, 2)

    uv = UvCoord.from_python([0.25, 0.5])
    assert len(uv) == 2
    assert tuple(uv) == (0.25, 0.5)
    assert uv == UvCoord(0.25, 0.5)

    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = _sample_uv_grid(low_face, 1, 2)
    context = MappingContext()
    context.enable_parallel = True
    native_batch = map_shape_single_low_face_samples_to_high_faces(
        pair.low,
        pair.high,
        low_face.face_id,
        samples,
        context,
    )
    batch = MappingResultBatch.from_native(native_batch)
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
    assert batch.to_numpy_high_uv_array().shape == (len(samples), 2)
    assert batch.to_numpy_low_uv_array().shape == (len(samples), 2)
    assert batch.to_numpy_high_face_id_array().shape == (len(samples),)


def test_native_uniform_uv_sampler_generates_face_group_in_cpp():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]

    group = sample_shape_face_uniform_uv_grid(pair.low, low_face.face_id, 2, 3, 0.5)

    assert group.face_id == low_face.face_id
    assert [sample.index for sample in group.samples] == list(range(6))
    expected = _sample_uv_grid(low_face, 2, 3)
    actual = [(sample.value.u, sample.value.v) for sample in group.samples]
    assert np.allclose(np.array(actual), np.array(expected))


def test_native_shape_like_inputs_accept_face_lists_for_sampling_and_mapping():
    pair = identity_box_pair()
    low_faces = list(pair.low.faces())
    high_faces = list(pair.high.faces())

    face_info = describe_shape_faces(low_faces)
    assert len(face_info) == len(low_faces)

    low_face = face_info[0]
    samples = _sample_uv_grid(low_faces[0], 2, 2)
    context = MappingContext()
    context.enable_parallel = True

    group = sample_shape_face_uniform_uv_grid(low_faces, low_face.face_id, 2, 2, 0.5)
    assert group.face_id == low_face.face_id
    assert len(group.samples) == 4

    batch = map_shape_single_low_face_samples_to_high_faces_nearest(
        low_faces,
        high_faces,
        low_face.face_id,
        samples,
        context,
    )
    assert len(batch.results) == len(samples)
    assert all(result.value.low_face_id == low_face.face_id for result in batch.results)


def test_native_uniform_uv_tolerance_sampler_generates_face_group_in_cpp():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]

    group = sample_shape_face_uniform_uv_tolerance_grid(pair.low, low_face.face_id, 0.5, 0.5)

    assert group.face_id == low_face.face_id
    assert len(group.samples) > 0
    assert [sample.index for sample in group.samples] == list(range(len(group.samples)))


def test_native_high_face_normal_evaluation_and_full_pipeline_smoke():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = _sample_uv_grid(low_face, 2, 2)
    context = MappingContext()
    context.enable_parallel = True

    native_surface_batch = evaluate_shape_single_high_face_samples(pair.high, low_face.face_id, samples, context)
    surface_batch = SurfaceEvalResultBatch.from_native(native_surface_batch)
    assert len(surface_batch.results) == len(samples)
    assert all(result.value.normal_defined for result in surface_batch.results)
    assert all(hasattr(result.value, "uv") and hasattr(result.value, "point") and hasattr(result.value, "normal") for result in surface_batch.results)
    assert surface_batch.to_numpy_uv_array().shape == (len(samples), 2)
    assert surface_batch.to_numpy_normal_array().shape == (len(samples), 3)
    assert surface_batch.to_numpy_point_array().shape == (len(samples), 3)
    assert surface_batch.to_numpy_normal_defined_mask().shape == (len(samples),)

    native_mapping_batch = map_shape_single_low_face_samples_to_high_faces(
        pair.low,
        pair.high,
        low_face.face_id,
        samples,
        context,
    )
    mapping_batch = MappingResultBatch.from_native(native_mapping_batch)
    mapped_surface_batch = SurfaceEvalResultBatch.from_native(
        evaluate_shape_multiple_high_face_samples(pair.high, native_mapping_batch)
    )
    assert len(mapped_surface_batch.results) == len(samples)
    assert all(result.value.normal_defined for result in mapped_surface_batch.results)

    low_face_batch = normalize_face_uv_samples({low_face.face_id: samples})

    native_combined_batch = map_and_evaluate_shape_multiple_low_face_samples(
        pair.low,
        pair.high,
        low_face_batch,
        context,
    )
    combined_batch = MappedSampleBatch.from_native(native_combined_batch)
    assert len(combined_batch.records) == len(samples)
    assert all(record.value.surface.normal_defined for record in combined_batch.records)
    assert combined_batch.to_numpy_low_uv_array().shape == (len(samples), 2)
    assert combined_batch.to_numpy_high_uv_array().shape == (len(samples), 2)
    assert combined_batch.to_numpy_point_array().shape == (len(samples), 3)
    assert combined_batch.to_numpy_normal_array().shape == (len(samples), 3)
    assert combined_batch.to_numpy_status_array().shape == (len(samples),)


def test_native_high_face_evaluation_accepts_numpy_uv_arrays():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    uv_array = np.array([[0.25, 0.25], [0.75, 0.75]], dtype=np.float64)

    native_surface_batch = evaluate_shape_single_high_face_samples(pair.high, low_face.face_id, uv_array)
    surface_batch = SurfaceEvalResultBatch.from_native(native_surface_batch)

    assert len(surface_batch.results) == 2
    assert surface_batch.to_numpy_uv_array().shape == (2, 2)
    assert surface_batch.to_numpy_point_array().shape == (2, 3)
    assert surface_batch.to_numpy_normal_array().shape == (2, 3)
    assert surface_batch.to_numpy_normal_defined_mask().shape == (2,)


def test_native_ray_mapping_smoke_on_ribbed_pedestal():
    pair = pedestal_ribs_pair()
    faces = pair.low.faces()
    side_face_index = None
    for idx, face in enumerate(faces):
        center = face.center()
        normal = face.normal_at(center)
        if abs(normal.Z) < 0.5:
            side_face_index = idx
            break

    assert side_face_index is not None

    low_face = describe_shape_faces(pair.low)[side_face_index]
    samples = _sample_uv_grid(low_face, 4, 4)
    context = MappingContext()
    context.enable_parallel = True

    batch = map_shape_single_low_face_samples_to_high_faces_ray(
        pair.low,
        pair.high,
        low_face.face_id,
        samples,
        context,
    )

    assert len(batch.results) == len(samples)
    assert [result.index for result in batch.results] == list(range(len(samples)))
    assert any(_status_name(result.value.status) == "hit" for result in batch.results)
