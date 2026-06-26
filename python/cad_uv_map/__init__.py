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


__all__ = [
    "describe_brep_faces",
    "describe_shape_faces",
    "debug_print_brep_faces",
    "debug_print_shape_faces",
]
