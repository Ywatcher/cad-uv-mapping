#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/compat.hpp"
#include "cad_uv_map/pipeline.hpp"
#include "cad_uv_map/surface_eval.hpp"
#include "projection/mapping_projection.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepLProp_SLProps.hxx>
#include <algorithm>
#include <cmath>
#include <future>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <TopAbs.hxx>
#include <thread>
#include <iostream>

namespace cad_uv_map {

// =============================================================================
// Internal support code
// =============================================================================
namespace {

// internal geometry helpers

// core surface-evaluation helper
SurfaceEvalResult make_no_surface_eval_result(std::int32_t face_id, const UvCoord& uv) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return SurfaceEvalResult{
        face_id,
        uv,
        Vec3{nan, nan, nan},
        Vec3{nan, nan, nan},
        false,
    };
}

// take one sample on a high_face, identified by (high_face, high_face_id, uv)
// and turn it into actual surface data
SurfaceEvalResult evaluate_high_face_sample(
    const TopoDS_Face& high_face,
    std::int32_t high_face_id,
    const UvCoord& uv,
    double tolerance) {
    try {
        BRepAdaptor_Surface high_surface(high_face);
        const gp_Pnt point = high_surface.Value(uv.u, uv.v);
        BRepLProp_SLProps props(high_surface, uv.u, uv.v, 1, tolerance);

        if (!props.IsNormalDefined()) {
            SurfaceEvalResult result = make_no_surface_eval_result(high_face_id, uv);
            result.point = Vec3{point.X(), point.Y(), point.Z()};
            return result;
        }

        gp_Dir normal = props.Normal();
        if (high_face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }

        return SurfaceEvalResult{
            high_face_id,
            uv,
            Vec3{point.X(), point.Y(), point.Z()},
            Vec3{normal.X(), normal.Y(), normal.Z()},
            true,
        };
    } catch (const std::exception&) {
        return make_no_surface_eval_result(high_face_id, uv);
    }
}

// batch grouping helper
// decides how many parallel workers the mapping code should use for a batch
std::size_t mapping_worker_count(std::size_t sample_count, const MappingContext* shared_context) {
    if (shared_context == nullptr || !shared_context->enable_parallel || sample_count < 64) {
        return 1;
    }

    // Keep the worker count bounded by hardware, but never exceed the sample count.
    const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    return std::min(sample_count, hardware);
}

// debug / inspection helper
void print_face_uv_sample_batch(const TopoDS_Shape& shape, const FaceUvSampleGroupBatch& samples, const std::string& label) {
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
        const FaceUvSampleGroup& group = samples.faces[group_index];
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

// =============================================================================
// Public mapping API
// =============================================================================
// input: one low face, its face id, low-face UV samples, candidate high faces, method
// output: one mapping record per sample, with high face id, high UV, hit point, and distance
// how: dispatches to the per-method projection implementation in cpp/src/projection/.
//      The nearest/ray algorithms remain separate functions; only the selection is merged.
MappingResultBatch map_single_low_face_samples_to_high_faces(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    MappingMethod method,
    const MappingContext* shared_context) {
    switch (method) {
        case MappingMethod::ray_bidirectional:
            return detail::map_low_face_samples_to_high_faces_ray_bidirectional_impl(
                low_face, low_face_id, low_uv_samples, high_faces, shared_context);
        case MappingMethod::ray:
            return detail::map_low_face_samples_to_high_faces_ray_impl(
                low_face, low_face_id, low_uv_samples, high_faces, shared_context);
        case MappingMethod::nearest:
        default:
            return detail::map_low_face_samples_to_high_faces_nearest_impl(
                low_face, low_face_id, low_uv_samples, high_faces, shared_context);
    }
}

// =============================================================================
// Public surface-evaluation API
// =============================================================================
// input: one high face, its face id, and high-face UV samples
// output: one surface-evaluation record per sample, with world point, normal, and normal flag
// how: evaluates the OCCT surface directly at each UV on the given face
SurfaceEvalResultBatch evaluate_single_high_face_samples(
    const TopoDS_Face& high_face,
    std::int32_t high_face_id,
    const std::vector<UvCoord>& high_uv_samples,
    const MappingContext* shared_context) {
    (void)shared_context;

    SurfaceEvalResultBatch batch;
    batch.results.resize(high_uv_samples.size());

    const double tolerance = detail::mapping_tolerance(shared_context);
    for (std::size_t sample_index = 0; sample_index < high_uv_samples.size(); ++sample_index) {
        batch.results[sample_index] = IndexedSurfaceEvalResult{
            sample_index,
            evaluate_high_face_sample(
                high_face,
                high_face_id,
                high_uv_samples[sample_index],
                tolerance),
        };
    }

    return batch;
}

// input: low/high BREP bytes, low face id, low-face UV samples, projection method
// output: mapping batch for the selected low face
// how: reads both shapes from BREP, extracts faces, validates the low face id, then
//      dispatches to the projection method via the single-face entry point
MappingResultBatch map_brep_single_low_face_samples_to_high_faces(
    const std::string& low_brep_data,
    const std::string& high_brep_data,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    MappingMethod method,
    const MappingContext* shared_context) {
    const TopoDS_Shape low_shape = read_brep_bytes(low_brep_data);
    const TopoDS_Shape high_shape = read_brep_bytes(high_brep_data);
    const std::vector<TopoDS_Face> low_faces = collect_faces(low_shape);
    const std::vector<TopoDS_Face> high_faces = collect_faces(high_shape);

    if (low_face_id < 0 || low_face_id >= static_cast<std::int32_t>(low_faces.size())) {
        throw std::out_of_range("low_face_id is outside the low face list");
    }

    return map_single_low_face_samples_to_high_faces(
        low_faces[static_cast<std::size_t>(low_face_id)],
        low_face_id,
        low_uv_samples,
        high_faces,
        method,
        shared_context);
}

// input: grouped low-face samples, resolved low faces, resolved high faces, and optional context
// output: mapping batch with original sample indices restored
// how: maps one face group at a time, then rewrites each result index back to the Python order
MappingResultBatch map_multiple_low_face_sample_groups_to_high_faces(
    const FaceUvSampleGroupBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    MappingMethod method,
    const MappingContext* shared_context) {
    std::vector<MappingResultBatch> group_batches(low_face_samples.faces.size());
    const bool use_parallel = shared_context != nullptr && shared_context->enable_parallel && low_face_samples.faces.size() > 1;

    auto map_group = [&](std::size_t group_index) {
        const FaceUvSampleGroup& group = low_face_samples.faces[group_index];
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

        MappingResultBatch group_batch = map_single_low_face_samples_to_high_faces(
            low_faces[static_cast<std::size_t>(group.face_id)],
            group.face_id,
            local_uvs,
            high_faces,
            method,
            shared_context);

        group_batches[group_index] = std::move(group_batch);
    };

    if (use_parallel) {
        std::vector<std::future<void>> futures;
        futures.reserve(low_face_samples.faces.size());
        for (std::size_t group_index = 0; group_index < low_face_samples.faces.size(); ++group_index) {
            futures.push_back(std::async(std::launch::async, [&, group_index]() {
                map_group(group_index);
            }));
        }
        for (std::future<void>& future : futures) {
            future.get();
        }
    } else {
        for (std::size_t group_index = 0; group_index < low_face_samples.faces.size(); ++group_index) {
            map_group(group_index);
        }
    }

    MappingResultBatch batch;
    for (std::size_t group_index = 0; group_index < low_face_samples.faces.size(); ++group_index) {
        const FaceUvSampleGroup& group = low_face_samples.faces[group_index];
        const MappingResultBatch& group_batch = group_batches[group_index];
        for (std::size_t local_index = 0; local_index < group_batch.results.size(); ++local_index) {
            IndexedMappingResult result = group_batch.results[local_index];
            result.index = group.samples[local_index].index;
            batch.results.push_back(result);
        }
    }

    return batch;
}

// input: flat low-face samples, resolved low faces, resolved high faces, and optional context
// output: mapping batch in the same indexed record form as the grouped API
// how: normalizes the flat samples into face groups, then reuses the grouped mapping path
// =============================================================================
// Debug / inspection API
// =============================================================================
// input: shape plus grouped UV samples
// output: none, writes a human-readable dump to stdout
// how: prints face count, per-face bounds, and the grouped sample order
void debug_print_shape_uv_sample_batch(
    const TopoDS_Shape& shape,
    const FaceUvSampleGroupBatch& samples,
    const std::string& label) {
    // Debug helpers stay in C++ so we can verify the Python bridge before mapping.
    print_face_uv_sample_batch(shape, samples, label);
}

// input: low-face BREP bytes plus grouped UV samples
// output: none, writes a human-readable dump to stdout
// how: loads the shape from BREP, then prints the grouped sample layout
void debug_print_brep_uv_sample_batch(
    const std::string& brep_data,
    const FaceUvSampleGroupBatch& samples,
    const std::string& label) {
    TopoDS_Shape shape = read_brep_bytes(brep_data);
    print_face_uv_sample_batch(shape, samples, label);
}

// input: shape plus flat UV samples
// output: none, writes a human-readable dump to stdout
// how: normalizes the flat list into grouped face records, then prints them
void debug_print_shape_uv_samples(
    const TopoDS_Shape& shape,
    const std::vector<FlatUvSample>& samples,
    const std::string& label) {
    print_face_uv_sample_batch(shape, normalize_flat_uv_samples(samples), label);
}

// input: mapping batch from the previous stage, plus resolved high faces
// output: one surface-evaluation record per mapped sample
// how: groups mapped UVs by high face id, evaluates each face batch, then restores indices
SurfaceEvalResultBatch evaluate_multiple_high_face_samples(
    const MappingResultBatch& mapping,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    // surface evaluation wrapper
    SurfaceEvalResultBatch batch;
    std::size_t output_count = 0;
    for (const IndexedMappingResult& indexed_mapping : mapping.results) {
        output_count = std::max(output_count, indexed_mapping.index + 1);
    }
    batch.results.resize(output_count);

    std::vector<std::vector<IndexedUvCoord>> grouped_samples(high_faces.size());
    std::size_t grouped_count = 0;

    for (const IndexedMappingResult& indexed_mapping : mapping.results) {
        const MappingResult& value = indexed_mapping.value;
        if (value.high_face_id < 0 || value.high_face_id >= static_cast<std::int32_t>(high_faces.size())) {
            batch.results[indexed_mapping.index] = IndexedSurfaceEvalResult{
                indexed_mapping.index,
                make_no_surface_eval_result(value.high_face_id, value.high_uv),
            };
            continue;
        }

        grouped_samples[static_cast<std::size_t>(value.high_face_id)].push_back(IndexedUvCoord{
            indexed_mapping.index,
            value.high_uv,
        });
        ++grouped_count;
    }

    auto evaluate_group = [&](std::int32_t high_face_id) {
        const std::vector<IndexedUvCoord>& group = grouped_samples[static_cast<std::size_t>(high_face_id)];
        if (group.empty()) {
            return;
        }

        std::vector<UvCoord> local_uvs;
        local_uvs.reserve(group.size());
        for (const IndexedUvCoord& indexed_uv : group) {
            local_uvs.push_back(indexed_uv.value);
        }

        SurfaceEvalResultBatch group_batch = evaluate_single_high_face_samples(
            high_faces[static_cast<std::size_t>(high_face_id)],
            high_face_id,
            local_uvs,
            shared_context);

        for (std::size_t local_index = 0; local_index < group_batch.results.size(); ++local_index) {
            const std::size_t output_index = group[local_index].index;
            batch.results[output_index] = IndexedSurfaceEvalResult{
                output_index,
                group_batch.results[local_index].value,
            };
        }
    };

    const bool use_parallel = shared_context != nullptr && shared_context->enable_parallel && grouped_count > 1;
    if (use_parallel) {
        std::vector<std::future<void>> futures;
        futures.reserve(high_faces.size());
        for (std::int32_t high_face_id = 0; high_face_id < static_cast<std::int32_t>(high_faces.size()); ++high_face_id) {
            if (grouped_samples[static_cast<std::size_t>(high_face_id)].empty()) {
                continue;
            }
            futures.push_back(std::async(std::launch::async, [&, high_face_id]() {
                evaluate_group(high_face_id);
            }));
        }
        for (std::future<void>& future : futures) {
            future.get();
        }
    } else {
        for (std::int32_t high_face_id = 0; high_face_id < static_cast<std::int32_t>(high_faces.size()); ++high_face_id) {
            evaluate_group(high_face_id);
        }
    }

    return batch;
}

// =============================================================================
// Public combined pipeline
// =============================================================================
// input: grouped low-face samples, resolved low faces, resolved high faces, and optional context
// output: combined record containing the original sample, mapping result, and surface result
// how: runs grouped mapping first, then surface evaluation, then joins both stages by index
MappedSampleBatch map_and_evaluate_multiple_low_face_samples(
    const FaceUvSampleGroupBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    MappingMethod method,
    const MappingContext* shared_context) {
    MappedSampleBatch batch;

    const MappingResultBatch mapping = map_multiple_low_face_sample_groups_to_high_faces(
        low_face_samples,
        low_faces,
        high_faces,
        method,
        shared_context);
    const SurfaceEvalResultBatch surface = evaluate_multiple_high_face_samples(mapping, high_faces, shared_context);

    std::size_t output_count = 0;
    for (const IndexedMappingResult& indexed_mapping : mapping.results) {
        output_count = std::max(output_count, indexed_mapping.index + 1);
    }
    batch.records.resize(output_count);
    for (const IndexedMappingResult& indexed_mapping : mapping.results) {
        const MappingResult& mapping_value = indexed_mapping.value;
        const std::size_t output_index = indexed_mapping.index;
        const SurfaceEvalResult& surface_value = surface.results[output_index].value;

        batch.records[output_index] = IndexedMappedSampleRecord{
            output_index,
            MappedSampleRecord{
                FlatUvSample{
                    mapping_value.low_face_id,
                    mapping_value.low_uv,
                },
                mapping_value,
                surface_value,
            },
        };
    }

    return batch;
}

} // namespace cad_uv_map
