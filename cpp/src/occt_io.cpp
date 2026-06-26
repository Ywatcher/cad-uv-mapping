#include "cad_uv_map/occt_io.hpp"

#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <sstream>
#include <stdexcept>

namespace cad_uv_map {

TopoDS_Shape read_brep_file(const std::string& path) {
    BRep_Builder builder;
    TopoDS_Shape shape;

    if (!BRepTools::Read(shape, path.c_str(), builder)) {
        throw std::runtime_error("failed to read BREP file: " + path);
    }

    if (shape.IsNull()) {
        throw std::runtime_error("BREP file produced a null shape: " + path);
    }

    return shape;
}

TopoDS_Shape read_brep_bytes(const std::string& brep_data) {
    BRep_Builder builder;
    TopoDS_Shape shape;
    std::istringstream stream(brep_data);

    BRepTools::Read(shape, stream, builder);

    if (shape.IsNull()) {
        throw std::runtime_error("BREP byte buffer produced a null shape");
    }

    return shape;
}

std::vector<TopoDS_Face> collect_faces(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Face> faces;

    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        faces.push_back(TopoDS::Face(exp.Current()));
    }

    return faces;
}

} // namespace cad_uv_map
