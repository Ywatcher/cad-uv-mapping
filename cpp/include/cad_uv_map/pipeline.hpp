#pragma once

#include "cad_uv_map/indexed_record.hpp"
#include "cad_uv_map/mapping_context.hpp"
#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/sample.hpp"
#include "cad_uv_map/surface_eval.hpp"

#include <TopoDS_Face.hxx>
#include <vector>

namespace cad_uv_map {

/*
 * MappedSampleRecord keeps the source sample, the chosen high-side mapping, and the
 * downstream surface evaluation together.
 */
struct MappedSampleRecord {
    FlatUvSample sample;
    MappingResult mapping;
    SurfaceEvalResult surface;
};

using IndexedMappedSampleRecord = IndexedRecord<MappedSampleRecord>;

/*
 * MappedSampleBatch is the combined output of the full two-stage flow.
 */
struct MappedSampleBatch {
    IndexedRecords<MappedSampleRecord> records;
};

/*
 * Full pipeline convenience API.
 *
 * This is a thin orchestration layer. It exists for callers that want the full
 * low-sample -> mapping -> surface evaluation flow in one call, while keeping
 * the stage-specific functions available for direct inspection and testing.
 */
MappedSampleBatch map_and_evaluate_multiple_low_face_samples(
    const FaceUvSampleGroupBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context = nullptr);

} // namespace cad_uv_map
