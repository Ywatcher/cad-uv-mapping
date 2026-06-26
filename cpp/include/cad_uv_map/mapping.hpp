#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cad_uv_map {

struct FaceInfo {
    std::int32_t face_id;
    double u_min;
    double u_max;
    double v_min;
    double v_max;
};

std::vector<FaceInfo> describe_brep_faces(const std::string& brep_path);

} // namespace cad_uv_map
