import numpy as np

from cad_uv_map import (
    FaceUvSampleGroupBatch,
    MappingMethod,
    MappingStatus,
    describe_shape_faces,
    evaluate_multiple_face_samples,
    evaluate_single_face_samples,
    map_and_evaluate_source_samples_to_target,
    map_source_samples_to_target,
    map_source_uv_grid_to_target,
    mapping_batch_to_structured_array,
    normalize_face_uv_samples,
    sample_shape_face_uniform_uv_grid,
    sample_shape_face_uniform_uv_tolerance_grid,
    UvCoord,
)
from cad_uv_map.api import MappingContext
from tests.fixtures.cad_cases import identity_box_pair, pedestal_ribs_pair

HIT = MappingStatus.hit.value


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


def test_native_single_low_face_mapping_identity_box_returns_zero_distance_hits():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = _sample_uv_grid(low_face, 9, 9)
    context = MappingContext()
    context.enable_parallel = True

    batch = map_source_samples_to_target(pair.low, pair.high, low_face.face_id, samples, MappingMethod.nearest, context)

    assert len(batch) == len(samples)
    assert batch.index.tolist() == list(range(len(samples)))
    assert np.all(batch.status == HIT)
    assert np.all(batch.low_face_id == low_face.face_id)
    assert np.all(batch.high_face_id == low_face.face_id)
    assert np.allclose(batch.high_uv, batch.low_uv)
    assert np.allclose(batch.distance, 0.0)


def test_native_single_low_face_uv_grid_mapping_returns_structured_numpy_grid():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    samples = np.array(_sample_uv_grid(low_face, 2, 2), dtype=np.float64).reshape(2, 2, 2)
    context = MappingContext()
    context.enable_parallel = True

    mapped = map_source_uv_grid_to_target(pair.low, pair.high, low_face.face_id, samples, MappingMethod.nearest, context)

    assert mapped.shape == (2, 2)
    assert mapped.dtype.names == ("high_face_id", "high_u", "high_v")
    assert np.all(mapped["high_face_id"] == low_face.face_id)
    assert np.allclose(mapped["high_u"], samples[..., 0])
    assert np.allclose(mapped["high_v"], samples[..., 1])


def test_python_conversion_helpers_keep_grouped_face_samples_and_columnar_batches_usable():
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
    mapping = map_source_samples_to_target(pair.low, pair.high, low_face.face_id, samples, MappingMethod.nearest, context)

    # Columnar per-field access.
    assert mapping.high_uv.shape == (len(samples), 2)
    assert mapping.low_uv.shape == (len(samples), 2)
    assert mapping.high_face_id.shape == (len(samples),)
    assert mapping.point.shape == (len(samples), 3)

    # One structured array, and a flat row view.
    structured = mapping_batch_to_structured_array(mapping)
    assert structured.shape == (len(samples),)
    assert structured.dtype.names == (
        "index",
        "low_face_id",
        "low_uv",
        "high_face_id",
        "high_uv",
        "point",
        "distance",
        "status",
    )
    row = mapping.row(0)
    assert row.high_face_id == mapping.high_face_id[0]
    assert np.allclose(row.high_uv, mapping.high_uv[0])

    # to_dict exposes the same arrays (zero-copy).
    assert mapping.to_dict()["high_uv"] is mapping.high_uv


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
    samples = _sample_uv_grid(low_face, 2, 2)
    context = MappingContext()
    context.enable_parallel = True

    group = sample_shape_face_uniform_uv_grid(low_faces, low_face.face_id, 2, 2, 0.5)
    assert group.face_id == low_face.face_id
    assert len(group.samples) == 4

    batch = map_source_samples_to_target(low_faces, high_faces, low_face.face_id, samples, MappingMethod.nearest, context)
    assert len(batch) == len(samples)
    assert np.all(batch.low_face_id == low_face.face_id)


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

    surface_batch = evaluate_single_face_samples(pair.high, low_face.face_id, samples, context)
    assert len(surface_batch) == len(samples)
    assert np.all(surface_batch.normal_defined)
    assert surface_batch.uv.shape == (len(samples), 2)
    assert surface_batch.normal.shape == (len(samples), 3)
    assert surface_batch.point.shape == (len(samples), 3)
    assert surface_batch.normal_defined.shape == (len(samples),)

    mapping_batch = map_source_samples_to_target(pair.low, pair.high, low_face.face_id, samples, MappingMethod.nearest, context)
    mapped_surface_batch = evaluate_multiple_face_samples(pair.high, mapping_batch)
    assert len(mapped_surface_batch) == len(samples)
    assert np.all(mapped_surface_batch.normal_defined)

    low_face_batch = normalize_face_uv_samples({low_face.face_id: samples})

    combined = map_and_evaluate_source_samples_to_target(
        pair.low, pair.high, low_face_batch, MappingMethod.nearest, context
    )
    assert len(combined) == len(samples)
    assert np.all(combined.surface.normal_defined)
    assert combined.mapping.low_uv.shape == (len(samples), 2)
    assert combined.mapping.high_uv.shape == (len(samples), 2)
    assert combined.surface.point.shape == (len(samples), 3)
    assert combined.surface.normal.shape == (len(samples), 3)
    assert combined.mapping.status.shape == (len(samples),)
    # Composed row pulls from both stages.
    assert np.allclose(combined.row(0).surface.normal, combined.surface.normal[0], equal_nan=True)


def test_native_high_face_evaluation_accepts_numpy_uv_arrays():
    pair = identity_box_pair()
    low_face = describe_shape_faces(pair.low)[0]
    uv_array = np.array([[0.25, 0.25], [0.75, 0.75]], dtype=np.float64)

    surface_batch = evaluate_single_face_samples(pair.high, low_face.face_id, uv_array)

    assert len(surface_batch) == 2
    assert surface_batch.uv.shape == (2, 2)
    assert surface_batch.point.shape == (2, 3)
    assert surface_batch.normal.shape == (2, 3)
    assert surface_batch.normal_defined.shape == (2,)


def test_merged_method_arg_routes_to_separate_cpp_impls_on_pedestal():
    pair = pedestal_ribs_pair()
    faces = pair.low.faces()
    side_face_index = None
    for idx, face in enumerate(faces):
        if abs(face.normal_at(face.center()).Z) < 0.5:
            side_face_index = idx
            break
    assert side_face_index is not None

    low_face = describe_shape_faces(pair.low)[side_face_index]
    samples = _sample_uv_grid(low_face, 4, 4)
    context = MappingContext()
    context.enable_parallel = True

    nearest = map_source_samples_to_target(pair.low, pair.high, low_face.face_id, samples, MappingMethod.nearest, context)
    ray = map_source_samples_to_target(pair.low, pair.high, low_face.face_id, samples, MappingMethod.ray, context)

    assert len(nearest) == len(samples)
    assert len(ray) == len(samples)
    assert ray.index.tolist() == list(range(len(samples)))

    # nearest collapses onto the smooth base face; ray reaches the raised ribs.
    nearest_faces = set(nearest.high_face_id.tolist())
    ray_faces = set(ray.high_face_id.tolist())
    assert nearest_faces != ray_faces
    assert any(ray.status == HIT)
