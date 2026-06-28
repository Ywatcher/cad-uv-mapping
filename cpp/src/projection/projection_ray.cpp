#include "mapping_projection.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>

namespace cad_uv_map::detail {

namespace {

MappingResult map_low_face_sample_to_high_faces_ray(
    const BRepAdaptor_Surface& low_surface,
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const UvCoord& uv,
    const std::vector<TopoDS_Face>& high_faces,
    double tolerance) {
    gp_Pnt origin;
    gp_Dir direction(0.0, 0.0, 1.0);

    if (!build_low_face_ray(low_surface, low_face, uv, tolerance, &origin, &direction)) {
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

    for (std::int32_t high_face_id = 0; high_face_id < static_cast<std::int32_t>(high_faces.size()); ++high_face_id) {
        try {
            const ProjectionCandidate candidate = project_ray_to_face(
                origin,
                direction,
                high_face_id,
                high_faces[static_cast<std::size_t>(high_face_id)],
                tolerance);

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

    auto map_sample = [&](const BRepAdaptor_Surface& low_surface, std::size_t sample_index) {
        batch.results[sample_index] = IndexedMappingResult{
            sample_index,
            map_low_face_sample_to_high_faces_ray(
                low_surface,
                low_face,
                low_face_id,
                low_uv_samples[sample_index],
                high_faces,
                tolerance),
        };
    };

    if (worker_count <= 1) {
        BRepAdaptor_Surface low_surface(low_face);
        for (std::size_t sample_index = 0; sample_index < low_uv_samples.size(); ++sample_index) {
            map_sample(low_surface, sample_index);
        }
        return batch;
    }

    const std::size_t chunk_size = (low_uv_samples.size() + worker_count - 1) / worker_count;
    std::vector<std::future<void>> futures;
    futures.reserve(worker_count);

    for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
        const std::size_t begin = worker_index * chunk_size;
        if (begin >= low_uv_samples.size()) {
            break;
        }

        const std::size_t end = std::min(low_uv_samples.size(), begin + chunk_size);
        futures.push_back(std::async(std::launch::async, [&, begin, end]() {
            BRepAdaptor_Surface low_surface(low_face);
            for (std::size_t sample_index = begin; sample_index < end; ++sample_index) {
                map_sample(low_surface, sample_index);
            }
        }));
    }

    for (std::future<void>& future : futures) {
        future.get();
    }

    return batch;
}

} // namespace cad_uv_map::detail
