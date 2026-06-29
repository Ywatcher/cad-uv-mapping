from ._native import MappingContext, MappingMethod, MappingStatus
from .types import (
    FaceUvSampleGroup,
    FaceUvSampleGroupBatch,
    FlatUvSample,
    IndexedFaceUvSampleGroup,
    IndexedFlatUvSample,
    IndexedUvCoord,
    UvCoord,
    Vec3,
)
from .results import (
    MappedSampleBatch,
    MappingResultBatch,
    SurfaceEvalResultBatch,
)


def describe_brep_faces(*args, **kwargs):
    from .api import describe_brep_faces as _describe_brep_faces

    return _describe_brep_faces(*args, **kwargs)


def shape_to_brep_bytes(*args, **kwargs):
    from .api import shape_to_brep_bytes as _shape_to_brep_bytes

    return _shape_to_brep_bytes(*args, **kwargs)


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


def debug_print_shape_uv_sample_batch(*args, **kwargs):
    from .api import debug_print_shape_uv_sample_batch as _debug_print_shape_uv_sample_batch

    return _debug_print_shape_uv_sample_batch(*args, **kwargs)


def debug_print_shape_uv_samples(*args, **kwargs):
    from .api import debug_print_shape_uv_samples as _debug_print_shape_uv_samples

    return _debug_print_shape_uv_samples(*args, **kwargs)


def sample_shape_face_uniform_uv_grid(*args, **kwargs):
    from .api import sample_shape_face_uniform_uv_grid as _sample_shape_face_uniform_uv_grid

    return _sample_shape_face_uniform_uv_grid(*args, **kwargs)


def sample_shape_face_uniform_uv_grid_batch(*args, **kwargs):
    from .api import sample_shape_face_uniform_uv_grid_batch as _sample_shape_face_uniform_uv_grid_batch

    return _sample_shape_face_uniform_uv_grid_batch(*args, **kwargs)


def sample_shape_face_uniform_uv_tolerance_grid(*args, **kwargs):
    from .api import sample_shape_face_uniform_uv_tolerance_grid as _sample_shape_face_uniform_uv_tolerance_grid

    return _sample_shape_face_uniform_uv_tolerance_grid(*args, **kwargs)


def sample_shape_face_uniform_uv_tolerance_grid_batch(*args, **kwargs):
    from .api import sample_shape_face_uniform_uv_tolerance_grid_batch as _sample_shape_face_uniform_uv_tolerance_grid_batch

    return _sample_shape_face_uniform_uv_tolerance_grid_batch(*args, **kwargs)


def map(*args, **kwargs):
    """Merged low-to-high mapping; select the method with `method=MappingMethod...`."""
    from .api import map as _map

    return _map(*args, **kwargs)


def map_grid(*args, **kwargs):
    """Merged UV-grid mapping; select the method with `method=MappingMethod...`."""
    from .api import map_grid as _map_grid

    return _map_grid(*args, **kwargs)


def evaluate_shape_single_high_face_samples(*args, **kwargs):
    from .api import evaluate_shape_single_high_face_samples as _evaluate_shape_single_high_face_samples

    return _evaluate_shape_single_high_face_samples(*args, **kwargs)


def evaluate_shape_multiple_high_face_samples(*args, **kwargs):
    from .api import evaluate_shape_multiple_high_face_samples as _evaluate_shape_multiple_high_face_samples

    return _evaluate_shape_multiple_high_face_samples(*args, **kwargs)


def map_and_evaluate_shape_multiple_low_face_samples(*args, **kwargs):
    from .api import map_and_evaluate_shape_multiple_low_face_samples as _map_and_evaluate_shape_multiple_low_face_samples

    return _map_and_evaluate_shape_multiple_low_face_samples(*args, **kwargs)


__all__ = [
    # enums / config
    "MappingContext",
    "MappingMethod",
    "MappingStatus",
    # input-side types
    "UvCoord",
    "Vec3",
    "IndexedUvCoord",
    "FlatUvSample",
    "IndexedFlatUvSample",
    "FaceUvSampleGroup",
    "IndexedFaceUvSampleGroup",
    "FaceUvSampleGroupBatch",
    # columnar result batches
    "MappingResultBatch",
    "SurfaceEvalResultBatch",
    "MappedSampleBatch",
    # functions
    "describe_brep_faces",
    "describe_shape_faces",
    "shape_to_brep_bytes",
    "debug_print_brep_faces",
    "debug_print_shape_faces",
    "normalize_face_uv_samples",
    "to_native_uv_coord",
    "to_native_uv_coords",
    "to_native_face_uv_samples",
    "mapping_batch_to_structured_array",
    "debug_print_shape_uv_sample_batch",
    "debug_print_shape_uv_samples",
    "sample_shape_face_uniform_uv_grid",
    "sample_shape_face_uniform_uv_grid_batch",
    "sample_shape_face_uniform_uv_tolerance_grid",
    "sample_shape_face_uniform_uv_tolerance_grid_batch",
    "map",
    "map_grid",
    "evaluate_shape_single_high_face_samples",
    "evaluate_shape_multiple_high_face_samples",
    "map_and_evaluate_shape_multiple_low_face_samples",
]
