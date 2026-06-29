#pragma once

#include "cad_uv_map/mapping.hpp"

namespace cad_uv_map {

/*
 * Compatibility helpers for older flat sample streams.
 *
 * These helpers are intentionally separate from the core mapping API. Keep
 * them only while we still need to bridge Python-side flat collections into
 * the grouped native batch form. TODO: decide later whether this layer should
 * stay public or be removed after all callers move to grouped records.
 */
FaceUvSampleGroupBatch normalize_flat_uv_samples(const std::vector<FlatUvSample>& samples);

MappingResultBatch map_multiple_low_face_uv_samples_to_high_faces(
    const std::vector<FlatUvSample>& samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    MappingMethod method = MappingMethod::nearest,
    const MappingContext* shared_context = nullptr);

void debug_print_brep_uv_samples(
    const std::string& brep_data,
    const std::vector<FlatUvSample>& samples,
    const std::string& label);

} // namespace cad_uv_map
