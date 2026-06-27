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
    "debug_print_shape_uv_sample_batch",
    "debug_print_shape_uv_samples",
    "map_shape_low_face_samples_to_high_faces",
    "map_shape_low_face_uv_grid_to_high_face_uv_grid",
]
