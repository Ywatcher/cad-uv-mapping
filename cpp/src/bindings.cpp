#include "cad_uv_map/mapping.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_native, m) {
    m.doc() = "OpenCascade-backed CAD UV mapping native module";

    py::class_<cad_uv_map::UvCoord>(m, "UvCoord")
        .def(py::init<>())
        .def_readwrite("u", &cad_uv_map::UvCoord::u)
        .def_readwrite("v", &cad_uv_map::UvCoord::v);

    py::class_<cad_uv_map::IndexedUvCoord>(m, "IndexedUvCoord")
        .def(py::init<>())
        .def_readwrite("index", &cad_uv_map::IndexedUvCoord::index)
        .def_readwrite("value", &cad_uv_map::IndexedUvCoord::value);

    py::class_<cad_uv_map::UvSample>(m, "UvSample")
        .def(py::init<>())
        .def_readwrite("face_id", &cad_uv_map::UvSample::face_id)
        .def_readwrite("uv", &cad_uv_map::UvSample::uv);

    py::class_<cad_uv_map::IndexedUvSample>(m, "IndexedUvSample")
        .def(py::init<>())
        .def_readwrite("index", &cad_uv_map::IndexedUvSample::index)
        .def_readwrite("value", &cad_uv_map::IndexedUvSample::value);

    py::class_<cad_uv_map::FaceUvSamples>(m, "FaceUvSamples")
        .def(py::init<>())
        .def_readwrite("face_id", &cad_uv_map::FaceUvSamples::face_id)
        .def_readwrite("samples", &cad_uv_map::FaceUvSamples::samples);

    py::class_<cad_uv_map::IndexedFaceUvSamples>(m, "IndexedFaceUvSamples")
        .def(py::init<>())
        .def_readwrite("index", &cad_uv_map::IndexedFaceUvSamples::index)
        .def_readwrite("value", &cad_uv_map::IndexedFaceUvSamples::value);

    py::class_<cad_uv_map::FaceUvSampleBatch>(m, "FaceUvSampleBatch")
        .def(py::init<>())
        .def_readwrite("faces", &cad_uv_map::FaceUvSampleBatch::faces);

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
        .def_readonly("low_u", &cad_uv_map::MappingResult::low_u)
        .def_readonly("low_v", &cad_uv_map::MappingResult::low_v)
        .def_readonly("high_face_id", &cad_uv_map::MappingResult::high_face_id)
        .def_readonly("high_u", &cad_uv_map::MappingResult::high_u)
        .def_readonly("high_v", &cad_uv_map::MappingResult::high_v)
        .def_readonly("point_x", &cad_uv_map::MappingResult::point_x)
        .def_readonly("point_y", &cad_uv_map::MappingResult::point_y)
        .def_readonly("point_z", &cad_uv_map::MappingResult::point_z)
        .def_readonly("distance", &cad_uv_map::MappingResult::distance)
        .def_readonly("status", &cad_uv_map::MappingResult::status);

    py::class_<cad_uv_map::IndexedMappingResult>(m, "IndexedMappingResult")
        .def_readonly("index", &cad_uv_map::IndexedMappingResult::index)
        .def_readonly("value", &cad_uv_map::IndexedMappingResult::value);

    py::class_<cad_uv_map::MappingBatch>(m, "MappingBatch")
        .def_readonly("results", &cad_uv_map::MappingBatch::results);

    py::class_<cad_uv_map::FaceInfo>(m, "FaceInfo")
        .def_readonly("face_id", &cad_uv_map::FaceInfo::face_id)
        .def_readonly("u_min", &cad_uv_map::FaceInfo::u_min)
        .def_readonly("u_max", &cad_uv_map::FaceInfo::u_max)
        .def_readonly("v_min", &cad_uv_map::FaceInfo::v_min)
        .def_readonly("v_max", &cad_uv_map::FaceInfo::v_max);

    /*
     * summary functions
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
          [](py::bytes brep_data, const cad_uv_map::FaceUvSampleBatch& samples, const std::string& label) {
              cad_uv_map::debug_print_brep_uv_sample_batch(static_cast<std::string>(brep_data), samples, label);
          },
          py::arg("brep_data"),
          py::arg("samples"),
          py::arg("label") = "BREP bytes + UV samples",
          "Read BREP bytes and print the provided grouped UV samples in C++.");

    m.def("debug_print_brep_uv_samples",
          [](py::bytes brep_data, const std::vector<cad_uv_map::UvSample>& samples, const std::string& label) {
              cad_uv_map::debug_print_brep_uv_samples(static_cast<std::string>(brep_data), samples, label);
          },
          py::arg("brep_data"),
          py::arg("samples"),
          py::arg("label") = "BREP bytes + flat UV samples",
          "Read BREP bytes and print the provided flat UV samples in C++.");

    m.def("map_brep_low_face_samples_to_high_faces",
          [](py::bytes low_brep_data,
             py::bytes high_brep_data,
             std::int32_t low_face_id,
             const std::vector<cad_uv_map::UvCoord>& low_uv_samples,
             const cad_uv_map::MappingContext* shared_context) {
              return cad_uv_map::map_brep_low_face_samples_to_high_faces(
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
}
