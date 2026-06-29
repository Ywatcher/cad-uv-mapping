# Native NumPy Batch Round-Trip Report

## Scope

This note records the batch-array bridge problem in `cad_uv_map` and how it was
fixed.

The issue was not the mapping algorithm itself. It was the path from Python batch
wrappers to native C++ batch exporters and back to NumPy arrays.

## What Broke

The Python wrapper classes for batch records were calling `to_native()` and then
expecting the native batch object to export NumPy arrays.

That exposed two problems:

1. The native batch classes did not initially have default constructors bound in
   pybind11.
2. Some wrapper methods had been temporarily rewritten to build arrays in Python
   instead of using the native C++ batch exporters.

The result was an unstable round trip:

```text
Python wrapper batch -> native batch -> NumPy
```

## Symptoms

The failure showed up as an import/runtime mismatch when the wrapper tried to use
the native batch path.

The important part of the problem was:

- the batch object existed in Python
- the wrapper could construct a matching native value in principle
- but the native batch class was not safely constructible through the pybind layer

That meant the wrapper NumPy methods were relying on a bridge that was not fully
wired up.

## How It Was Fixed

The fix was to make the bridge consistent again:

1. Bind default constructors for the native batch classes:
   - `MappingResultBatch`
   - `SurfaceEvalResultBatch`
   - `MappedSampleBatch`
2. Keep the batch-array packing logic in native C++.
3. Keep the Python wrapper methods as thin adapters over the native batch path.

After that, the wrapper path became:

```text
Python wrapper batch -> native batch constructor -> native C++ NumPy exporter
```

This restored the intended division of labor:

- Python handles input normalization and wrapper typing
- C++ handles batch packing and array construction

## What Is Different From the Earlier Implementation

The earlier implementation drifted in two ways:

- some array exports were being rebuilt directly in Python
- the native batch constructors were missing, so the round trip was incomplete

The current implementation differs because:

- the native batch constructors are bound
- the native `to_numpy_*` batch methods are available again
- the Python wrappers now route through a valid native bridge instead of inventing
  a parallel array-building path

## Why This Is Better

- it keeps the array packing work in C++
- it preserves one data model across Python and native code
- it avoids duplicate Python-side packing logic
- it makes the wrapper methods predictable for demo code

## Verification

The native module rebuild completed successfully after the binding changes.

The Python wrapper code also compiled cleanly after the NumPy export methods were
returned to the native path.

## Expected Demo Feedback

When the `agent_game` demos test this, I expect the feedback to focus on behavior
at the demo layer, not on the round-trip bridge itself.

Likely good results:

- batch NumPy arrays have the expected shapes
- sample order is preserved
- the demos no longer need to rebuild arrays sample-by-sample

Likely remaining issues if something still fails:

- a demo is calling the wrong wrapper method for the returned batch type
- the demo expects a structured array where the current API returns split arrays
- the runtime is loading an old native binary instead of the rebuilt one
- a demo has its own local import or plumbing error unrelated to the batch bridge

## Practical Conclusion

The round-trip problem is fixed on the code side.

The batch wrappers can now construct native batch values and use the native C++
NumPy exporters again, which is the intended design for the current API.
