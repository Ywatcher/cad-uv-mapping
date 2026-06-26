#pragma once

#include <cstdint>
#include <string>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
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
std::vector<FaceInfo> describe_brep_bytes(const std::string& brep_data);
std::vector<FaceInfo> describe_faces(const std::vector<TopoDS_Face>& faces);
void debug_print_shape_faces(const TopoDS_Shape& shape, const std::string& label);
void debug_print_brep_faces(const std::string& brep_path);
void debug_print_brep_bytes(const std::string& brep_data, const std::string& label);
void debug_print_faces(const std::vector<TopoDS_Face>& faces, const std::string& label);

} // namespace cad_uv_map
