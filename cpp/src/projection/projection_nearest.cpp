#include "mapping_projection.hpp"

#include "../geom/PointFaceProjector.hpp"
#include "../geom/SurfaceAdaptor.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>

namespace cad_uv_map::detail {

namespace {

MappingResult map_low_face_sample_to_high_faces_nearest(
    const geom::SurfaceAdaptor& adaptor,
    std::int32_t low_face_id,
    const UvCoord& uv,
    const std::vector<geom::PointFaceProjector>& projectors,
    double tolerance) {
    const gp_Pnt low_point = adaptor.Value(uv.u, uv.v);

    bool has_best = false;
    bool ambiguous = false;
    double best_dist = std::numeric_limits<double>::infinity();
    ProjectionCandidate best{
        -1,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        gp_Pnt(),
        std::numeric_limits<double>::infinity(),
    };

    for (std::int32_t high_face_id = 0; high_face_id < static_cast<std::int32_t>(projectors.size()); ++high_face_id) {
        // Bbox cull: once we have a best at distance D, any face whose nearest
        // bbox point is farther than D + tolerance cannot improve it.
        if (!projectors[static_cast<std::size_t>(high_face_id)].CanReach(low_point, best_dist + tolerance))
            continue;

        try {
            const ProjectionCandidate candidate = project_point_to_face(
                low_point,
                high_face_id,
                projectors[static_cast<std::size_t>(high_face_id)],
                tolerance);

            if (!has_best || candidate.distance < best.distance - tolerance) {
                best = candidate;
                has_best = true;
                best_dist = best.distance;
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
 * This is the nearest-surface implementation used by the public mapping API
 * through the orchestration layer in cpp/src/mapping.cpp.
 */
static std::vector<geom::PointFaceProjector> make_face_projectors(
    const std::vector<TopoDS_Face>& high_faces, double tolerance) {
    std::vector<geom::PointFaceProjector> projectors;
    projectors.resize(high_faces.size());
    for (std::size_t i = 0; i < high_faces.size(); ++i)
        projectors[i].Load(high_faces[i], tolerance);
    return projectors;
}

MappingResultBatch map_low_face_samples_to_high_faces_nearest_impl(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingResultBatch batch;
    const double tolerance = mapping_tolerance(shared_context);
    batch.results.resize(low_uv_samples.size());

    // Build one projector per high face in serial. After this point all
    // projectors are read-only: Perform() is const and returns by value, so
    // workers can call it concurrently with no shared mutable state.
    const std::vector<geom::PointFaceProjector> projectors =
        make_face_projectors(high_faces, tolerance);

    const std::size_t worker_count = mapping_worker_count(low_uv_samples.size(), shared_context);
    const std::size_t chunk_size = (low_uv_samples.size() + worker_count - 1) / worker_count;

    auto map_sample = [&](const geom::SurfaceAdaptor& adaptor, std::size_t sample_index) {
        batch.results[sample_index] = IndexedMappingResult{
            sample_index,
            map_low_face_sample_to_high_faces_nearest(
                adaptor,
                low_face_id,
                low_uv_samples[sample_index],
                projectors,
                tolerance),
        };
    };

    if (worker_count <= 1) {
        geom::SurfaceAdaptor adaptor;
        adaptor.Load(low_face);
        for (std::size_t sample_index = 0; sample_index < low_uv_samples.size(); ++sample_index) {
            map_sample(adaptor, sample_index);
        }
        return batch;
    }

    // Create one low-face adaptor per worker in serial so each worker has
    // private low-face geometry for Value() calls.
    std::vector<geom::SurfaceAdaptor> worker_adaptors;
    worker_adaptors.reserve(worker_count);
    for (std::size_t wi = 0; wi < worker_count && wi * chunk_size < low_uv_samples.size(); ++wi) {
        worker_adaptors.emplace_back();
        worker_adaptors.back().Load(low_face);
    }

    std::vector<std::future<void>> futures;
    futures.reserve(worker_adaptors.size());

    for (std::size_t wi = 0; wi < worker_adaptors.size(); ++wi) {
        const std::size_t begin = wi * chunk_size;
        const std::size_t end = std::min(low_uv_samples.size(), begin + chunk_size);
        futures.push_back(std::async(std::launch::async, [&, begin, end, wi]() {
            for (std::size_t sample_index = begin; sample_index < end; ++sample_index) {
                map_sample(worker_adaptors[wi], sample_index);
            }
        }));
    }

    for (std::future<void>& future : futures) {
        future.get();
    }

    return batch;
}

} // namespace cad_uv_map::detail
