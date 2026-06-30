#include "mapping_projection.hpp"

#include <Precision.hxx>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

namespace cad_uv_map::detail {

/*
 * Shared projection helpers.
 *
 * These helpers are used by both projection methods below. They are the
 * common geometry and result-construction primitives that keep the nearest and
 * ray implementations small and consistent.
 */
double mapping_tolerance(const MappingContext* shared_context) {
    if (shared_context == nullptr) {
        return 1e-7;
    }
    return shared_context->tolerance;
}

std::size_t mapping_worker_count(std::size_t sample_count, const MappingContext* shared_context) {
    if (shared_context == nullptr || !shared_context->enable_parallel || sample_count < 64) {
        return 1;
    }

    const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    return std::min(sample_count, hardware);
}

ProjectionCandidate project_point_to_face(
    const gp_Pnt& point,
    std::int32_t high_face_id,
    const geom::PointFaceProjector& projector,
    double /*tolerance*/) {
    const geom::PointResult result = projector.Perform(point);

    if (!result.done || result.hits.empty()) {
        throw std::runtime_error("no projection solution");
    }

    const geom::PointHit& hit = result.hits[0];
    return ProjectionCandidate{
        high_face_id,
        hit.u,
        hit.v,
        hit.pt,
        hit.distance,
    };
}

bool build_low_face_ray(
    const geom::SurfaceAdaptor& adaptor,
    const UvCoord& uv,
    double tolerance,
    gp_Pnt* origin,
    gp_Dir* direction) {
    gp_Pnt point;
    gp_Vec du, dv;
    adaptor.D1(uv.u, uv.v, point, du, dv);

    const gp_Vec normal_vec = du.Crossed(dv);
    if (normal_vec.Magnitude() < Precision::Confusion()) {
        return false;
    }

    gp_Dir ray_direction(normal_vec);
    if (adaptor.Orientation() == TopAbs_REVERSED) {
        ray_direction.Reverse();
    }

    *origin = point;
    *direction = ray_direction;
    return true;
}

static ProjectionCandidate project_ray_to_face_impl(
    const gp_Pnt& origin,
    const gp_Dir& direction,
    std::int32_t high_face_id,
    const geom::RayFaceIntersector& intersector,
    double tolerance,
    double minimum_ray_distance) {
    const geom::RayResult result = intersector.Perform(
        gp_Lin(origin, direction),
        -std::numeric_limits<double>::max(),
         std::numeric_limits<double>::max());

    if (!result.done || result.hits.empty()) {
        throw std::runtime_error("no ray intersection");
    }

    bool has_best = false;
    ProjectionCandidate best{
        high_face_id,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        gp_Pnt(),
        std::numeric_limits<double>::infinity(),
    };

    for (const geom::RayHit& hit : result.hits) {
        if (hit.w < minimum_ray_distance) continue;

        const double result_distance = std::abs(hit.w) <= tolerance ? 0.0 : hit.w;
        if (!has_best || result_distance < best.distance - tolerance) {
            best = ProjectionCandidate{high_face_id, hit.u, hit.v, hit.pt, result_distance};
            has_best = true;
        }
    }

    if (!has_best) {
        throw std::runtime_error("no forward ray intersection");
    }

    return best;
}

ProjectionCandidate project_ray_to_face(
    const gp_Pnt& origin,
    const gp_Dir& direction,
    std::int32_t high_face_id,
    const geom::RayFaceIntersector& intersector,
    double tolerance) {
    return project_ray_to_face_impl(origin, direction, high_face_id, intersector, tolerance, tolerance);
}

ProjectionCandidate project_ray_to_face_allow_zero(
    const gp_Pnt& origin,
    const gp_Dir& direction,
    std::int32_t high_face_id,
    const geom::RayFaceIntersector& intersector,
    double tolerance) {
    return project_ray_to_face_impl(origin, direction, high_face_id, intersector, tolerance, -tolerance);
}

MappingResult make_no_hit_result(std::int32_t low_face_id, const UvCoord& uv) {
    return MappingResult{
        low_face_id,
        uv,
        -1,
        UvCoord{std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()},
        Vec3{
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
        },
        std::numeric_limits<double>::infinity(),
        MappingStatus::no_hit,
    };
}

MappingResult make_hit_result(
    std::int32_t low_face_id,
    const UvCoord& uv,
    const ProjectionCandidate& best,
    bool ambiguous) {
    return MappingResult{
        low_face_id,
        uv,
        best.high_face_id,
        UvCoord{best.high_u, best.high_v},
        Vec3{best.point.X(), best.point.Y(), best.point.Z()},
        best.distance,
        ambiguous ? MappingStatus::ambiguous : MappingStatus::hit,
    };
}

} // namespace cad_uv_map::detail
