#pragma once

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>

namespace cad_uv_map {

TopoDS_Shape read_brep_file(const std::string& path);
TopoDS_Shape read_brep_bytes(const std::string& brep_data);
std::vector<TopoDS_Face> collect_faces(const TopoDS_Shape& shape);

} // namespace cad_uv_map
