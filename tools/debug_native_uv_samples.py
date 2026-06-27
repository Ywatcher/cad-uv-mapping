from __future__ import annotations

import argparse
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from cad_uv_map import (  # noqa: E402
    debug_print_shape_uv_sample_batch,
    debug_print_shape_uv_samples,
    describe_shape_faces,
    normalize_face_uv_samples,
)
from tests.fixtures.cad_cases import (  # noqa: E402
    flat_to_u_groove_pair,
    flat_to_v_groove_pair,
    identity_box_pair,
    pedestal_ribs_pair,
)


CASE_FACTORIES = {
    "identity_box": identity_box_pair,
    "flat_to_u_groove": flat_to_u_groove_pair,
    "flat_to_v_groove": flat_to_v_groove_pair,
    "pedestal_ribs": pedestal_ribs_pair,
}


def _sample_uv_grid(face_info, u_count: int, v_count: int, margin: float) -> list[tuple[float, float]]:
    u_min = float(face_info.u_min)
    u_max = float(face_info.u_max)
    v_min = float(face_info.v_min)
    v_max = float(face_info.v_max)
    samples: list[tuple[float, float]] = []

    for v_index in range(v_count):
        v_t = (v_index + margin) / v_count
        v = v_min + (v_max - v_min) * v_t
        for u_index in range(u_count):
            u_t = (u_index + margin) / u_count
            u = u_min + (u_max - u_min) * u_t
            samples.append((u, v))

    return samples


def _build_sample_inputs(face_id: int, uvs: list[tuple[float, float]]):
    flat = [(face_id, u, v) for u, v in uvs]
    grouped = {face_id: uvs}
    batch = normalize_face_uv_samples(grouped)
    return flat, grouped, batch


def main() -> None:
    parser = argparse.ArgumentParser(description="Debug Python UV sample inputs through the native C++ bridge.")
    parser.add_argument("--case", choices=sorted(CASE_FACTORIES), default="flat_to_v_groove")
    parser.add_argument("--shape", choices=["low", "high"], default="low")
    parser.add_argument("--face-index", type=int, default=0)
    parser.add_argument("--u-count", type=int, default=3)
    parser.add_argument("--v-count", type=int, default=3)
    parser.add_argument("--margin", type=float, default=0.5)
    parser.add_argument("--mode", choices=["all", "flat", "grouped", "batch"], default="all")
    args = parser.parse_args()

    pair = CASE_FACTORIES[args.case]()
    shape = pair.low if args.shape == "low" else pair.high
    face_infos = describe_shape_faces(shape)
    if args.face_index < 0 or args.face_index >= len(face_infos):
        raise IndexError(f"face index {args.face_index} is out of range for {args.case}.{args.shape}")

    face_info = face_infos[args.face_index]
    uvs = _sample_uv_grid(face_info, args.u_count, args.v_count, args.margin)
    flat_samples, grouped_samples, native_batch = _build_sample_inputs(face_info.face_id, uvs)

    print(f"shape={args.case}.{args.shape} face_id={face_info.face_id} sample_count={len(uvs)}", flush=True)

    if args.mode in {"all", "flat"}:
        print("mode=flat", flush=True)
        debug_print_shape_uv_samples(shape, flat_samples, f"{args.case}.{args.shape}.flat")

    if args.mode in {"all", "grouped"}:
        print("mode=grouped", flush=True)
        debug_print_shape_uv_sample_batch(shape, grouped_samples, f"{args.case}.{args.shape}.grouped")

    if args.mode in {"all", "batch"}:
        print("mode=batch", flush=True)
        debug_print_shape_uv_sample_batch(shape, native_batch, f"{args.case}.{args.shape}.native_batch")

    print(f"group_count={len(grouped_samples)}", flush=True)


if __name__ == "__main__":
    main()
