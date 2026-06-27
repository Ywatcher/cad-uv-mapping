#pragma once

#include <cstdint>
#include <vector>

#include "cad_uv_map/indexed_record.hpp"

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
 * MappingResult stores the selected high-side correspondence for one sample.
 *
 * The sample is treated independently from the batch, so high_face_id may vary
 * from sample to sample even within the same low face.
 */
struct MappingResult {
    std::int32_t low_face_id;
    double low_u;
    double low_v;

    std::int32_t high_face_id;
    double high_u;
    double high_v;

    double point_x;
    double point_y;
    double point_z;

    double distance;
    MappingStatus status;
};

using IndexedMappingResult = IndexedRecord<MappingResult>;

/*
 * MappingBatch is the batched output from the low-to-high mapping stage.
 */
struct MappingBatch {
    IndexedRecords<MappingResult> results;
};

} // namespace cad_uv_map
