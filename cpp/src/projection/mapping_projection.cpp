#include "mapping_projection.hpp"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Tool.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <TopAbs.hxx>
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
    const TopoDS_Face& high_face,
    double tolerance) {
    BRepBuilderAPI_MakeVertex vertex_builder(point);
    BRepExtrema_DistShapeShape distance(vertex_builder.Vertex(), high_face, tolerance);
    distance.Perform();

    if (!distance.IsDone() || distance.NbSolution() == 0) {
        throw std::runtime_error("no projection solution");
    }

    const gp_Pnt hit_point = distance.PointOnShape2(1);
    ShapeAnalysis_Surface surface_analysis(BRep_Tool::Surface(high_face));
    const gp_Pnt2d high_uv = surface_analysis.ValueOfUV(hit_point, tolerance);

    return ProjectionCandidate{
        high_face_id,
        high_uv.X(),
        high_uv.Y(),
        hit_point,
        distance.Value(),
    };
}

bool build_low_face_ray(
    const BRepAdaptor_Surface& low_surface,
    const TopoDS_Face& low_face,
    const UvCoord& uv,
    double tolerance,
    gp_Pnt* origin,
    gp_Dir* direction) {
    const gp_Pnt point = low_surface.Value(uv.u, uv.v);
    BRepLProp_SLProps props(low_surface, uv.u, uv.v, 1, tolerance);
    if (!props.IsNormalDefined()) {
        return false;
    }

    gp_Dir ray_direction = props.Normal();
    if (low_face.Orientation() == TopAbs_REVERSED) {
        ray_direction.Reverse();
    }

    *origin = point;
    *direction = ray_direction;
    return true;
}

ProjectionCandidate project_ray_to_face(
    const gp_Pnt& origin,
    const gp_Dir& direction,
    std::int32_t high_face_id,
    const TopoDS_Face& high_face,
    double tolerance) {
    IntCurvesFace_ShapeIntersector intersector;
    intersector.Load(high_face, tolerance);
    intersector.Perform(gp_Lin(origin, direction), -std::numeric_limits<double>::max(), std::numeric_limits<double>::max());

    if (!intersector.IsDone() || intersector.NbPnt() == 0) {
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

    for (Standard_Integer i = 1; i <= intersector.NbPnt(); ++i) {
        const double ray_distance = intersector.WParameter(i);
        if (ray_distance <= tolerance) {
            continue;
        }

        if (!has_best || ray_distance < best.distance - tolerance) {
            best = ProjectionCandidate{
                high_face_id,
                intersector.UParameter(i),
                intersector.VParameter(i),
                intersector.Pnt(i),
                ray_distance,
            };
            has_best = true;
        }
    }

    if (!has_best) {
        throw std::runtime_error("no forward ray intersection");
    }

    return best;
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
