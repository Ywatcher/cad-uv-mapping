from __future__ import annotations

import argparse
import sys
from pathlib import Path

from build123d import export_brep

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

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


def main() -> None:
    parser = argparse.ArgumentParser(description="Print native C++ face debug info for a Python CAD fixture.")
    parser.add_argument("--case", choices=sorted(CASE_FACTORIES), default="flat_to_v_groove")
    parser.add_argument("--shape", choices=["low", "high"], default="low")
    parser.add_argument("--mode", choices=["bytes", "brep"], default="bytes")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("/tmp") / "cad_uv_map_debug" / "debug_native_faces.brep",
    )
    args = parser.parse_args()

    pair = CASE_FACTORIES[args.case]()
    shape = pair.low if args.shape == "low" else pair.high
    label = f"{args.case}.{args.shape}"

    if args.mode == "bytes":
        from cad_uv_map import debug_print_shape_faces

        print(f"passing in-memory BREP bytes for {label}", flush=True)
        debug_print_shape_faces(shape, label)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        export_brep(shape, str(args.output))
        if not args.output.exists():
            raise RuntimeError(f"export_brep did not create {args.output}")

        from cad_uv_map import debug_print_brep_faces

        print(f"exported {label} to {args.output}", flush=True)
        debug_print_brep_faces(args.output)


if __name__ == "__main__":
    main()
