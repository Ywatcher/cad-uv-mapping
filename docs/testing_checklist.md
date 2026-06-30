# Testing checklist: thread-safety fix

Run these checks before and after the fix. Each section says what to run,
what a pass looks like, and what a failure means.

---

## 0. Prerequisites

Build and install the package in editable mode (needed once, or after any C++ change):

```bash
cd /mnt/D/Games/cad-uv-map
pip install -e ".[dev]" --no-build-isolation
```

Activate the right environment first (`conda activate game-qt6-py312` or equivalent).

---

## 1. Baseline: existing tests still pass

Run the full existing test suite. This must pass both before and after the fix.
If it fails after the fix, something was broken.

```bash
pytest tests/python/ -v
```

Expected: `17 passed` (or more if new tests were added). Zero failures.

---

## 2. Crash regression test

This is the specific case that caused the SIGILL crash on 2026-06-29.

```bash
pytest tests/python/test_thread_safety.py::test_crash_regression_ray_bidirectional_pedestal_nofold -v
```

**Before the fix:** may crash the Python process entirely (no pytest output,
terminal dies or prints `Illegal instruction`). That is the bug.

**After the fix:** must print `PASSED`. If it prints `FAILED` (a Python-level
assertion error, not a crash), the result is wrong — the fix changed behaviour.

---

## 3. Correctness: parallel == serial

Verifies the fix did not change any computed values.

```bash
pytest tests/python/test_thread_safety.py -v -k "parallel_matches_serial"
```

Expected: all 6 parametrized cases pass (2 shapes × 3 methods).

If any case fails with a numerical difference, the implementation of the fix
changed the algorithm, not just the threading. Investigate before shipping.

---

## 4. Stress test (slow, ~5–10 minutes)

Runs many iterations to force the race window open. A race that does not crash
in test 2 may still appear here.

```bash
pytest tests/python/test_thread_safety.py -v -m stress
```

Expected: all 4 stress tests pass. This takes several minutes — normal.

If a stress test crashes the process (rather than failing with an assertion),
the fix is incomplete. If it raises a Python-level assertion error, a result
is wrong under concurrency.

---

## 5. ThreadSanitizer build (strongest check)

TSan instruments every memory access and reports data races at runtime,
even ones that do not crash in a given run. This catches races that stress
testing might miss due to lucky thread scheduling.

### 5a. Build with TSan

```bash
cd /mnt/D/Games/cad-uv-map
# fresh build directory — TSan and normal builds cannot share one
cmake -B build-tsan \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_TSAN=ON \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DPython3_EXECUTABLE=$(which python)
cmake --build build-tsan -- -j$(nproc)
cp build-tsan/_native*.so python/cad_uv_map/
```

### 5b. Run tests under TSan

TSan reports go to stderr. Run with output not captured so you see them:

```bash
TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" \
    pytest tests/python/test_thread_safety.py -v -m stress -s 2>&1 | tee tsan_output.txt
```

### 5c. Reading the output

A data race report looks like:

```
WARNING: ThreadSanitizer: data race (pid=...)
  Write of size 8 at 0x... by thread T2:
    #0 Geom_BSplineSurface::...
    ...
  Previous read of size 8 at 0x... by thread T1:
    #0 IntCurvesFace_ShapeIntersector::Load
    ...
```

**After the fix:** no reports mentioning `IntCurvesFace`, `BRepAdaptor_Surface`,
`Geom_BSplineSurface`, `BRepExtrema`, or any cad_uv_map symbol.

You may see reports from CPython internals (GIL, allocator). Those are
pre-existing Python false positives — ignore them unless the stack trace
mentions cad_uv_map C++ code.

### 5d. Restore normal build

```bash
pip install -e ".[dev]" --no-build-isolation
```

---

## 6. AddressSanitizer (optional, catches memory corruption)

ASan is useful if the SIGILL was caused by memory corruption (corrupted code
being executed) rather than a pure race. Build and run similarly to TSan:

```bash
cmake -B build-asan \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_ASAN=ON \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DPython3_EXECUTABLE=$(which python)
cmake --build build-asan -- -j$(nproc)
cp build-asan/_native*.so python/cad_uv_map/

ASAN_OPTIONS="detect_leaks=0" \
    pytest tests/python/test_thread_safety.py -v -s 2>&1 | tee asan_output.txt
```

Note: TSan and ASan cannot be combined in the same build.

---

## Summary: what constitutes a passing fix

| Check | Command | Pass condition |
|---|---|---|
| Existing tests | `pytest tests/python/ -v` | 17+ passed, 0 failed |
| Crash regression | `pytest ... ::test_crash_regression...` | PASSED, process alive |
| Correctness | `pytest ... -k parallel_matches_serial` | 6/6 passed |
| Stress | `pytest ... -m stress` | 4/4 passed |
| TSan | build-tsan + stress run | No cad_uv_map races in output |
