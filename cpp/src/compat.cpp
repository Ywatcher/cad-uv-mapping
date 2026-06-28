#include "cad_uv_map/compat.hpp"

#include <algorithm>

namespace cad_uv_map {

FaceUvSampleGroupBatch normalize_flat_uv_samples(const std::vector<FlatUvSample>& samples) {
    FaceUvSampleGroupBatch batch;

    for (std::size_t sample_index = 0; sample_index < samples.size(); ++sample_index) {
        const FlatUvSample& sample = samples[sample_index];
        auto group_it = std::find_if(
            batch.faces.begin(),
            batch.faces.end(),
            [&sample](const FaceUvSampleGroup& group) { return group.face_id == sample.face_id; });

        if (group_it == batch.faces.end()) {
            batch.faces.push_back(FaceUvSampleGroup{sample.face_id, {}});
            group_it = std::prev(batch.faces.end());
        }

        group_it->samples.push_back(IndexedRecord<UvCoord>{sample_index, sample.uv});
    }

    return batch;
}

MappingResultBatch map_multiple_low_face_uv_samples_to_high_faces(
    const std::vector<FlatUvSample>& samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    return map_multiple_low_face_sample_groups_to_high_faces(
        normalize_flat_uv_samples(samples),
        low_faces,
        high_faces,
        shared_context);
}

void debug_print_brep_uv_samples(
    const std::string& brep_data,
    const std::vector<FlatUvSample>& samples,
    const std::string& label) {
    TopoDS_Shape shape = read_brep_bytes(brep_data);
    debug_print_shape_uv_sample_batch(shape, normalize_flat_uv_samples(samples), label);
}

} // namespace cad_uv_map
