#include "cad_uv_map/numpy_exports.hpp"

namespace cad_uv_map {

namespace {

template <typename T, typename Fn>
pybind11::array_t<T> make_array_1d(std::size_t size, Fn&& fill) {
    pybind11::array_t<T> array({static_cast<pybind11::ssize_t>(size)});
    auto view = array.template mutable_unchecked<1>();
    for (std::size_t i = 0; i < size; ++i) {
        view(i) = fill(i);
    }
    return array;
}

template <typename Fn>
pybind11::array_t<double> make_array_2d(std::size_t rows, std::size_t cols, Fn&& fill) {
    pybind11::array_t<double> array({
        static_cast<pybind11::ssize_t>(rows),
        static_cast<pybind11::ssize_t>(cols),
    });
    auto view = array.template mutable_unchecked<2>();
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            view(r, c) = fill(r, c);
        }
    }
    return array;
}

} // namespace

pybind11::array_t<double> to_numpy_uv_array(const std::vector<UvCoord>& values) {
    return make_array_2d(values.size(), 2, [&](std::size_t row, std::size_t col) {
        return col == 0 ? values[row].u : values[row].v;
    });
}

pybind11::array_t<double> to_numpy_vec3_array(const std::vector<Vec3>& values) {
    return make_array_2d(values.size(), 3, [&](std::size_t row, std::size_t col) {
        if (col == 0) {
            return values[row].x;
        }
        if (col == 1) {
            return values[row].y;
        }
        return values[row].z;
    });
}

pybind11::array_t<std::int64_t> to_numpy_index_array(const std::vector<std::size_t>& values) {
    return make_array_1d<std::int64_t>(values.size(), [&](std::size_t row) {
        return static_cast<std::int64_t>(values[row]);
    });
}

pybind11::array_t<std::int32_t> to_numpy_int32_array(const std::vector<std::int32_t>& values) {
    return make_array_1d<std::int32_t>(values.size(), [&](std::size_t row) {
        return values[row];
    });
}

pybind11::array_t<std::int32_t> to_numpy_status_array(const std::vector<MappingStatus>& values) {
    return make_array_1d<std::int32_t>(values.size(), [&](std::size_t row) {
        return static_cast<std::int32_t>(values[row]);
    });
}

pybind11::array_t<bool> to_numpy_bool_array(const std::vector<bool>& values) {
    return make_array_1d<bool>(values.size(), [&](std::size_t row) {
        return values[row];
    });
}

pybind11::array_t<double> to_numpy_indexed_uv_array(const std::vector<IndexedUvCoord>& values) {
    return make_array_2d(values.size(), 3, [&](std::size_t row, std::size_t col) {
        if (col == 0) {
            return static_cast<double>(values[row].index);
        }
        if (col == 1) {
            return values[row].value.u;
        }
        return values[row].value.v;
    });
}

} // namespace cad_uv_map
