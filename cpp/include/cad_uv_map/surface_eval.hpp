#pragma once

#include <cstdint>
#include <vector>

#include "cad_uv_map/indexed_record.hpp"
#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/sample.hpp"

#include <TopoDS_Face.hxx>

namespace cad_uv_map {

/*
 * SurfaceEvalResult stores the result of evaluating a face at one UV point.
 *
 * This is the second stage in the pipeline: once a sample has a target face and
 * UV pair, the evaluator can recover point and normal data from OCCT.
 */
struct SurfaceEvalResult {
    std::int32_t face_id;
    UvCoord uv;

    Vec3 point;

    Vec3 normal;

    bool normal_defined;
};

using IndexedSurfaceEvalResult = IndexedRecord<SurfaceEvalResult>;

/*
 * SurfaceEvalResultBatch is the batched output from the UV-to-normal stage.
 */
struct SurfaceEvalResultBatch {
    IndexedRecords<SurfaceEvalResult> results;
};

/*
 * Public surface evaluation API.
 *
 * This is the second stage in the public flow: once a sample has a target
 * high face and UV pair, these functions recover point and normal data from
 * OCCT.
 */
SurfaceEvalResultBatch evaluate_single_high_face_samples(
    const TopoDS_Face& high_face,
    std::int32_t high_face_id,
    const std::vector<UvCoord>& high_uv_samples,
    const MappingContext* shared_context = nullptr);

SurfaceEvalResultBatch evaluate_multiple_high_face_samples(
    const MappingResultBatch& mapping,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context = nullptr);

} // namespace cad_uv_map
