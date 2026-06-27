def describe_brep_faces(*args, **kwargs):
    from .api import describe_brep_faces as _describe_brep_faces

    return _describe_brep_faces(*args, **kwargs)


def debug_print_brep_faces(*args, **kwargs):
    from .api import debug_print_brep_faces as _debug_print_brep_faces

    return _debug_print_brep_faces(*args, **kwargs)


def describe_shape_faces(*args, **kwargs):
    from .api import describe_shape_faces as _describe_shape_faces

    return _describe_shape_faces(*args, **kwargs)


def debug_print_shape_faces(*args, **kwargs):
    from .api import debug_print_shape_faces as _debug_print_shape_faces

    return _debug_print_shape_faces(*args, **kwargs)


def normalize_face_uv_samples(*args, **kwargs):
    from .api import normalize_face_uv_samples as _normalize_face_uv_samples

    return _normalize_face_uv_samples(*args, **kwargs)


def to_native_uv_coord(*args, **kwargs):
    from .conversions import to_native_uv_coord as _to_native_uv_coord

    return _to_native_uv_coord(*args, **kwargs)


def to_native_uv_coords(*args, **kwargs):
    from .conversions import to_native_uv_coords as _to_native_uv_coords

    return _to_native_uv_coords(*args, **kwargs)


def to_native_face_uv_samples(*args, **kwargs):
    from .conversions import to_native_face_uv_samples as _to_native_face_uv_samples

    return _to_native_face_uv_samples(*args, **kwargs)


def mapping_batch_to_structured_array(*args, **kwargs):
    from .api import mapping_batch_to_numpy_structured_array as _mapping_batch_to_structured_array

    return _mapping_batch_to_structured_array(*args, **kwargs)


def mapping_batch_to_numpy_grid(*args, **kwargs):
    from .conversions import mapping_batch_to_numpy_grid as _mapping_batch_to_numpy_grid

    return _mapping_batch_to_numpy_grid(*args, **kwargs)


def debug_print_shape_uv_sample_batch(*args, **kwargs):
    from .api import debug_print_shape_uv_sample_batch as _debug_print_shape_uv_sample_batch

    return _debug_print_shape_uv_sample_batch(*args, **kwargs)


def debug_print_shape_uv_samples(*args, **kwargs):
    from .api import debug_print_shape_uv_samples as _debug_print_shape_uv_samples

    return _debug_print_shape_uv_samples(*args, **kwargs)


def map_shape_low_face_samples_to_high_faces(*args, **kwargs):
    from .api import map_shape_low_face_samples_to_high_faces as _map_shape_low_face_samples_to_high_faces

    return _map_shape_low_face_samples_to_high_faces(*args, **kwargs)


def map_shape_low_face_uv_grid_to_high_face_uv_grid(*args, **kwargs):
    from .api import map_shape_low_face_uv_grid_to_high_face_uv_grid as _map_shape_low_face_uv_grid_to_high_face_uv_grid

    return _map_shape_low_face_uv_grid_to_high_face_uv_grid(*args, **kwargs)


__all__ = [
    "describe_brep_faces",
    "describe_shape_faces",
    "debug_print_brep_faces",
    "debug_print_shape_faces",
    "normalize_face_uv_samples",
    "to_native_uv_coord",
    "to_native_uv_coords",
    "to_native_face_uv_samples",
    "mapping_batch_to_structured_array",
    "mapping_batch_to_numpy_grid",
    "debug_print_shape_uv_sample_batch",
    "debug_print_shape_uv_samples",
    "map_shape_low_face_samples_to_high_faces",
    "map_shape_low_face_uv_grid_to_high_face_uv_grid",
]
