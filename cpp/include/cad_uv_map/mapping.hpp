#pragma once

#include "cad_uv_map/face_info.hpp"
#include "cad_uv_map/indexed_record.hpp"
#include "cad_uv_map/mapping_context.hpp"
#include "cad_uv_map/occt_io.hpp"
#include "cad_uv_map/sample.hpp"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <string>
#include <vector>

namespace cad_uv_map {

/*
 * MappingStatus is the per-sample outcome of low-to-high projection.
 */
enum class MappingStatus : std::uint8_t {
    hit,
    no_hit,
    ambiguous,
    outside_trim,
    failed,
};

/*
 * MappingMethod selects the low-to-high projection strategy.
 *
 * The per-method implementations live as separate functions in
 * cpp/src/projection/; the public entry points below dispatch on this value.
 */
enum class MappingMethod : std::uint8_t {
    nearest,
    ray,
};

/*
 * MappingResult stores the selected high-side correspondence for one sample.
 *
 * The sample is treated independently from the batch, so high_face_id may vary
 * from sample to sample even within the same low face.
 */
struct MappingResult {
    std::int32_t low_face_id;
    UvCoord low_uv;

    std::int32_t high_face_id;
    UvCoord high_uv;

    Vec3 point;

    double distance;
    MappingStatus status;
};

using IndexedMappingResult = IndexedRecord<MappingResult>;

/*
 * MappingResultBatch is the batched output from the low-to-high mapping stage.
 */
struct MappingResultBatch {
    IndexedRecords<MappingResult> results;
};

/*
 * Public mapping API.
 *
 * These declarations are the native entry points that are intended to be
 * reachable from Python through pybind11. The implementation may use internal
 * helpers, but callers should treat these as the supported surfaces.
 */

/*
 * Core single-face mapping entry point.
 *
 * This is the unit of work that does the real projection logic. The projection
 * method is selected by `method`; the per-method implementations live as
 * separate functions in cpp/src/projection/ and are dispatched here. The
 * multi-face wrapper reuses this function rather than duplicating it.
 */
MappingResultBatch map_single_low_face_samples_to_high_faces(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    MappingMethod method = MappingMethod::nearest,
    const MappingContext* shared_context = nullptr);

/*
 * BREP-bytes single-face mapping entry point.
 *
 * Reads both shapes from BREP bytes, extracts faces, validates the low face id,
 * then dispatches to the projection method selected by `method`.
 */
MappingResultBatch map_brep_single_low_face_samples_to_high_faces(
    const std::string& low_brep_data,
    const std::string& high_brep_data,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    MappingMethod method = MappingMethod::nearest,
    const MappingContext* shared_context = nullptr);

/*
 * Multi-face wrapper for the single-face core.
 *
 * The intended default implementation is to call the single-face function once
 * per low face. If we later add parallelism, it belongs here, not in the core
 * mapping logic.
 */
MappingResultBatch map_multiple_low_face_sample_groups_to_high_faces(
    const FaceUvSampleGroupBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    MappingMethod method = MappingMethod::nearest,
    const MappingContext* shared_context = nullptr);

/*
 * Debug and inspection helpers.
 *
 * These functions are intentionally lightweight inspection tools. They are
 * useful for validating the Python-to-C++ input bridge and for debugging sample
 * grouping, but they are not part of the core mapping algorithm itself.
 */
void debug_print_shape_uv_sample_batch(
    const TopoDS_Shape& shape,
    const FaceUvSampleGroupBatch& samples,
    const std::string& label);

void debug_print_brep_uv_sample_batch(
    const std::string& brep_data,
    const FaceUvSampleGroupBatch& samples,
    const std::string& label);

void debug_print_shape_uv_samples(
    const TopoDS_Shape& shape,
    const std::vector<FlatUvSample>& samples,
    const std::string& label);

} // namespace cad_uv_map
