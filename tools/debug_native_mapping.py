from __future__ import annotations

import argparse
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from cad_uv_map import describe_shape_faces, map_source_samples_to_target  # noqa: E402
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
    batch = map_source_samples_to_target(pair.low, pair.high, face_info.face_id, samples)

    print(
        f"case={args.case} source_face_id={face_info.face_id} sample_count={len(samples)} result_count={len(batch)}",
        flush=True,
    )
    for row_index in range(len(batch)):
        row = batch.row(row_index)
        print(
            "index={index} low=({low_u:.6g}, {low_v:.6g}) "
            "high_face={high_face} high_uv=({high_u:.6g}, {high_v:.6g}) "
            "point=({x:.6g}, {y:.6g}, {z:.6g}) distance={distance:.6g} status={status}".format(
                index=row.index,
                low_u=row.low_uv[0],
                low_v=row.low_uv[1],
                high_face=row.high_face_id,
                high_u=row.high_uv[0],
                high_v=row.high_uv[1],
                x=row.point[0],
                y=row.point[1],
                z=row.point[2],
                distance=row.distance,
                status=_status_name(row.status),
            ),
            flush=True,
        )


if __name__ == "__main__":
    main()
