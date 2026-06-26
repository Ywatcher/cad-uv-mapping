# How To Reproduce Tests

This document lists the commands used to verify the current `cad-uv-map`
implementation.

## Environment

Use the dedicated `cad-uv-map` conda environment for the native C++ path.

## Install Editable Package

Build and install the native extension after C++ changes:

```bash
OpenCASCADE_DIR=/mnt/D/packages/miniconda3/envs/cad-uv-map/lib/cmake/opencascade \
/mnt/D/packages/miniconda3/envs/cad-uv-map/bin/python -m pip install -e . --no-build-isolation
```

## Run Python Tests

Run the full Python test suite with source-path import enabled and pytest cache
redirected to `/tmp`:

```bash
env PYTHONPATH=python PYTHONPYCACHEPREFIX=/tmp/codex_pycache \
/mnt/D/packages/miniconda3/envs/cad-uv-map/bin/python -m pytest tests/python \
-o cache_dir=/tmp/cad_uv_pytest_cache
```

Expected result:

```text
8 passed, 7 warnings
```

The warnings come from third-party `ezdxf` / `pyparsing` deprecations.

## Run Native Face Debug

Use the in-memory BREP bridge to print native C++ face information from a Python
fixture shape:

```bash
/mnt/D/packages/miniconda3/envs/cad-uv-map/bin/python tools/debug_native_faces.py \
  --case flat_to_v_groove \
  --shape high
```

This uses the default `bytes` bridge and should print a face list to stdout.

If you want the file-backed version for comparison, use:

```bash
/mnt/D/packages/miniconda3/envs/cad-uv-map/bin/python tools/debug_native_faces.py \
  --case flat_to_v_groove \
  --shape high \
  --mode brep
```

## Run A Single Comparison Test

To compare the manual reference against the CAD-derived reference:

```bash
env PYTHONPATH=python PYTHONPYCACHEPREFIX=/tmp/codex_pycache \
/mnt/D/packages/miniconda3/envs/cad-uv-map/bin/python -m pytest \
tests/python/test_reference_method_comparison.py \
-o cache_dir=/tmp/cad_uv_pytest_cache
```

## Notes

- The native bridge is now face-first internally.
- The runtime-friendly path uses in-memory BREP bytes instead of direct Python
  wrapper passing.
- If you edit C++ code, rerun the editable install command before running tests.
