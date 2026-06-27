#pragma once

#include <cstdint>
#include <vector>

#include "cad_uv_map/indexed_record.hpp"

namespace cad_uv_map {

/*
 * SurfaceEvalResult stores the result of evaluating a face at one UV point.
 *
 * This is the second stage in the pipeline: once a sample has a target face and
 * UV pair, the evaluator can recover point and normal data from OCCT.
 */
struct SurfaceEvalResult {
    std::int32_t face_id;
    double u;
    double v;

    double point_x;
    double point_y;
    double point_z;

    double normal_x;
    double normal_y;
    double normal_z;

    bool normal_defined;
};

using IndexedSurfaceEvalResult = IndexedRecord<SurfaceEvalResult>;

/*
 * SurfaceEvalBatch is the batched output from the UV-to-normal stage.
 */
struct SurfaceEvalBatch {
    IndexedRecords<SurfaceEvalResult> results;
};

} // namespace cad_uv_map
