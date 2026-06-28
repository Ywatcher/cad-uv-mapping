#pragma once

#include "cad_uv_map/pipeline.hpp"

#include <pybind11/numpy.h>

namespace cad_uv_map {

pybind11::array_t<double> to_numpy_uv_array(const std::vector<UvCoord>& values);
pybind11::array_t<double> to_numpy_vec3_array(const std::vector<Vec3>& values);
pybind11::array_t<std::int64_t> to_numpy_index_array(const std::vector<std::size_t>& values);
pybind11::array_t<std::int32_t> to_numpy_int32_array(const std::vector<std::int32_t>& values);
pybind11::array_t<std::int32_t> to_numpy_status_array(const std::vector<MappingStatus>& values);
pybind11::array_t<bool> to_numpy_bool_array(const std::vector<bool>& values);
pybind11::array_t<double> to_numpy_indexed_uv_array(const std::vector<IndexedUvCoord>& values);

} // namespace cad_uv_map
