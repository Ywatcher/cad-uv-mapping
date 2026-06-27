#include "cad_uv_map/mapping.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Tool.hxx>
#include <algorithm>
#include <cmath>
#include <future>
#include <iterator>
#include <limits>
#include <ShapeAnalysis_Surface.hxx>
#include <stdexcept>
#include <TopAbs.hxx>
#include <gp_Pnt.hxx>
#include <thread>
#include <iostream>

namespace cad_uv_map {

namespace {

struct ProjectionCandidate {
    std::int32_t high_face_id;
    double high_u;
    double high_v;
    gp_Pnt point;
    double distance;
};

double mapping_tolerance(const MappingContext* shared_context) {
    if (shared_context == nullptr) {
        return 1e-7;
    }
    return shared_context->tolerance;
}

ProjectionCandidate project_point_to_face(
    const gp_Pnt& point,
    std::int32_t high_face_id,
    const TopoDS_Face& high_face,
    double tolerance) {
    // OCCT gives us the distance query and the UV recovery on the target face.
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

MappingResult map_low_face_sample_to_high_faces(
    const BRepAdaptor_Surface& low_surface,
    std::int32_t low_face_id,
    const UvCoord& uv,
    const std::vector<TopoDS_Face>& high_faces,
    double tolerance) {
    // This helper is intentionally sample-local so the parallel wrapper can call
    // it independently for disjoint ranges without sharing mutable state.
    const gp_Pnt low_point = low_surface.Value(uv.u, uv.v);

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
            const ProjectionCandidate candidate = project_point_to_face(
                low_point,
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
        return MappingResult{
            low_face_id,
            uv.u,
            uv.v,
            -1,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            MappingStatus::no_hit,
        };
    }

    return MappingResult{
        low_face_id,
        uv.u,
        uv.v,
        best.high_face_id,
        best.high_u,
        best.high_v,
        best.point.X(),
        best.point.Y(),
        best.point.Z(),
        best.distance,
        ambiguous ? MappingStatus::ambiguous : MappingStatus::hit,
    };
}

std::size_t mapping_worker_count(std::size_t sample_count, const MappingContext* shared_context) {
    if (shared_context == nullptr || !shared_context->enable_parallel || sample_count < 64) {
        return 1;
    }

    // Keep the worker count bounded by hardware, but never exceed the sample count.
    const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    return std::min(sample_count, hardware);
}

FaceUvSampleBatch normalize_flat_uv_samples(const std::vector<UvSample>& samples) {
    FaceUvSampleBatch batch;

    for (std::size_t sample_index = 0; sample_index < samples.size(); ++sample_index) {
        const UvSample& sample = samples[sample_index];
        auto group_it = std::find_if(
            batch.faces.begin(),
            batch.faces.end(),
            [&sample](const FaceUvSamples& group) { return group.face_id == sample.face_id; });

        if (group_it == batch.faces.end()) {
            batch.faces.push_back(FaceUvSamples{sample.face_id, {}});
            group_it = std::prev(batch.faces.end());
        }

        group_it->samples.push_back(IndexedRecord<UvCoord>{sample_index, sample.uv});
    }

    return batch;
}

void print_face_uv_sample_batch(const TopoDS_Shape& shape, const FaceUvSampleBatch& samples, const std::string& label) {
    const std::vector<TopoDS_Face> faces = collect_faces(shape);

    std::cout << "shape: " << label << '\n';
    std::cout << "face_count: " << faces.size() << '\n';
    std::cout << "sample_group_count: " << samples.faces.size() << '\n';

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

    for (std::size_t group_index = 0; group_index < samples.faces.size(); ++group_index) {
        const FaceUvSamples& group = samples.faces[group_index];
        std::cout << "sample_group " << group_index
                  << " face_id=" << group.face_id
                  << " sample_count=" << group.samples.size()
                  << '\n';

        for (const IndexedRecord<UvCoord>& sample : group.samples) {
            std::cout << "  sample index=" << sample.index
                      << " uv=(" << sample.value.u << ", " << sample.value.v << ")"
                      << '\n';
        }
    }

    std::cout.flush();
}

} // namespace

MappingBatch map_low_face_samples_to_high_faces(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingBatch batch;
    const double tolerance = mapping_tolerance(shared_context);
    // Preserve the input order by writing each sample directly back to its slot.
    batch.results.resize(low_uv_samples.size());

    const std::size_t worker_count = mapping_worker_count(low_uv_samples.size(), shared_context);

    if (worker_count <= 1) {
        BRepAdaptor_Surface low_surface(low_face);
        for (std::size_t sample_index = 0; sample_index < low_uv_samples.size(); ++sample_index) {
            batch.results[sample_index] = IndexedMappingResult{
                sample_index,
                map_low_face_sample_to_high_faces(
                    low_surface,
                    low_face_id,
                    low_uv_samples[sample_index],
                    high_faces,
                    tolerance),
            };
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
            // Each worker builds its own adaptor so the OCCT face access stays local
            // to the task and the only shared state is the read-only input arrays.
            BRepAdaptor_Surface low_surface(low_face);
            for (std::size_t sample_index = begin; sample_index < end; ++sample_index) {
                batch.results[sample_index] = IndexedMappingResult{
                    sample_index,
                    map_low_face_sample_to_high_faces(
                        low_surface,
                        low_face_id,
                        low_uv_samples[sample_index],
                        high_faces,
                        tolerance),
                };
            }
        }));
    }

    for (std::future<void>& future : futures) {
        future.get();
    }

    return batch;
}

MappingBatch map_brep_low_face_samples_to_high_faces(
    const std::string& low_brep_data,
    const std::string& high_brep_data,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const MappingContext* shared_context) {
    const TopoDS_Shape low_shape = read_brep_bytes(low_brep_data);
    const TopoDS_Shape high_shape = read_brep_bytes(high_brep_data);
    const std::vector<TopoDS_Face> low_faces = collect_faces(low_shape);
    const std::vector<TopoDS_Face> high_faces = collect_faces(high_shape);

    if (low_face_id < 0 || low_face_id >= static_cast<std::int32_t>(low_faces.size())) {
        throw std::out_of_range("low_face_id is outside the low face list");
    }

    return map_low_face_samples_to_high_faces(
        low_faces[static_cast<std::size_t>(low_face_id)],
        low_face_id,
        low_uv_samples,
        high_faces,
        shared_context);
}

MappingBatch map_low_face_sample_groups_to_high_faces(
    const FaceUvSampleBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingBatch batch;

    for (const FaceUvSamples& group : low_face_samples.faces) {
        if (group.face_id < 0 || group.face_id >= static_cast<std::int32_t>(low_faces.size())) {
            throw std::out_of_range("sample group face_id is outside the low face list");
        }

        // Flatten one face-group at a time, then restore the original sample indices
        // so later stages can still join records by the Python-side order.
        std::vector<UvCoord> local_uvs;
        local_uvs.reserve(group.samples.size());
        for (const IndexedUvCoord& indexed_uv : group.samples) {
            local_uvs.push_back(indexed_uv.value);
        }

        MappingBatch group_batch = map_low_face_samples_to_high_faces(
            low_faces[static_cast<std::size_t>(group.face_id)],
            group.face_id,
            local_uvs,
            high_faces,
            shared_context);

        for (std::size_t local_index = 0; local_index < group_batch.results.size(); ++local_index) {
            IndexedMappingResult result = group_batch.results[local_index];
            result.index = group.samples[local_index].index;
            batch.results.push_back(result);
        }
    }

    return batch;
}

MappingBatch map_uv_samples_to_high_faces(
    const std::vector<UvSample>& samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    // Keep the compatibility path thin: normalize once, then reuse the grouped path.
    return map_low_face_sample_groups_to_high_faces(
        normalize_flat_uv_samples(samples),
        low_faces,
        high_faces,
        shared_context);
}

void debug_print_shape_uv_sample_batch(
    const TopoDS_Shape& shape,
    const FaceUvSampleBatch& samples,
    const std::string& label) {
    // Debug helpers stay in C++ so we can verify the Python bridge before mapping.
    print_face_uv_sample_batch(shape, samples, label);
}

void debug_print_brep_uv_sample_batch(
    const std::string& brep_data,
    const FaceUvSampleBatch& samples,
    const std::string& label) {
    TopoDS_Shape shape = read_brep_bytes(brep_data);
    print_face_uv_sample_batch(shape, samples, label);
}

void debug_print_shape_uv_samples(
    const TopoDS_Shape& shape,
    const std::vector<UvSample>& samples,
    const std::string& label) {
    print_face_uv_sample_batch(shape, normalize_flat_uv_samples(samples), label);
}

void debug_print_brep_uv_samples(
    const std::string& brep_data,
    const std::vector<UvSample>& samples,
    const std::string& label) {
    TopoDS_Shape shape = read_brep_bytes(brep_data);
    print_face_uv_sample_batch(shape, normalize_flat_uv_samples(samples), label);
}

SurfaceEvalBatch evaluate_mapped_high_uvs(
    const MappingBatch& mapping,
    const std::vector<TopoDS_Face>& high_faces) {
    SurfaceEvalBatch batch;

    // TODO: this stage should only read the selected high face and UV per sample.
    // TODO: determine whether to cache adaptors per face before parallelizing.
    //
    // Intended OCCT use:
    // - BRepAdaptor_Surface(high_face)
    // - BRepLProp_SLProps(adaptor, u, v, 1, tol)
    // - props.IsNormalDefined()
    // - props.Normal()
    // - BRepAdaptor_Surface::Value(u, v) for the 3D point
    // - face.Orientation() / TopAbs_REVERSED to flip the normal when needed
    //
    // What we must do ourselves:
    // - map a `high_face_id` back to the face list safely
    // - handle missing/failed mappings
    // - define the fallback for undefined normals
    // - convert OCCT vectors into our plain POD result structures

    (void)mapping;
    (void)high_faces;
    return batch;
}

MappedSampleBatch map_and_evaluate_samples(
    const FaceUvSampleBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappedSampleBatch batch;

    // TODO: this convenience path should just orchestrate the two stages above.
    // TODO: keep this function thin so tests can hit each stage independently.
    //
    // Intended structure:
    // 1. map low UV samples to high faces, one low face at a time.
    // 2. evaluate high UVs to normals.
    // 3. merge sample, mapping, and surface results into one record stream.
    //
    // What we must do ourselves:
    // - decide whether records are joined by index or by explicit sample ids
    // - keep errors visible instead of silently dropping failed samples
    // - decide how to expose partial failures to Python and tests

    (void)low_face_samples;
    (void)low_faces;
    (void)high_faces;
    (void)shared_context;
    return batch;
}

} // namespace cad_uv_map
