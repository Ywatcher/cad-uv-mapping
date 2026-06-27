#pragma once

#include <cstddef>
#include <vector>

namespace cad_uv_map {

/*
 * IndexedRecord carries a stable output/input index together with a payload.
 *
 * This keeps ordering metadata separate from the CAD payload itself and helps
 * preserve array order when samples are grouped, parallelized, or merged across
 * multiple stages.
 */
template <typename T>
struct IndexedRecord {
    std::size_t index;
    T value;
};

template <typename T>
using IndexedRecords = std::vector<IndexedRecord<T>>;

} // namespace cad_uv_map
