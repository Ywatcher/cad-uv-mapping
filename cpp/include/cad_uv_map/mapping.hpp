#pragma once

#include "cad_uv_map/face_info.hpp"
#include "cad_uv_map/indexed_record.hpp"
#include "cad_uv_map/mapping_context.hpp"
#include "cad_uv_map/mapping_types.hpp"
#include "cad_uv_map/occt_io.hpp"
#include "cad_uv_map/sample.hpp"
#include "cad_uv_map/surface_eval.hpp"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <string>
#include <vector>

namespace cad_uv_map {

/*
 * MappedSampleRecord keeps the source sample, the chosen high-side mapping, and the
 * downstream surface evaluation together.
 */
struct MappedSampleRecord {
    UvSample sample;
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
 * Core single-face mapping entry point.
 *
 * This is the unit of work that should do the real projection logic. The
 * multi-face wrapper should reuse this function rather than duplicating it.
 *
 * TODO: if we later want a pure UV-only variant, expose that separately and
 * keep this as the face-qualified CAD path.
 */
MappingBatch map_low_face_samples_to_high_faces(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context = nullptr);

MappingBatch map_brep_low_face_samples_to_high_faces(
    const std::string& low_brep_data,
    const std::string& high_brep_data,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const MappingContext* shared_context = nullptr);

/*
 * Multi-face wrapper for the single-face core.
 *
 * The intended default implementation is to call the single-face function once
 * per low face. If we later add parallelism, it belongs here, not in the core
 * mapping logic.
 */
MappingBatch map_low_face_sample_groups_to_high_faces(
    const FaceUvSampleBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context = nullptr);

/*
 * Backward-compatible convenience wrapper for flat sample streams.
 *
 * TODO: decide whether this should remain public or be retired once the face
 * grouped API is used everywhere.
 */
MappingBatch map_uv_samples_to_high_faces(
    const std::vector<UvSample>& samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context = nullptr);

/*
 * Debug print helpers for UV sample batches.
 *
 * These functions are meant to validate the Python-to-C++ input bridge before
 * the real mapping algorithm is filled in.
 */
void debug_print_shape_uv_sample_batch(
    const TopoDS_Shape& shape,
    const FaceUvSampleBatch& samples,
    const std::string& label);

void debug_print_brep_uv_sample_batch(
    const std::string& brep_data,
    const FaceUvSampleBatch& samples,
    const std::string& label);

void debug_print_shape_uv_samples(
    const TopoDS_Shape& shape,
    const std::vector<UvSample>& samples,
    const std::string& label);

void debug_print_brep_uv_samples(
    const std::string& brep_data,
    const std::vector<UvSample>& samples,
    const std::string& label);

/*
 * Evaluate normals and hit points for already-mapped high-side UVs.
 *
 * TODO: decide whether to accept MappingBatch only or a richer record stream.
 */
SurfaceEvalBatch evaluate_mapped_high_uvs(
    const MappingBatch& mapping,
    const std::vector<TopoDS_Face>& high_faces);

/*
 * Run the full low-sample -> mapping -> surface evaluation pipeline.
 */
MappedSampleBatch map_and_evaluate_samples(
    const FaceUvSampleBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context = nullptr);

} // namespace cad_uv_map
