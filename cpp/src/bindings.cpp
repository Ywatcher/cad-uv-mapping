#include "cad_uv_map/mapping.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_native, m) {
    m.doc() = "OpenCascade-backed CAD UV mapping native module";

    py::class_<cad_uv_map::FaceInfo>(m, "FaceInfo")
        .def_readonly("face_id", &cad_uv_map::FaceInfo::face_id)
        .def_readonly("u_min", &cad_uv_map::FaceInfo::u_min)
        .def_readonly("u_max", &cad_uv_map::FaceInfo::u_max)
        .def_readonly("v_min", &cad_uv_map::FaceInfo::v_min)
        .def_readonly("v_max", &cad_uv_map::FaceInfo::v_max);

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
}
