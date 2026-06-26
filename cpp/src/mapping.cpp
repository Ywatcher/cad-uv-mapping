#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/occt_io.hpp"

#include <BRepAdaptor_Surface.hxx>

namespace cad_uv_map {

std::vector<FaceInfo> describe_brep_faces(const std::string& brep_path) {
    TopoDS_Shape shape = read_brep_file(brep_path);
    std::vector<TopoDS_Face> faces = collect_faces(shape);

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

} // namespace cad_uv_map
