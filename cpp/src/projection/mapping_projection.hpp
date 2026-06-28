#pragma once

#include "cad_uv_map/mapping.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace cad_uv_map::detail {

/*
 * ProjectionCandidate is an internal scratch record used by the projection
 * helpers and by the nearest and ray mapping methods.
 *
 * It stores one projected hit on a high face so the mapper can compare
 * candidate faces, keep the nearest or first valid ray hit, and then copy that
 * choice into the public MappingResult structure.
 */
struct ProjectionCandidate {
    std::int32_t high_face_id;
    double high_u;
    double high_v;
    gp_Pnt point;
    double distance;
};

/*
 * Shared projection helpers.
 *
 * These functions are used by both projection methods. mapping.cpp does not
 * call them directly; instead, the nearest and ray method files use them to
 * build per-sample MappingResult records.
 */
double mapping_tolerance(const MappingContext* shared_context);

std::size_t mapping_worker_count(std::size_t sample_count, const MappingContext* shared_context);

ProjectionCandidate project_point_to_face(
    const gp_Pnt& point,
    std::int32_t high_face_id,
    const TopoDS_Face& high_face,
    double tolerance);

bool build_low_face_ray(
    const BRepAdaptor_Surface& low_surface,
    const TopoDS_Face& low_face,
    const UvCoord& uv,
    double tolerance,
    gp_Pnt* origin,
    gp_Dir* direction);

ProjectionCandidate project_ray_to_face(
    const gp_Pnt& origin,
    const gp_Dir& direction,
    std::int32_t high_face_id,
    const TopoDS_Face& high_face,
    double tolerance);

MappingResult make_no_hit_result(std::int32_t low_face_id, const UvCoord& uv);

MappingResult make_hit_result(
    std::int32_t low_face_id,
    const UvCoord& uv,
    const ProjectionCandidate& best,
    bool ambiguous);

/*
 * Projection method entry points.
 *
 * These are the internal methods exposed to the map-projection layer. The
 * public mapping API in cpp/include/cad_uv_map/mapping.hpp calls these through
 * mapping.cpp, which keeps orchestration separate from geometry logic.
 */
MappingResultBatch map_low_face_samples_to_high_faces_nearest_impl(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context);

MappingResultBatch map_low_face_samples_to_high_faces_ray_impl(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context);

} // namespace cad_uv_map::detail
