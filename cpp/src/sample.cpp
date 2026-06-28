#include "cad_uv_map/sample.hpp"

#include "cad_uv_map/occt_io.hpp"

#include <BRepAdaptor_Surface.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cad_uv_map {

namespace {

FaceUvSampleGroup make_uniform_uv_grid_group(
    const TopoDS_Face& face,
    std::int32_t face_id,
    std::int32_t u_count,
    std::int32_t v_count,
    double margin) {
    if (u_count <= 0 || v_count <= 0) {
        throw std::invalid_argument("u_count and v_count must be positive");
    }

    BRepAdaptor_Surface surface(face);
    const double u_min = surface.FirstUParameter();
    const double u_max = surface.LastUParameter();
    const double v_min = surface.FirstVParameter();
    const double v_max = surface.LastVParameter();

    FaceUvSampleGroup group;
    group.face_id = face_id;
    group.samples.reserve(static_cast<std::size_t>(u_count * v_count));

    std::size_t sample_index = 0;
    for (std::int32_t v_index = 0; v_index < v_count; ++v_index) {
        const double v_t = (static_cast<double>(v_index) + margin) / static_cast<double>(v_count);
        const double v = v_min + (v_max - v_min) * v_t;
        for (std::int32_t u_index = 0; u_index < u_count; ++u_index) {
            const double u_t = (static_cast<double>(u_index) + margin) / static_cast<double>(u_count);
            const double u = u_min + (u_max - u_min) * u_t;
            group.samples.push_back(IndexedRecord<UvCoord>{
                sample_index,
                UvCoord{u, v},
            });
            ++sample_index;
        }
    }

    return group;
}

} // namespace

FaceUvSampleGroup sample_brep_face_uniform_uv_grid(
    const std::string& brep_data,
    const UniformUvGrid& grid) {
    const TopoDS_Shape shape = read_brep_bytes(brep_data);
    const std::vector<TopoDS_Face> faces = collect_faces(shape);
    if (grid.face_id < 0 || grid.face_id >= static_cast<std::int32_t>(faces.size())) {
        throw std::out_of_range("grid.face_id is outside the face list");
    }

    return make_uniform_uv_grid_group(
        faces[static_cast<std::size_t>(grid.face_id)],
        grid.face_id,
        grid.u_count,
        grid.v_count,
        grid.margin);
}

FaceUvSampleGroupBatch sample_brep_face_uniform_uv_grid_batch(
    const std::string& brep_data,
    const std::vector<UniformUvGrid>& grids) {
    const TopoDS_Shape shape = read_brep_bytes(brep_data);
    const std::vector<TopoDS_Face> faces = collect_faces(shape);

    FaceUvSampleGroupBatch batch;
    batch.faces.reserve(grids.size());

    for (const UniformUvGrid& grid : grids) {
        if (grid.face_id < 0 || grid.face_id >= static_cast<std::int32_t>(faces.size())) {
            throw std::out_of_range("grid.face_id is outside the face list");
        }
        batch.faces.push_back(make_uniform_uv_grid_group(
            faces[static_cast<std::size_t>(grid.face_id)],
            grid.face_id,
            grid.u_count,
            grid.v_count,
            grid.margin));
    }

    return batch;
}

namespace {

FaceUvSampleGroup make_uniform_uv_tolerance_grid_group(
    const TopoDS_Face& face,
    std::int32_t face_id,
    double tolerance,
    double margin) {
    if (tolerance <= 0.0) {
        throw std::invalid_argument("tolerance must be positive");
    }

    BRepAdaptor_Surface surface(face);
    const double u_min = surface.FirstUParameter();
    const double u_max = surface.LastUParameter();
    const double v_min = surface.FirstVParameter();
    const double v_max = surface.LastVParameter();

    const double u_span = std::abs(u_max - u_min);
    const double v_span = std::abs(v_max - v_min);

    const std::int32_t u_count = std::max<std::int32_t>(
        1,
        static_cast<std::int32_t>(std::ceil(u_span / tolerance)));
    const std::int32_t v_count = std::max<std::int32_t>(
        1,
        static_cast<std::int32_t>(std::ceil(v_span / tolerance)));

    // Reuse the count-based generator so the output layout stays identical.
    // FIXME: if tolerance should define a different distribution strategy, this
    // is the place to replace the count derivation.
    return make_uniform_uv_grid_group(face, face_id, u_count, v_count, margin);
}

} // namespace

FaceUvSampleGroup sample_brep_face_uniform_uv_tolerance_grid(
    const std::string& brep_data,
    const UniformUvToleranceGrid& grid) {
    const TopoDS_Shape shape = read_brep_bytes(brep_data);
    const std::vector<TopoDS_Face> faces = collect_faces(shape);
    if (grid.face_id < 0 || grid.face_id >= static_cast<std::int32_t>(faces.size())) {
        throw std::out_of_range("grid.face_id is outside the face list");
    }

    return make_uniform_uv_tolerance_grid_group(
        faces[static_cast<std::size_t>(grid.face_id)],
        grid.face_id,
        grid.tolerance,
        grid.margin);
}

FaceUvSampleGroupBatch sample_brep_face_uniform_uv_tolerance_grid_batch(
    const std::string& brep_data,
    const std::vector<UniformUvToleranceGrid>& grids) {
    const TopoDS_Shape shape = read_brep_bytes(brep_data);
    const std::vector<TopoDS_Face> faces = collect_faces(shape);

    FaceUvSampleGroupBatch batch;
    batch.faces.reserve(grids.size());

    for (const UniformUvToleranceGrid& grid : grids) {
        if (grid.face_id < 0 || grid.face_id >= static_cast<std::int32_t>(faces.size())) {
            throw std::out_of_range("grid.face_id is outside the face list");
        }
        batch.faces.push_back(make_uniform_uv_tolerance_grid_group(
            faces[static_cast<std::size_t>(grid.face_id)],
            grid.face_id,
            grid.tolerance,
            grid.margin));
    }

    return batch;
}

} // namespace cad_uv_map
