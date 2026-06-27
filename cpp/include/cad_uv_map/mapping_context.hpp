#pragma once

namespace cad_uv_map {

/*
 * MappingContext carries optional batch-level policy and cache hints.
 *
 * The core single-face function should work without this object. The batch
 * wrapper can pass one in when it wants shared tolerances, cache ownership, or
 * future parallel execution hints.
 *
 * TODO: add explicit cache handles only when we know which OCCT objects are
 * safe to reuse across samples or threads.
 */
struct MappingContext {
    double tolerance = 1e-7;
    bool enable_parallel = false;
    bool preserve_input_order = true;
};

} // namespace cad_uv_map
