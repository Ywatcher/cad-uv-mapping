#pragma once

#include <cstdint>
#include <string>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <vector>

namespace cad_uv_map {

/*
 * FaceInfo is a lightweight, Python-facing summary of a TopoDS_Face.
 *
 * The real CAD geometry remains in TopoDS_Shape/TopoDS_Face, including the
 * underlying Geom_Surface and trimming information. FaceInfo only exposes
 * basic metadata, such as the face index and UV parameter bounds, for inspection
 * and lightweight data exchange with Python.
 *
 * It is not enough for surface evaluation, normal computation, projection, or
 * final UV mapping; those operations must use the stored TopoDS_Face/TopoDS_Shape.
 * FIXME (currently for debug use?)
 */
struct FaceInfo { 
    std::int32_t face_id;
    double u_min;
    double u_max;
    double v_min;
    double v_max;
};
/*
 * Summary functions
 */
std::vector<FaceInfo> describe_brep_faces(const std::string& brep_path);
std::vector<FaceInfo> describe_brep_bytes(const std::string& brep_data);
std::vector<FaceInfo> describe_faces(const std::vector<TopoDS_Face>& faces);
/*
 * 	debug functions
 * 	with these functions you can print out information
 * 	from the faces to stdout, in order to check whether
 * 	you have read these data correctly
 */
void debug_print_shape_faces(const TopoDS_Shape& shape, const std::string& label);
void debug_print_brep_faces(const std::string& brep_path);
void debug_print_brep_bytes(const std::string& brep_data, const std::string& label);
void debug_print_faces(const std::vector<TopoDS_Face>& faces, const std::string& label);

} // namespace cad_uv_map
