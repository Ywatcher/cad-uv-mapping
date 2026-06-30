#include "mapping_projection.hpp"

#include "../geom/RayFaceIntersector.hpp"
#include "../geom/SurfaceAdaptor.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>

namespace cad_uv_map::detail {

namespace {

bool find_best_ray_hit_allow_zero(
    const gp_Pnt& origin,
    const gp_Dir& direction,
    const std::vector<geom::RayFaceIntersector>& intersectors,
    double tolerance,
    ProjectionCandidate* best,
    bool* ambiguous) {
    bool has_best = false;
    *ambiguous = false;

    const gp_Lin line(origin, direction);
    for (std::int32_t hfi = 0; hfi < static_cast<std::int32_t>(intersectors.size()); ++hfi) {
        if (!intersectors[static_cast<std::size_t>(hfi)].CanRayReach(line)) continue;

        try {
            const ProjectionCandidate candidate = project_ray_to_face_allow_zero(
                origin, direction, hfi, intersectors[static_cast<std::size_t>(hfi)], tolerance);

            if (!has_best || candidate.distance < best->distance - tolerance) {
                *best = candidate;
                has_best = true;
                *ambiguous = false;
            } else if (std::abs(candidate.distance - best->distance) <= tolerance) {
                *ambiguous = true;
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    return has_best;
}

MappingResult map_low_face_sample_to_high_faces_ray(
    const geom::SurfaceAdaptor& adaptor,
    std::int32_t low_face_id,
    const UvCoord& uv,
    const std::vector<geom::RayFaceIntersector>& intersectors,
    double tolerance) {
    gp_Pnt origin;
    gp_Dir direction(0.0, 0.0, 1.0);

    if (!build_low_face_ray(adaptor, uv, tolerance, &origin, &direction)) {
        return make_no_hit_result(low_face_id, uv);
    }

    bool has_best = false;
    bool ambiguous = false;
    ProjectionCandidate best{
        -1,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        gp_Pnt(),
        std::numeric_limits<double>::infinity(),
    };

    const gp_Lin line(origin, direction);
    for (std::int32_t hfi = 0; hfi < static_cast<std::int32_t>(intersectors.size()); ++hfi) {
        if (!intersectors[static_cast<std::size_t>(hfi)].CanRayReach(line)) continue;

        try {
            const ProjectionCandidate candidate = project_ray_to_face(
                origin, direction, hfi, intersectors[static_cast<std::size_t>(hfi)], tolerance);

            if (!has_best || candidate.distance < best.distance - tolerance) {
                best = candidate;
                has_best = true;
                ambiguous = false;
            } else if (std::abs(candidate.distance - best.distance) <= tolerance) {
                ambiguous = true;
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    if (!has_best) {
        return make_no_hit_result(low_face_id, uv);
    }

    return make_hit_result(low_face_id, uv, best, ambiguous);
}

MappingResult map_low_face_sample_to_high_faces_ray_bidirectional(
    const geom::SurfaceAdaptor& adaptor,
    std::int32_t low_face_id,
    const UvCoord& uv,
    const std::vector<geom::RayFaceIntersector>& intersectors,
    double tolerance) {
    gp_Pnt origin;
    gp_Dir outward_direction(0.0, 0.0, 1.0);

    if (!build_low_face_ray(adaptor, uv, tolerance, &origin, &outward_direction)) {
        return make_no_hit_result(low_face_id, uv);
    }

    bool ambiguous = false;
    ProjectionCandidate best{
        -1,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        gp_Pnt(),
        std::numeric_limits<double>::infinity(),
    };

    if (find_best_ray_hit_allow_zero(origin, outward_direction, intersectors, tolerance, &best, &ambiguous)) {
        return make_hit_result(low_face_id, uv, best, ambiguous);
    }

    best = ProjectionCandidate{
        -1,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        gp_Pnt(),
        std::numeric_limits<double>::infinity(),
    };
    const gp_Dir inward_direction = outward_direction.Reversed();
    if (find_best_ray_hit_allow_zero(origin, inward_direction, intersectors, tolerance, &best, &ambiguous)) {
        return make_hit_result(low_face_id, uv, best, ambiguous);
    }

    return make_no_hit_result(low_face_id, uv);
}

// Create one SurfaceAdaptor per worker in serial before launching threads.
std::vector<geom::SurfaceAdaptor> make_worker_adaptors(
    const TopoDS_Face& face,
    std::size_t worker_count,
    std::size_t chunk_size,
    std::size_t sample_count) {
    std::vector<geom::SurfaceAdaptor> adaptors;
    adaptors.reserve(worker_count);
    for (std::size_t wi = 0; wi < worker_count && wi * chunk_size < sample_count; ++wi) {
        adaptors.emplace_back();
        adaptors.back().Load(face);
    }
    return adaptors;
}

// Build one RayFaceIntersector per high face in serial.
// After this, Perform() on each intersector is safe from any number of threads.
std::vector<geom::RayFaceIntersector> make_face_intersectors(
    const std::vector<TopoDS_Face>& high_faces,
    double tolerance) {
    std::vector<geom::RayFaceIntersector> intersectors(high_faces.size());
    for (std::size_t i = 0; i < high_faces.size(); ++i)
        intersectors[i].Load(high_faces[i], tolerance);
    return intersectors;
}

} // namespace

/*
 * Projection method entry point exposed to the map-projection layer.
 *
 * Args:
 * - low_face: the source face that supplies the UV samples
 * - low_face_id: index of the source face in the low-face list
 * - low_uv_samples: UV samples on the source face, in input order
 * - high_faces: candidate target faces
 * - shared_context: optional tolerance and parallelism settings
 *
 * Returns:
 * - MappingResultBatch with one IndexedMappingResult per input sample, preserving
 *   the sample order and storing the selected high-face UV / 3D hit.
 *
 * This is the ray-cast implementation used by the public mapping API through
 * the orchestration layer in cpp/src/mapping.cpp.
 */
MappingResultBatch map_low_face_samples_to_high_faces_ray_impl(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingResultBatch batch;
    const double tolerance = mapping_tolerance(shared_context);
    batch.results.resize(low_uv_samples.size());

    const std::size_t worker_count = mapping_worker_count(low_uv_samples.size(), shared_context);
    const std::size_t chunk_size = (low_uv_samples.size() + worker_count - 1) / worker_count;

    // Pre-build one intersector per high face in serial — Perform() is then
    // safe to call from any number of threads concurrently.
    const std::vector<geom::RayFaceIntersector> face_intersectors =
        make_face_intersectors(high_faces, tolerance);

    auto map_sample = [&](const geom::SurfaceAdaptor& adaptor, std::size_t sample_index) {
        batch.results[sample_index] = IndexedMappingResult{
            sample_index,
            map_low_face_sample_to_high_faces_ray(
                adaptor, low_face_id, low_uv_samples[sample_index],
                face_intersectors, tolerance),
        };
    };

    if (worker_count <= 1) {
        geom::SurfaceAdaptor adaptor;
        adaptor.Load(low_face);
        for (std::size_t sample_index = 0; sample_index < low_uv_samples.size(); ++sample_index)
            map_sample(adaptor, sample_index);
        return batch;
    }

    std::vector<geom::SurfaceAdaptor> worker_adaptors =
        make_worker_adaptors(low_face, worker_count, chunk_size, low_uv_samples.size());

    std::vector<std::future<void>> futures;
    futures.reserve(worker_adaptors.size());

    for (std::size_t wi = 0; wi < worker_adaptors.size(); ++wi) {
        const std::size_t begin = wi * chunk_size;
        const std::size_t end = std::min(low_uv_samples.size(), begin + chunk_size);
        futures.push_back(std::async(std::launch::async, [&, begin, end, wi]() {
            for (std::size_t sample_index = begin; sample_index < end; ++sample_index)
                map_sample(worker_adaptors[wi], sample_index);
        }));
    }

    for (std::future<void>& future : futures) future.get();

    return batch;
}

MappingResultBatch map_low_face_samples_to_high_faces_ray_bidirectional_impl(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingResultBatch batch;
    const double tolerance = mapping_tolerance(shared_context);
    batch.results.resize(low_uv_samples.size());

    const std::size_t worker_count = mapping_worker_count(low_uv_samples.size(), shared_context);
    const std::size_t chunk_size = (low_uv_samples.size() + worker_count - 1) / worker_count;

    const std::vector<geom::RayFaceIntersector> face_intersectors =
        make_face_intersectors(high_faces, tolerance);

    auto map_sample = [&](const geom::SurfaceAdaptor& adaptor, std::size_t sample_index) {
        batch.results[sample_index] = IndexedMappingResult{
            sample_index,
            map_low_face_sample_to_high_faces_ray_bidirectional(
                adaptor, low_face_id, low_uv_samples[sample_index],
                face_intersectors, tolerance),
        };
    };

    if (worker_count <= 1) {
        geom::SurfaceAdaptor adaptor;
        adaptor.Load(low_face);
        for (std::size_t sample_index = 0; sample_index < low_uv_samples.size(); ++sample_index)
            map_sample(adaptor, sample_index);
        return batch;
    }

    std::vector<geom::SurfaceAdaptor> worker_adaptors =
        make_worker_adaptors(low_face, worker_count, chunk_size, low_uv_samples.size());

    std::vector<std::future<void>> futures;
    futures.reserve(worker_adaptors.size());

    for (std::size_t wi = 0; wi < worker_adaptors.size(); ++wi) {
        const std::size_t begin = wi * chunk_size;
        const std::size_t end = std::min(low_uv_samples.size(), begin + chunk_size);
        futures.push_back(std::async(std::launch::async, [&, begin, end, wi]() {
            for (std::size_t sample_index = begin; sample_index < end; ++sample_index)
                map_sample(worker_adaptors[wi], sample_index);
        }));
    }

    for (std::future<void>& future : futures) future.get();

    return batch;
}

} // namespace cad_uv_map::detail
