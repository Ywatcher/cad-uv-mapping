#pragma once

#include <cstdint>
#include <vector>

#include "cad_uv_map/indexed_record.hpp"

namespace cad_uv_map {

/*
 * UvCoord is a pure native UV coordinate with no face context.
 *
 * This is reusable wherever only parameter-space coordinates matter.
 */
struct UvCoord {
    double u;
    double v;
};

/*
 * UvSample is the input side of the pipeline.
 *
 * It identifies a source face and one native UV coordinate on that face.
 * The sampler can generate these samples from a uniform grid, an adaptive
 * pattern, user selection, or any other policy.
 */
struct UvSample {
    std::int32_t face_id;
    UvCoord uv;
};

using IndexedUvCoord = IndexedRecord<UvCoord>;
using IndexedUvSample = IndexedRecord<UvSample>;

/*
 * UvSampleBatch carries arbitrary sample points for batch processing.
 */
struct UvSampleBatch {
    std::vector<UvSample> samples;
};

/*
 * FaceUvSamples groups arbitrary UV samples under one source face.
 *
 * This is useful for the batch wrapper around the single-face core: each face
 * can be handled independently, then merged into a larger result stream.
 */
struct FaceUvSamples {
    std::int32_t face_id;
    IndexedRecords<UvCoord> samples;
};

using IndexedFaceUvSamples = IndexedRecord<FaceUvSamples>;

/*
 * FaceUvSampleBatch carries all low-face sample groups in one request.
 */
struct FaceUvSampleBatch {
    std::vector<FaceUvSamples> faces;
};

/*
 * UniformUvGrid is a simple fixture-oriented generator description.
 *
 * It is intentionally small and explicit so tests can create repeatable grids
 * without coupling the mapper to a single sampling strategy.
 */
struct UniformUvGrid {
    std::int32_t face_id;
    std::int32_t u_count;
    std::int32_t v_count;
    double margin;
};

} // namespace cad_uv_map
