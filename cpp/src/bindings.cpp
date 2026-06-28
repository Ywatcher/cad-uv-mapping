#include "cad_uv_map/mapping.hpp"
#include "cad_uv_map/compat.hpp"
#include "cad_uv_map/numpy_exports.hpp"
#include "cad_uv_map/pipeline.hpp"
#include "cad_uv_map/surface_eval.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

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

    py::class_<cad_uv_map::MappingResult>(m, "MappingResult")
        .def_readonly("low_face_id", &cad_uv_map::MappingResult::low_face_id)
        .def_readonly("low_uv", &cad_uv_map::MappingResult::low_uv)
        .def_readonly("high_face_id", &cad_uv_map::MappingResult::high_face_id)
        .def_readonly("high_uv", &cad_uv_map::MappingResult::high_uv)
        .def_readonly("point", &cad_uv_map::MappingResult::point)
        .def_readonly("distance", &cad_uv_map::MappingResult::distance)
        .def_readonly("status", &cad_uv_map::MappingResult::status)
        .def_property_readonly("low_u", [](const cad_uv_map::MappingResult& value) { return value.low_uv.u; })
        .def_property_readonly("low_v", [](const cad_uv_map::MappingResult& value) { return value.low_uv.v; })
        .def_property_readonly("high_u", [](const cad_uv_map::MappingResult& value) { return value.high_uv.u; })
        .def_property_readonly("high_v", [](const cad_uv_map::MappingResult& value) { return value.high_uv.v; })
        .def_property_readonly("point_x", [](const cad_uv_map::MappingResult& value) { return value.point.x; })
        .def_property_readonly("point_y", [](const cad_uv_map::MappingResult& value) { return value.point.y; })
        .def_property_readonly("point_z", [](const cad_uv_map::MappingResult& value) { return value.point.z; });

    py::class_<cad_uv_map::IndexedMappingResult>(m, "IndexedMappingResult")
        .def_readonly("index", &cad_uv_map::IndexedMappingResult::index)
        .def_readonly("value", &cad_uv_map::IndexedMappingResult::value);

    py::class_<cad_uv_map::MappingResultBatch>(m, "MappingResultBatch")
        .def(py::init<>())
        .def_readonly("results", &cad_uv_map::MappingResultBatch::results)
        .def("to_numpy_low_uv_array", [](const cad_uv_map::MappingResultBatch& value) {
            std::vector<cad_uv_map::UvCoord> coords;
            coords.reserve(value.results.size());
            for (const auto& result : value.results) {
                coords.push_back(result.value.low_uv);
            }
            return cad_uv_map::to_numpy_uv_array(coords);
        })
        .def("to_numpy_high_uv_array", [](const cad_uv_map::MappingResultBatch& value) {
            std::vector<cad_uv_map::UvCoord> coords;
            coords.reserve(value.results.size());
            for (const auto& result : value.results) {
                coords.push_back(result.value.high_uv);
            }
            return cad_uv_map::to_numpy_uv_array(coords);
        })
        .def("to_numpy_point_array", [](const cad_uv_map::MappingResultBatch& value) {
            std::vector<cad_uv_map::Vec3> points;
            points.reserve(value.results.size());
            for (const auto& result : value.results) {
                points.push_back(result.value.point);
            }
            return cad_uv_map::to_numpy_vec3_array(points);
        })
        .def("to_numpy_distance_array", [](const cad_uv_map::MappingResultBatch& value) {
            py::array_t<double> array({static_cast<py::ssize_t>(value.results.size())});
            auto view = array.mutable_unchecked<1>();
            for (std::size_t i = 0; i < value.results.size(); ++i) {
                view(i) = value.results[i].value.distance;
            }
            return array;
        })
        .def("to_numpy_high_face_id_array", [](const cad_uv_map::MappingResultBatch& value) {
            std::vector<std::int32_t> ids;
            ids.reserve(value.results.size());
            for (const auto& result : value.results) {
                ids.push_back(result.value.high_face_id);
            }
            return cad_uv_map::to_numpy_int32_array(ids);
        })
        .def("to_numpy_low_face_id_array", [](const cad_uv_map::MappingResultBatch& value) {
            std::vector<std::int32_t> ids;
            ids.reserve(value.results.size());
            for (const auto& result : value.results) {
                ids.push_back(result.value.low_face_id);
            }
            return cad_uv_map::to_numpy_int32_array(ids);
        })
        .def("to_numpy_status_array", [](const cad_uv_map::MappingResultBatch& value) {
            std::vector<cad_uv_map::MappingStatus> statuses;
            statuses.reserve(value.results.size());
            for (const auto& result : value.results) {
                statuses.push_back(result.value.status);
            }
            return cad_uv_map::to_numpy_status_array(statuses);
        });

    py::class_<cad_uv_map::SurfaceEvalResult>(m, "SurfaceEvalResult")
        .def_readonly("face_id", &cad_uv_map::SurfaceEvalResult::face_id)
        .def_readonly("uv", &cad_uv_map::SurfaceEvalResult::uv)
        .def_readonly("point", &cad_uv_map::SurfaceEvalResult::point)
        .def_readonly("normal", &cad_uv_map::SurfaceEvalResult::normal)
        .def_readonly("normal_defined", &cad_uv_map::SurfaceEvalResult::normal_defined)
        .def_property_readonly("u", [](const cad_uv_map::SurfaceEvalResult& value) { return value.uv.u; })
        .def_property_readonly("v", [](const cad_uv_map::SurfaceEvalResult& value) { return value.uv.v; })
        .def_property_readonly("point_x", [](const cad_uv_map::SurfaceEvalResult& value) { return value.point.x; })
        .def_property_readonly("point_y", [](const cad_uv_map::SurfaceEvalResult& value) { return value.point.y; })
        .def_property_readonly("point_z", [](const cad_uv_map::SurfaceEvalResult& value) { return value.point.z; })
        .def_property_readonly("normal_x", [](const cad_uv_map::SurfaceEvalResult& value) { return value.normal.x; })
        .def_property_readonly("normal_y", [](const cad_uv_map::SurfaceEvalResult& value) { return value.normal.y; })
        .def_property_readonly("normal_z", [](const cad_uv_map::SurfaceEvalResult& value) { return value.normal.z; });

    py::class_<cad_uv_map::IndexedSurfaceEvalResult>(m, "IndexedSurfaceEvalResult")
        .def_readonly("index", &cad_uv_map::IndexedSurfaceEvalResult::index)
        .def_readonly("value", &cad_uv_map::IndexedSurfaceEvalResult::value);

    py::class_<cad_uv_map::SurfaceEvalResultBatch>(m, "SurfaceEvalResultBatch")
        .def(py::init<>())
        .def_readonly("results", &cad_uv_map::SurfaceEvalResultBatch::results)
        .def("to_numpy_uv_array", [](const cad_uv_map::SurfaceEvalResultBatch& value) {
            std::vector<cad_uv_map::UvCoord> coords;
            coords.reserve(value.results.size());
            for (const auto& result : value.results) {
                coords.push_back(result.value.uv);
            }
            return cad_uv_map::to_numpy_uv_array(coords);
        })
        .def("to_numpy_point_array", [](const cad_uv_map::SurfaceEvalResultBatch& value) {
            std::vector<cad_uv_map::Vec3> points;
            points.reserve(value.results.size());
            for (const auto& result : value.results) {
                points.push_back(result.value.point);
            }
            return cad_uv_map::to_numpy_vec3_array(points);
        })
        .def("to_numpy_normal_array", [](const cad_uv_map::SurfaceEvalResultBatch& value) {
            std::vector<cad_uv_map::Vec3> normals;
            normals.reserve(value.results.size());
            for (const auto& result : value.results) {
                normals.push_back(result.value.normal);
            }
            return cad_uv_map::to_numpy_vec3_array(normals);
        })
        .def("to_numpy_face_id_array", [](const cad_uv_map::SurfaceEvalResultBatch& value) {
            std::vector<std::int32_t> ids;
            ids.reserve(value.results.size());
            for (const auto& result : value.results) {
                ids.push_back(result.value.face_id);
            }
            return cad_uv_map::to_numpy_int32_array(ids);
        })
        .def("to_numpy_normal_defined_mask", [](const cad_uv_map::SurfaceEvalResultBatch& value) {
            std::vector<bool> flags;
            flags.reserve(value.results.size());
            for (const auto& result : value.results) {
                flags.push_back(result.value.normal_defined);
            }
            return cad_uv_map::to_numpy_bool_array(flags);
        });

    py::class_<cad_uv_map::MappedSampleRecord>(m, "MappedSampleRecord")
        .def_readonly("sample", &cad_uv_map::MappedSampleRecord::sample)
        .def_readonly("mapping", &cad_uv_map::MappedSampleRecord::mapping)
        .def_readonly("surface", &cad_uv_map::MappedSampleRecord::surface);

    py::class_<cad_uv_map::IndexedMappedSampleRecord>(m, "IndexedMappedSampleRecord")
        .def_readonly("index", &cad_uv_map::IndexedMappedSampleRecord::index)
        .def_readonly("value", &cad_uv_map::IndexedMappedSampleRecord::value);

    py::class_<cad_uv_map::MappedSampleBatch>(m, "MappedSampleBatch")
        .def(py::init<>())
        .def_readonly("records", &cad_uv_map::MappedSampleBatch::records)
        .def("to_numpy_low_uv_array", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<cad_uv_map::UvCoord> coords;
            coords.reserve(value.records.size());
            for (const auto& record : value.records) {
                coords.push_back(record.value.sample.uv);
            }
            return cad_uv_map::to_numpy_uv_array(coords);
        })
        .def("to_numpy_high_uv_array", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<cad_uv_map::UvCoord> coords;
            coords.reserve(value.records.size());
            for (const auto& record : value.records) {
                coords.push_back(record.value.mapping.high_uv);
            }
            return cad_uv_map::to_numpy_uv_array(coords);
        })
        .def("to_numpy_point_array", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<cad_uv_map::Vec3> points;
            points.reserve(value.records.size());
            for (const auto& record : value.records) {
                points.push_back(record.value.surface.point);
            }
            return cad_uv_map::to_numpy_vec3_array(points);
        })
        .def("to_numpy_normal_array", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<cad_uv_map::Vec3> normals;
            normals.reserve(value.records.size());
            for (const auto& record : value.records) {
                normals.push_back(record.value.surface.normal);
            }
            return cad_uv_map::to_numpy_vec3_array(normals);
        })
        .def("to_numpy_face_id_array", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<std::int32_t> ids;
            ids.reserve(value.records.size());
            for (const auto& record : value.records) {
                ids.push_back(record.value.mapping.high_face_id);
            }
            return cad_uv_map::to_numpy_int32_array(ids);
        })
        .def("to_numpy_status_array", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<cad_uv_map::MappingStatus> statuses;
            statuses.reserve(value.records.size());
            for (const auto& record : value.records) {
                statuses.push_back(record.value.mapping.status);
            }
            return cad_uv_map::to_numpy_status_array(statuses);
        })
        .def("to_numpy_normal_defined_mask", [](const cad_uv_map::MappedSampleBatch& value) {
            std::vector<bool> flags;
            flags.reserve(value.records.size());
            for (const auto& record : value.records) {
                flags.push_back(record.value.surface.normal_defined);
            }
            return cad_uv_map::to_numpy_bool_array(flags);
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
             const cad_uv_map::MappingContext* shared_context) {
              return cad_uv_map::map_brep_single_low_face_samples_to_high_faces(
                  static_cast<std::string>(low_brep_data),
                  static_cast<std::string>(high_brep_data),
                  low_face_id,
                  low_uv_samples,
                  shared_context);
          },
          py::arg("low_brep_data"),
          py::arg("high_brep_data"),
          py::arg("low_face_id"),
          py::arg("low_uv_samples"),
          py::arg("shared_context") = nullptr,
          "Map UV samples from one low face to nearest high faces.");

    m.def("map_brep_single_low_face_samples_to_high_faces_nearest",
          [](py::bytes low_brep_data,
             py::bytes high_brep_data,
             std::int32_t low_face_id,
             const std::vector<cad_uv_map::UvCoord>& low_uv_samples,
             const cad_uv_map::MappingContext* shared_context) {
              return cad_uv_map::map_brep_single_low_face_samples_to_high_faces(
                  static_cast<std::string>(low_brep_data),
                  static_cast<std::string>(high_brep_data),
                  low_face_id,
                  low_uv_samples,
                  shared_context);
          },
          py::arg("low_brep_data"),
          py::arg("high_brep_data"),
          py::arg("low_face_id"),
          py::arg("low_uv_samples"),
          py::arg("shared_context") = nullptr,
          "Map UV samples from one low face to nearest high faces.");

    m.def("map_brep_single_low_face_samples_to_high_faces_ray",
          [](py::bytes low_brep_data,
             py::bytes high_brep_data,
             std::int32_t low_face_id,
             const std::vector<cad_uv_map::UvCoord>& low_uv_samples,
             const cad_uv_map::MappingContext* shared_context) {
              return cad_uv_map::map_brep_single_low_face_samples_to_high_faces_ray(
                  static_cast<std::string>(low_brep_data),
                  static_cast<std::string>(high_brep_data),
                  low_face_id,
                  low_uv_samples,
                  shared_context);
          },
          py::arg("low_brep_data"),
          py::arg("high_brep_data"),
          py::arg("low_face_id"),
          py::arg("low_uv_samples"),
          py::arg("shared_context") = nullptr,
          "Map UV samples from one low face along the low-face normal rays.");

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
             const cad_uv_map::MappingContext* shared_context) {
              const TopoDS_Shape low_shape = cad_uv_map::read_brep_bytes(static_cast<std::string>(low_brep_data));
              const TopoDS_Shape high_shape = cad_uv_map::read_brep_bytes(static_cast<std::string>(high_brep_data));
              return cad_uv_map::map_and_evaluate_multiple_low_face_samples(
                  low_face_samples,
                  cad_uv_map::collect_faces(low_shape),
                  cad_uv_map::collect_faces(high_shape),
                  shared_context);
          },
          py::arg("low_brep_data"),
          py::arg("high_brep_data"),
          py::arg("low_face_samples"),
          py::arg("shared_context") = nullptr,
          "Run the full low-sample to mapping to surface-eval pipeline.");
}
