#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cad_uv_map/indexed_record.hpp"

#include <TopoDS_Face.hxx>

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
 * Vec3 is a compact 3D coordinate/value container.
 *
 * It is used for both points and normals in result records so the API stays
 * structured instead of exposing three unrelated scalars.
 */
struct Vec3 {
    double x;
    double y;
    double z;
};

/*
 * FlatUvSample is the input side of the pipeline.
 *
 * It identifies a source face and one native UV coordinate on that face.
 * The sampler can generate these samples from a uniform grid, an adaptive
 * pattern, user selection, or any other policy.
 */
struct FlatUvSample {
    std::int32_t face_id;
    UvCoord uv;
};

using IndexedUvCoord = IndexedRecord<UvCoord>;
using IndexedFlatUvSample = IndexedRecord<FlatUvSample>;

/*
 * FlatUvSampleBatch carries arbitrary sample points for batch processing.
 */
struct FlatUvSampleBatch {
    std::vector<FlatUvSample> samples;
};

/*
 * FaceUvSampleGroup groups arbitrary UV samples under one source face.
 *
 * This is useful for the batch wrapper around the single-face core: each face
 * can be handled independently, then merged into a larger result stream.
 */
struct FaceUvSampleGroup {
    std::int32_t face_id;
    IndexedRecords<UvCoord> samples;
};

using IndexedFaceUvSampleGroup = IndexedRecord<FaceUvSampleGroup>;

/*
 * FaceUvSampleGroupBatch carries all low-face sample groups in one request.
 */
struct FaceUvSampleGroupBatch {
    std::vector<FaceUvSampleGroup> faces;
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

/*
 * UniformUvToleranceGrid is a tolerance-driven sampling request.
 *
 * The sampler chooses the UV grid density from the face parameter ranges and
 * the requested tolerance, so callers do not need to provide explicit counts.
 * FIXME: if we later need a more adaptive step policy, this can become a more
 * general sampling description.
 */
struct UniformUvToleranceGrid {
    std::int32_t face_id;
    double tolerance;
    double margin;
};

/*
 * UV sampling API.
 *
 * These helpers generate native sample containers from OCCT face geometry so
 * Python callers do not need to synthesize the UV grid themselves.
 */
FaceUvSampleGroup sample_brep_face_uniform_uv_grid(
    const std::string& brep_data,
    const UniformUvGrid& grid);

FaceUvSampleGroupBatch sample_brep_face_uniform_uv_grid_batch(
    const std::string& brep_data,
    const std::vector<UniformUvGrid>& grids);

FaceUvSampleGroup sample_brep_face_uniform_uv_tolerance_grid(
    const std::string& brep_data,
    const UniformUvToleranceGrid& grid);

FaceUvSampleGroupBatch sample_brep_face_uniform_uv_tolerance_grid_batch(
    const std::string& brep_data,
    const std::vector<UniformUvToleranceGrid>& grids);

} // namespace cad_uv_map
