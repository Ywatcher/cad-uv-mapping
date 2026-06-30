#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/compat.hpp"
#include "cad_uv_map/numpy_exports.hpp"
#include "cad_uv_map/pipeline.hpp"
#include "cad_uv_map/surface_eval.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

// Pack a sequence of mapping records into a columnar dict (struct-of-arrays).
// `idx(rec)` yields the stable sample index; `val(rec)` yields the MappingResult.
// Used by both MappingResultBatch and the composed MappedSampleBatch.
template <typename Records, typename IdxFn, typename ValFn>
py::dict pack_mapping_columns(const Records& records, IdxFn idx, ValFn val) {
    const std::size_t n = records.size();
    std::vector<std::size_t> index;
    std::vector<std::int32_t> low_face_id, high_face_id;
    std::vector<cad_uv_map::UvCoord> low_uv, high_uv;
    std::vector<cad_uv_map::Vec3> point;
    std::vector<double> distance;
    std::vector<cad_uv_map::MappingStatus> status;
    index.reserve(n);
    low_face_id.reserve(n);
    high_face_id.reserve(n);
    low_uv.reserve(n);
    high_uv.reserve(n);
    point.reserve(n);
    distance.reserve(n);
    status.reserve(n);
    for (const auto& record : records) {
        const cad_uv_map::MappingResult& v = val(record);
        index.push_back(idx(record));
        low_face_id.push_back(v.low_face_id);
        high_face_id.push_back(v.high_face_id);
        low_uv.push_back(v.low_uv);
        high_uv.push_back(v.high_uv);
        point.push_back(v.point);
        distance.push_back(v.distance);
        status.push_back(v.status);
    }
    py::dict d;
    d["index"] = cad_uv_map::to_numpy_index_array(index);
    d["low_face_id"] = cad_uv_map::to_numpy_int32_array(low_face_id);
    d["low_uv"] = cad_uv_map::to_numpy_uv_array(low_uv);
    d["high_face_id"] = cad_uv_map::to_numpy_int32_array(high_face_id);
    d["high_uv"] = cad_uv_map::to_numpy_uv_array(high_uv);
    d["point"] = cad_uv_map::to_numpy_vec3_array(point);
    d["distance"] = cad_uv_map::to_numpy_double_array(distance);
    d["status"] = cad_uv_map::to_numpy_status_array(status);
    return d;
}

// Pack a sequence of surface-evaluation records into a columnar dict.
template <typename Records, typename IdxFn, typename ValFn>
py::dict pack_surface_columns(const Records& records, IdxFn idx, ValFn val) {
    const std::size_t n = records.size();
    std::vector<std::size_t> index;
    std::vector<std::int32_t> face_id;
    std::vector<cad_uv_map::UvCoord> uv;
    std::vector<cad_uv_map::Vec3> point, normal;
    std::vector<bool> normal_defined;
    index.reserve(n);
    face_id.reserve(n);
    uv.reserve(n);
    point.reserve(n);
    normal.reserve(n);
    normal_defined.reserve(n);
    for (const auto& record : records) {
        const cad_uv_map::SurfaceEvalResult& v = val(record);
        index.push_back(idx(record));
        face_id.push_back(v.face_id);
        uv.push_back(v.uv);
        point.push_back(v.point);
        normal.push_back(v.normal);
        normal_defined.push_back(v.normal_defined);
    }
    py::dict d;
    d["index"] = cad_uv_map::to_numpy_index_array(index);
    d["face_id"] = cad_uv_map::to_numpy_int32_array(face_id);
    d["uv"] = cad_uv_map::to_numpy_uv_array(uv);
    d["point"] = cad_uv_map::to_numpy_vec3_array(point);
    d["normal"] = cad_uv_map::to_numpy_vec3_array(normal);
    d["normal_defined"] = cad_uv_map::to_numpy_bool_array(normal_defined);
    return d;
}

}  // namespace

PYBIND11_MODULE(_native, m) {
    m.doc() = "OpenCascade-backed CAD UV mapping native module";

    /*
     * Python-facing record and batch types.
     *
     * These classes are intentionally exposed so Python tests and wrappers can
     * inspect raw native results without reimplementing the data model.
     */
    py::class_<cad_uv_map::UvCoord>(m, "UvCoord")
        .def(py::init<>())
        .def_readwrite("u", &cad_uv_map::UvCoord::u)
        .def_readwrite("v", &cad_uv_map::UvCoord::v)
        .def("to_numpy", [](const cad_uv_map::UvCoord& value) {
            std::vector<cad_uv_map::UvCoord> values{value};
            return cad_uv_map::to_numpy_uv_array(values);
        });

    py::class_<cad_uv_map::Vec3>(m, "Vec3")
        .def(py::init<>())
        .def_readwrite("x", &cad_uv_map::Vec3::x)
        .def_readwrite("y", &cad_uv_map::Vec3::y)
        .def_readwrite("z", &cad_uv_map::Vec3::z)
        .def("to_numpy", [](const cad_uv_map::Vec3& value) {
            std::vector<cad_uv_map::Vec3> values{value};
            return cad_uv_map::to_numpy_vec3_array(values);
        });

    py::class_<cad_uv_map::IndexedUvCoord>(m, "IndexedUvCoord")
        .def(py::init<>())
        .def_readwrite("index", &cad_uv_map::IndexedUvCoord::index)
        .def_readwrite("value", &cad_uv_map::IndexedUvCoord::value)
        .def("to_numpy_index_uv", [](const cad_uv_map::IndexedUvCoord& value) {
            py::array_t<std::int64_t> index_array({1});
            index_array.mutable_unchecked<1>()(0) = static_cast<std::int64_t>(value.index);
            std::vector<cad_uv_map::UvCoord> values{value.value};
            return py::make_tuple(index_array, cad_uv_map::to_numpy_uv_array(values));
        })
        .def("to_numpy_indexed_uv", [](const cad_uv_map::IndexedUvCoord& value) {
            std::vector<cad_uv_map::IndexedUvCoord> values{value};
            return cad_uv_map::to_numpy_indexed_uv_array(values);
        });

    py::class_<cad_uv_map::FlatUvSample>(m, "FlatUvSample")
        .def(py::init<>())
        .def_readwrite("face_id", &cad_uv_map::FlatUvSample::face_id)
        .def_readwrite("uv", &cad_uv_map::FlatUvSample::uv)
        .def("to_numpy_uv", [](const cad_uv_map::FlatUvSample& value) {
            std::vector<cad_uv_map::UvCoord> values{value.uv};
            return cad_uv_map::to_numpy_uv_array(values);
        })
        .def("to_numpy_face_uv", [](const cad_uv_map::FlatUvSample& value) {
            py::array_t<double> array({3});
            auto view = array.mutable_unchecked<1>();
            view(0) = static_cast<double>(value.face_id);
            view(1) = value.uv.u;
            view(2) = value.uv.v;
            return array;
        });

    py::class_<cad_uv_map::IndexedFlatUvSample>(m, "IndexedFlatUvSample")
        .def(py::init<>())
        .def_readwrite("index", &cad_uv_map::IndexedFlatUvSample::index)
        .def_readwrite("value", &cad_uv_map::IndexedFlatUvSample::value);

    py::class_<cad_uv_map::FaceUvSampleGroup>(m, "FaceUvSampleGroup")
        .def(py::init<>())
        .def_readwrite("face_id", &cad_uv_map::FaceUvSampleGroup::face_id)
        .def_readwrite("samples", &cad_uv_map::FaceUvSampleGroup::samples)
        .def("to_numpy_uv_array", [](const cad_uv_map::FaceUvSampleGroup& value) {
            std::vector<cad_uv_map::UvCoord> coords;
            coords.reserve(value.samples.size());
            for (const auto& sample : value.samples) {
                coords.push_back(sample.value);
            }
            return cad_uv_map::to_numpy_uv_array(coords);
        })
        .def("to_numpy_index_array", [](const cad_uv_map::FaceUvSampleGroup& value) {
            std::vector<std::size_t> indices;
            indices.reserve(value.samples.size());
            for (const auto& sample : value.samples) {
                indices.push_back(sample.index);
            }
            return cad_uv_map::to_numpy_index_array(indices);
        })
        .def("to_numpy_index_uv_arrays", [](const cad_uv_map::FaceUvSampleGroup& value) {
            std::vector<std::size_t> indices;
            std::vector<cad_uv_map::UvCoord> coords;
            indices.reserve(value.samples.size());
            coords.reserve(value.samples.size());
            for (const auto& sample : value.samples) {
                indices.push_back(sample.index);
                coords.push_back(sample.value);
            }
            return py::make_tuple(cad_uv_map::to_numpy_index_array(indices), cad_uv_map::to_numpy_uv_array(coords));
        })
        .def("to_numpy_indexed_uv_array", [](const cad_uv_map::FaceUvSampleGroup& value) {
            return cad_uv_map::to_numpy_indexed_uv_array(value.samples);
        });

    py::class_<cad_uv_map::IndexedFaceUvSampleGroup>(m, "IndexedFaceUvSampleGroup")
        .def(py::init<>())
        .def_readwrite("index", &cad_uv_map::IndexedFaceUvSampleGroup::index)
        .def_readwrite("value", &cad_uv_map::IndexedFaceUvSampleGroup::value);

    py::class_<cad_uv_map::FaceUvSampleGroupBatch>(m, "FaceUvSampleGroupBatch")
        .def(py::init<>())
        .def_readwrite("faces", &cad_uv_map::FaceUvSampleGroupBatch::faces)
        .def("to_numpy_group_list", [](const cad_uv_map::FaceUvSampleGroupBatch& value) {
            py::list result;
            for (const auto& group : value.faces) {
                std::vector<cad_uv_map::UvCoord> coords;
                coords.reserve(group.samples.size());
                for (const auto& sample : group.samples) {
                    coords.push_back(sample.value);
                }
                result.append(cad_uv_map::to_numpy_uv_array(coords));
            }
            return result;
        })
        .def("to_numpy_flat_face_ids", [](const cad_uv_map::FaceUvSampleGroupBatch& value) {
            std::vector<std::int32_t> face_ids;
            face_ids.reserve(value.faces.size());
            for (const auto& group : value.faces) {
                face_ids.push_back(group.face_id);
            }
            return cad_uv_map::to_numpy_int32_array(face_ids);
        })
        .def("to_numpy_flat_counts", [](const cad_uv_map::FaceUvSampleGroupBatch& value) {
            std::vector<std::int32_t> counts;
            counts.reserve(value.faces.size());
            for (const auto& group : value.faces) {
                counts.push_back(static_cast<std::int32_t>(group.samples.size()));
            }
            return cad_uv_map::to_numpy_int32_array(counts);
        })
        .def("to_numpy_flat_arrays", [](const cad_uv_map::FaceUvSampleGroupBatch& value) {
            std::vector<std::int32_t> face_ids;
            std::vector<std::size_t> indices;
            std::vector<cad_uv_map::UvCoord> coords;
            for (const auto& group : value.faces) {
                for (const auto& sample : group.samples) {
                    face_ids.push_back(group.face_id);
                    indices.push_back(sample.index);
                    coords.push_back(sample.value);
                }
            }
            return py::make_tuple(
                cad_uv_map::to_numpy_int32_array(face_ids),
                cad_uv_map::to_numpy_index_array(indices),
                cad_uv_map::to_numpy_uv_array(coords));
        })
        .def("to_numpy_grouped_indexed_uv_array", [](const cad_uv_map::FaceUvSampleGroupBatch& value) {
            std::size_t count = 0;
            for (const auto& group : value.faces) {
                count += group.samples.size();
            }
            py::array_t<double> array({
                static_cast<py::ssize_t>(count),
                static_cast<py::ssize_t>(4),
            });
            auto view = array.mutable_unchecked<2>();
            std::size_t row = 0;
            for (const auto& group : value.faces) {
                for (const auto& sample : group.samples) {
                    view(row, 0) = static_cast<double>(group.face_id);
                    view(row, 1) = static_cast<double>(sample.index);
                    view(row, 2) = sample.value.u;
                    view(row, 3) = sample.value.v;
                    ++row;
                }
            }
            return array;
        });

    py::class_<cad_uv_map::MappingContext>(m, "MappingContext")
        .def(py::init<>())
        .def_readwrite("tolerance", &cad_uv_map::MappingContext::tolerance)
        .def_readwrite("enable_parallel", &cad_uv_map::MappingContext::enable_parallel)
        .def_readwrite("preserve_input_order", &cad_uv_map::MappingContext::preserve_input_order);

    py::enum_<cad_uv_map::MappingStatus>(m, "MappingStatus")
        .value("hit", cad_uv_map::MappingStatus::hit)
        .value("no_hit", cad_uv_map::MappingStatus::no_hit)
        .value("ambiguous", cad_uv_map::MappingStatus::ambiguous)
        .value("outside_trim", cad_uv_map::MappingStatus::outside_trim)
        .value("failed", cad_uv_map::MappingStatus::failed);

    py::enum_<cad_uv_map::MappingMethod>(m, "MappingMethod")
        .value("nearest", cad_uv_map::MappingMethod::nearest)
        .value("ray", cad_uv_map::MappingMethod::ray)
        .value("ray_bidirectional", cad_uv_map::MappingMethod::ray_bidirectional);

    // Output batches expose a single columnar accessor `columns()` returning a
    // dict of name -> NumPy array (struct-of-arrays). Per-record and Indexed*
    // output types are intentionally not bound; Python builds row views and
    // structured arrays from the columns.
    py::class_<cad_uv_map::MappingResultBatch>(m, "MappingResultBatch")
        .def(py::init<>())
        .def("__len__", [](const cad_uv_map::MappingResultBatch& b) { return b.results.size(); })
        .def("columns", [](const cad_uv_map::MappingResultBatch& b) {
            return pack_mapping_columns(
                b.results,
                [](const cad_uv_map::IndexedMappingResult& r) { return r.index; },
                [](const cad_uv_map::IndexedMappingResult& r) -> const cad_uv_map::MappingResult& { return r.value; });
        });

    py::class_<cad_uv_map::SurfaceEvalResultBatch>(m, "SurfaceEvalResultBatch")
        .def(py::init<>())
        .def("__len__", [](const cad_uv_map::SurfaceEvalResultBatch& b) { return b.results.size(); })
        .def("columns", [](const cad_uv_map::SurfaceEvalResultBatch& b) {
            return pack_surface_columns(
                b.results,
                [](const cad_uv_map::IndexedSurfaceEvalResult& r) { return r.index; },
                [](const cad_uv_map::IndexedSurfaceEvalResult& r) -> const cad_uv_map::SurfaceEvalResult& { return r.value; });
        });

    // The combined batch composes the two stage column-sets, sharing row order.
    // Python wraps these into composed .mapping / .surface sub-batches.
    py::class_<cad_uv_map::MappedSampleBatch>(m, "MappedSampleBatch")
        .def(py::init<>())
        .def("__len__", [](const cad_uv_map::MappedSampleBatch& b) { return b.records.size(); })
        .def("mapping_columns", [](const cad_uv_map::MappedSampleBatch& b) {
            return pack_mapping_columns(
                b.records,
                [](const cad_uv_map::IndexedMappedSampleRecord& r) { return r.index; },
                [](const cad_uv_map::IndexedMappedSampleRecord& r) -> const cad_uv_map::MappingResult& { return r.value.mapping; });
        })
        .def("surface_columns", [](const cad_uv_map::MappedSampleBatch& b) {
            return pack_surface_columns(
                b.records,
                [](const cad_uv_map::IndexedMappedSampleRecord& r) { return r.index; },
                [](const cad_uv_map::IndexedMappedSampleRecord& r) -> const cad_uv_map::SurfaceEvalResult& { return r.value.surface; });
        });

    py::class_<cad_uv_map::FaceInfo>(m, "FaceInfo")
        .def_readonly("face_id", &cad_uv_map::FaceInfo::face_id)
        .def_readonly("u_min", &cad_uv_map::FaceInfo::u_min)
        .def_readonly("u_max", &cad_uv_map::FaceInfo::u_max)
        .def_readonly("v_min", &cad_uv_map::FaceInfo::v_min)
        .def_readonly("v_max", &cad_uv_map::FaceInfo::v_max);

    py::class_<cad_uv_map::UniformUvGrid>(m, "UniformUvGrid")
        .def(py::init<>())
        .def_readwrite("face_id", &cad_uv_map::UniformUvGrid::face_id)
        .def_readwrite("u_count", &cad_uv_map::UniformUvGrid::u_count)
        .def_readwrite("v_count", &cad_uv_map::UniformUvGrid::v_count)
        .def_readwrite("margin", &cad_uv_map::UniformUvGrid::margin);

    py::class_<cad_uv_map::UniformUvToleranceGrid>(m, "UniformUvToleranceGrid")
        .def(py::init<>())
        .def_readwrite("face_id", &cad_uv_map::UniformUvToleranceGrid::face_id)
        .def_readwrite("tolerance", &cad_uv_map::UniformUvToleranceGrid::tolerance)
        .def_readwrite("margin", &cad_uv_map::UniformUvToleranceGrid::margin);

    /*
     * Python-facing summary, mapping, and inspection functions.
     *
     * These are the native entry points that the Python layer wraps. Internal
     * helpers in mapping.cpp are not exported directly.
     */
    m.def("describe_brep_faces", &cad_uv_map::describe_brep_faces,
          py::arg("brep_path"),
          "Read a BREP file and return basic face UV bounds.");
    m.def("describe_brep_bytes",
          [](py::bytes brep_data) {
              return cad_uv_map::describe_brep_bytes(static_cast<std::string>(brep_data));
          },
          py::arg("brep_data"),
          "Read BREP bytes and return basic face UV bounds.");

    m.def("debug_print_brep_faces", &cad_uv_map::debug_print_brep_faces,
          py::arg("brep_path"),
          "Read a BREP file and print face debug information to stdout.");
    m.def("debug_print_brep_bytes",
          [](py::bytes brep_data, const std::string& label) {
              cad_uv_map::debug_print_brep_bytes(static_cast<std::string>(brep_data), label);
          },
          py::arg("brep_data"),
          py::arg("label") = "BREP bytes",
          "Read BREP bytes and print face debug information to stdout.");

    m.def("debug_print_brep_uv_sample_batch",
          [](py::bytes brep_data, const cad_uv_map::FaceUvSampleGroupBatch& samples, const std::string& label) {
              cad_uv_map::debug_print_brep_uv_sample_batch(static_cast<std::string>(brep_data), samples, label);
          },
          py::arg("brep_data"),
          py::arg("samples"),
          py::arg("label") = "BREP bytes + UV samples",
          "Read BREP bytes and print the provided grouped UV samples in C++.");

    m.def("debug_print_brep_uv_samples",
          [](py::bytes brep_data, const std::vector<cad_uv_map::FlatUvSample>& samples, const std::string& label) {
              cad_uv_map::debug_print_brep_uv_samples(static_cast<std::string>(brep_data), samples, label);
          },
          py::arg("brep_data"),
          py::arg("samples"),
          py::arg("label") = "BREP bytes + flat UV samples",
          "Read BREP bytes and print the provided flat UV samples in C++.");

    m.def("sample_brep_face_uniform_uv_grid",
          [](py::bytes brep_data, const cad_uv_map::UniformUvGrid& grid) {
              return cad_uv_map::sample_brep_face_uniform_uv_grid(static_cast<std::string>(brep_data), grid);
          },
          py::arg("brep_data"),
          py::arg("grid"),
          "Generate a grouped uniform UV sample grid for one face.");

    m.def("sample_brep_face_uniform_uv_grid_batch",
          [](py::bytes brep_data, const std::vector<cad_uv_map::UniformUvGrid>& grids) {
              return cad_uv_map::sample_brep_face_uniform_uv_grid_batch(static_cast<std::string>(brep_data), grids);
          },
          py::arg("brep_data"),
          py::arg("grids"),
          "Generate grouped uniform UV sample grids for multiple faces.");

    m.def("sample_brep_face_uniform_uv_tolerance_grid",
          [](py::bytes brep_data, const cad_uv_map::UniformUvToleranceGrid& grid) {
              return cad_uv_map::sample_brep_face_uniform_uv_tolerance_grid(static_cast<std::string>(brep_data), grid);
          },
          py::arg("brep_data"),
          py::arg("grid"),
          "Generate a grouped UV sample grid for one face using a tolerance-driven density.");

    m.def("sample_brep_face_uniform_uv_tolerance_grid_batch",
          [](py::bytes brep_data, const std::vector<cad_uv_map::UniformUvToleranceGrid>& grids) {
              return cad_uv_map::sample_brep_face_uniform_uv_tolerance_grid_batch(static_cast<std::string>(brep_data), grids);
          },
          py::arg("brep_data"),
          py::arg("grids"),
          "Generate grouped UV sample grids for multiple faces using tolerance-driven density.");

    m.def("map_brep_single_low_face_samples_to_high_faces",
          [](py::bytes low_brep_data,
             py::bytes high_brep_data,
             std::int32_t low_face_id,
             const std::vector<cad_uv_map::UvCoord>& low_uv_samples,
             cad_uv_map::MappingMethod method,
             const cad_uv_map::MappingContext* shared_context) {
              return cad_uv_map::map_brep_single_low_face_samples_to_high_faces(
                  static_cast<std::string>(low_brep_data),
                  static_cast<std::string>(high_brep_data),
                  low_face_id,
                  low_uv_samples,
                  method,
                  shared_context);
          },
          py::arg("low_brep_data"),
          py::arg("high_brep_data"),
          py::arg("low_face_id"),
          py::arg("low_uv_samples"),
          py::arg("method") = cad_uv_map::MappingMethod::nearest,
          py::arg("shared_context") = nullptr,
          "Map UV samples from one low face to high faces using the selected method.");

    m.def("evaluate_brep_single_high_face_samples",
          [](py::bytes high_brep_data,
             std::int32_t high_face_id,
             const std::vector<cad_uv_map::UvCoord>& high_uv_samples,
             const cad_uv_map::MappingContext* shared_context) {
              const TopoDS_Shape high_shape = cad_uv_map::read_brep_bytes(static_cast<std::string>(high_brep_data));
              const std::vector<TopoDS_Face> high_faces = cad_uv_map::collect_faces(high_shape);
              if (high_face_id < 0 || high_face_id >= static_cast<std::int32_t>(high_faces.size())) {
                  throw std::out_of_range("high_face_id is outside the high face list");
              }
              return cad_uv_map::evaluate_single_high_face_samples(
                  high_faces[static_cast<std::size_t>(high_face_id)],
                  high_face_id,
                  high_uv_samples,
                  shared_context);
          },
          py::arg("high_brep_data"),
          py::arg("high_face_id"),
          py::arg("high_uv_samples"),
          py::arg("shared_context") = nullptr,
          "Evaluate point and normal data for UV samples on one high face.");

    m.def("evaluate_brep_multiple_high_face_samples",
          [](py::bytes high_brep_data,
             const cad_uv_map::MappingResultBatch& mapping,
             const cad_uv_map::MappingContext* shared_context) {
              const TopoDS_Shape high_shape = cad_uv_map::read_brep_bytes(static_cast<std::string>(high_brep_data));
              return cad_uv_map::evaluate_multiple_high_face_samples(
                  mapping,
                  cad_uv_map::collect_faces(high_shape),
                  shared_context);
          },
          py::arg("high_brep_data"),
          py::arg("mapping"),
          py::arg("shared_context") = nullptr,
          "Evaluate mapped high-face UVs to point and normal batches.");

    m.def("map_and_evaluate_brep_multiple_low_face_samples",
          [](py::bytes low_brep_data,
             py::bytes high_brep_data,
             const cad_uv_map::FaceUvSampleGroupBatch& low_face_samples,
             cad_uv_map::MappingMethod method,
             const cad_uv_map::MappingContext* shared_context) {
              const TopoDS_Shape low_shape = cad_uv_map::read_brep_bytes(static_cast<std::string>(low_brep_data));
              const TopoDS_Shape high_shape = cad_uv_map::read_brep_bytes(static_cast<std::string>(high_brep_data));
              return cad_uv_map::map_and_evaluate_multiple_low_face_samples(
                  low_face_samples,
                  cad_uv_map::collect_faces(low_shape),
                  cad_uv_map::collect_faces(high_shape),
                  method,
                  shared_context);
          },
          py::arg("low_brep_data"),
          py::arg("high_brep_data"),
          py::arg("low_face_samples"),
          py::arg("method") = cad_uv_map::MappingMethod::nearest,
          py::arg("shared_context") = nullptr,
          "Run the full low-sample to mapping to surface-eval pipeline.");
}
