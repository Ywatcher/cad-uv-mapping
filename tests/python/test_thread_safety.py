"""
Thread-safety and correctness tests for the parallel projection path.

Normal run (fast, ~seconds):
    pytest tests/python/test_thread_safety.py -v

Stress run (slow, minutes — use before/after the fix):
    pytest tests/python/test_thread_safety.py -v -m stress

ThreadSanitizer run (catches races that don't always crash):
    build with -DENABLE_TSAN=ON, then same commands above.
    See docs/testing_checklist.md for full instructions.
"""

import threading

import numpy as np
import pytest

from cad_uv_map import (
    MappingMethod,
    MappingStatus,
    describe_shape_faces,
    map_source_samples_to_target,
)
from cad_uv_map.api import MappingContext
from tests.fixtures.cad_cases import (
    all_pairs,
    identity_box_pair,
    pedestal_ribs_nofold_pair,
)

HIT = MappingStatus.hit.value
ALL_METHODS = [MappingMethod.nearest, MappingMethod.ray, MappingMethod.ray_bidirectional]


# ── helpers ───────────────────────────────────────────────────────────────────

def _grid(face_info, n: int) -> list:
    """Uniform UV grid of n×n samples with half-cell margin."""
    samples = []
    for vi in range(n):
        v = face_info.v_min + (face_info.v_max - face_info.v_min) * (vi + 0.5) / n
        for ui in range(n):
            u = face_info.u_min + (face_info.u_max - face_info.u_min) * (ui + 0.5) / n
            samples.append((u, v))
    return samples


def _side_face(pair):
    """Return the face with the widest u-parameter range from the low shape.

    For pedestal-style models this picks the cylindrical side face (u≈2π).
    For box models it picks one of the larger rectangular faces.
    Falls back to face 0 if the list is empty.
    """
    faces = describe_shape_faces(pair.low)
    return max(faces, key=lambda f: f.u_max - f.u_min)


def _ctx(parallel: bool) -> MappingContext:
    ctx = MappingContext()
    ctx.enable_parallel = parallel
    return ctx


# ── 1. crash regression ───────────────────────────────────────────────────────

def test_crash_regression_ray_bidirectional_pedestal_nofold():
    """
    Regression for the SIGILL crash (PID 173372, 2026-06-29):
        python visualize_qt.py --case pedestal_ribs_nofold --method ray_bidirectional

    If this test crashes the process (SIGILL / SIGABRT) rather than raising a
    Python exception, the fix is not effective.
    Run under TSan to detect the race even when it does not happen to crash.
    """
    pair = pedestal_ribs_nofold_pair()
    face = _side_face(pair)
    samples = _grid(face, n=16)  # 256 samples — similar density to original crash
    ctx = _ctx(parallel=True)

    result = map_source_samples_to_target(
        pair.low, pair.high, face.face_id, samples, MappingMethod.ray_bidirectional, ctx
    )

    assert len(result) == len(samples)
    assert any(result.status == HIT)


# ── 2. correctness: parallel must equal serial ────────────────────────────────

@pytest.mark.parametrize("pair_fn", [identity_box_pair, pedestal_ribs_nofold_pair])
@pytest.mark.parametrize("method", ALL_METHODS)
def test_parallel_matches_serial(pair_fn, method):
    """
    Parallel results must be numerically identical to single-threaded results.
    A difference here means the fix changed the algorithm, not just the threading.
    """
    pair = pair_fn()
    face = describe_shape_faces(pair.low)[0]
    samples = _grid(face, n=8)  # 64 samples

    serial   = map_source_samples_to_target(
        pair.low, pair.high, face.face_id, samples, method, _ctx(False)
    )
    parallel = map_source_samples_to_target(
        pair.low, pair.high, face.face_id, samples, method, _ctx(True)
    )

    assert len(serial) == len(parallel)
    np.testing.assert_array_equal(serial.status,       parallel.status)
    np.testing.assert_array_equal(serial.high_face_id, parallel.high_face_id)
    np.testing.assert_allclose(serial.high_uv,         parallel.high_uv,   atol=1e-10)
    np.testing.assert_allclose(serial.distance,        parallel.distance,  atol=1e-10)


# ── 3. stress: repeated calls ─────────────────────────────────────────────────

@pytest.mark.stress
@pytest.mark.parametrize("method", ALL_METHODS)
def test_stress_repeated_parallel_calls(method):
    """
    Run many parallel calls in a tight loop.
    Race conditions are probabilistic; repetition forces the window to open.
    Under TSan this will report races even if they do not crash in this run.
    """
    pair = pedestal_ribs_nofold_pair()
    face = _side_face(pair)
    samples = _grid(face, n=20)  # 400 samples
    ctx = _ctx(parallel=True)

    for _ in range(200):
        result = map_source_samples_to_target(
            pair.low, pair.high, face.face_id, samples, method, ctx
        )
        assert len(result) == len(samples)


@pytest.mark.stress
def test_stress_concurrent_python_threads():
    """
    Call the mapping API from multiple Python threads at the same time.
    Each Python thread drives its own C++ worker pool, maximising concurrency pressure.
    This is the strongest test: nested parallelism from Python + C++ levels.
    """
    pair = pedestal_ribs_nofold_pair()
    face = _side_face(pair)
    samples = _grid(face, n=16)  # 256 samples
    ctx = _ctx(parallel=True)
    errors: list = []

    def worker():
        try:
            for _ in range(50):
                result = map_source_samples_to_target(
                    pair.low, pair.high, face.face_id,
                    samples, MappingMethod.ray_bidirectional, ctx
                )
                assert len(result) == len(samples)
        except Exception as exc:
            errors.append(exc)

    threads = [threading.Thread(target=worker) for _ in range(4)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert not errors, f"Errors from threads: {errors}"


# ── 4. correctness across all fixture pairs ───────────────────────────────────

@pytest.mark.stress
@pytest.mark.parametrize("method", ALL_METHODS)
def test_stress_all_pairs_parallel_matches_serial(method):
    """Serial == parallel on every fixture shape, not just the crash case."""
    for pair in all_pairs():
        face = describe_shape_faces(pair.low)[0]
        samples = _grid(face, n=6)

        serial   = map_source_samples_to_target(
            pair.low, pair.high, face.face_id, samples, method, _ctx(False)
        )
        parallel = map_source_samples_to_target(
            pair.low, pair.high, face.face_id, samples, method, _ctx(True)
        )

        np.testing.assert_array_equal(
            serial.status, parallel.status,
            err_msg=f"status mismatch on {pair.name} / {method}",
        )
        np.testing.assert_allclose(
            serial.high_uv, parallel.high_uv, atol=1e-10,
            err_msg=f"UV mismatch on {pair.name} / {method}",
        )
