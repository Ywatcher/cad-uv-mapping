#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/occt_io.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <iostream>

namespace cad_uv_map {

namespace {

std::vector<FaceInfo> describe_face_list(const std::vector<TopoDS_Face>& faces) {
    std::vector<FaceInfo> result;
    result.reserve(faces.size());

    for (std::int32_t i = 0; i < static_cast<std::int32_t>(faces.size()); ++i) {
        BRepAdaptor_Surface adaptor(faces[static_cast<std::size_t>(i)]);
        result.push_back(FaceInfo{
            i,
            adaptor.FirstUParameter(),
            adaptor.LastUParameter(),
            adaptor.FirstVParameter(),
            adaptor.LastVParameter(),
        });
    }

    return result;
}

void print_face_list(const std::vector<TopoDS_Face>& faces, const std::string& label) {
    std::cout << "shape: " << label << '\n';
    std::cout << "face_count: " << faces.size() << '\n';

    for (std::int32_t i = 0; i < static_cast<std::int32_t>(faces.size()); ++i) {
        const TopoDS_Face& face = faces[static_cast<std::size_t>(i)];
        BRepAdaptor_Surface adaptor(face);
        std::cout
            << "face " << i
            << " orientation=" << static_cast<int>(face.Orientation())
            << " surface_type=" << static_cast<int>(adaptor.GetType())
            << " u=[" << adaptor.FirstUParameter() << ", " << adaptor.LastUParameter() << "]"
            << " v=[" << adaptor.FirstVParameter() << ", " << adaptor.LastVParameter() << "]"
            << '\n';
    }

    std::cout.flush();
}

} // namespace

std::vector<FaceInfo> describe_brep_faces(const std::string& brep_path) {
    TopoDS_Shape shape = read_brep_file(brep_path);
    return describe_faces(collect_faces(shape));
}

std::vector<FaceInfo> describe_brep_bytes(const std::string& brep_data) {
    TopoDS_Shape shape = read_brep_bytes(brep_data);
    return describe_faces(collect_faces(shape));
}

std::vector<FaceInfo> describe_faces(const std::vector<TopoDS_Face>& faces) {
    return describe_face_list(faces);
}

void debug_print_shape_faces(const TopoDS_Shape& shape, const std::string& label) {
    debug_print_faces(collect_faces(shape), label);
}

void debug_print_brep_faces(const std::string& brep_path) {
    TopoDS_Shape shape = read_brep_file(brep_path);
    debug_print_faces(collect_faces(shape), "BREP " + brep_path);
}

void debug_print_brep_bytes(const std::string& brep_data, const std::string& label) {
    TopoDS_Shape shape = read_brep_bytes(brep_data);
    debug_print_faces(collect_faces(shape), label);
}

void debug_print_faces(const std::vector<TopoDS_Face>& faces, const std::string& label) {
    print_face_list(faces, label);
}

} // namespace cad_uv_map
