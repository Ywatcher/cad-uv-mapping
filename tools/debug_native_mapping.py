from __future__ import annotations

import argparse
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from cad_uv_map import describe_shape_faces, map_shape_single_low_face_samples_to_high_faces  # noqa: E402
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
    samples: list[tuple[float, float]] = []
    for v_index in range(v_count):
        v_t = (v_index + margin) / v_count
        v = face_info.v_min + (face_info.v_max - face_info.v_min) * v_t
        for u_index in range(u_count):
            u_t = (u_index + margin) / u_count
            u = face_info.u_min + (face_info.u_max - face_info.u_min) * u_t
            samples.append((u, v))
    return samples


def _status_name(status) -> str:
    name = getattr(status, "name", None)
    if name is not None:
        return str(name)
    return str(status)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run native C++ nearest-face mapping for one low face.")
    parser.add_argument("--case", choices=sorted(CASE_FACTORIES), default="flat_to_v_groove")
    parser.add_argument("--low-face-index", type=int, default=0)
    parser.add_argument("--u-count", type=int, default=3)
    parser.add_argument("--v-count", type=int, default=3)
    parser.add_argument("--margin", type=float, default=0.5)
    args = parser.parse_args()

    pair = CASE_FACTORIES[args.case]()
    low_face_infos = describe_shape_faces(pair.low)
    if args.low_face_index < 0 or args.low_face_index >= len(low_face_infos):
        raise IndexError(f"low face index {args.low_face_index} is out of range")

    face_info = low_face_infos[args.low_face_index]
    samples = _sample_uv_grid(face_info, args.u_count, args.v_count, args.margin)
    batch = map_shape_single_low_face_samples_to_high_faces(pair.low, pair.high, face_info.face_id, samples)

    print(
        f"case={args.case} low_face_id={face_info.face_id} sample_count={len(samples)} result_count={len(batch.results)}",
        flush=True,
    )
    for result in batch.results:
        value = result.value
        print(
            "index={index} low=({low_u:.6g}, {low_v:.6g}) "
            "high_face={high_face} high_uv=({high_u:.6g}, {high_v:.6g}) "
            "point=({x:.6g}, {y:.6g}, {z:.6g}) distance={distance:.6g} status={status}".format(
                index=result.index,
                low_u=value.low_u,
                low_v=value.low_v,
                high_face=value.high_face_id,
                high_u=value.high_u,
                high_v=value.high_v,
                x=value.point_x,
                y=value.point_y,
                z=value.point_z,
                distance=value.distance,
                status=_status_name(value.status),
            ),
            flush=True,
        )


if __name__ == "__main__":
    main()
