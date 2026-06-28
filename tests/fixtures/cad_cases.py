from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from build123d import (
    Axis,
    Box,
    BuildLine,
    BuildPart,
    BuildSketch,
    Circle,
    Compound,
    Cylinder,
    Ellipse,
    Line,
    Plane,
    ThreePointArc,
    add,
    export_brep,
    extrude,
    make_face,
    revolve,
    sweep,
)


@dataclass(frozen=True)
class CadPair:
    name: str
    low: Compound
    high: Compound


class LowResCuboid(Compound):
    def __init__(self):
        box = Box(20.0, 20.0, 10.0)
        super().__init__(box.wrapped)


class HighResCarvedCuboid(Compound):
    def __init__(self):
        base = Box(20.0, 20.0, 10.0)
        groove_ring = Cylinder(radius=5.5, height=2.0) - Cylinder(radius=4.5, height=3.0)
        groove_ring = groove_ring.translate((0.0, 0.0, 4.5))
        groove_h = Box(16.0, 1.0, 2.0).translate((0.0, 0.0, 4.5))
        groove_v = Box(1.0, 16.0, 2.0).translate((0.0, 0.0, 4.5))
        carved = base - groove_ring - groove_h - groove_v
        super().__init__(carved.wrapped)


class HighResVCarvedCuboid(Compound):
    def __init__(self):
        base = Box(20.0, 20.0, 10.0)

        with BuildPart() as v_groove_ring_builder:
            with BuildSketch(Plane.XZ):
                with BuildLine():
                    Line((4.5, 5.0), (5.5, 5.0))
                    Line((5.5, 5.0), (5.0, 4.5))
                    Line((5.0, 4.5), (4.5, 5.0))
                make_face()
            revolve(axis=Axis.Z)
        v_groove_ring = v_groove_ring_builder.part

        with BuildPart() as v_groove_h_builder:
            with BuildSketch(Plane.YZ):
                with BuildLine():
                    Line((-0.5, 5.0), (0.5, 5.0))
                    Line((0.5, 5.0), (0.0, 4.5))
                    Line((0.0, 4.5), (-0.5, 5.0))
                make_face()
            extrude(amount=11.0, both=True)
        v_groove_h = v_groove_h_builder.part

        with BuildPart() as v_groove_v_builder:
            with BuildSketch(Plane.XZ):
                with BuildLine():
                    Line((-0.5, 5.0), (0.5, 5.0))
                    Line((0.5, 5.0), (0.0, 4.5))
                    Line((0.0, 4.5), (-0.5, 5.0))
                make_face()
            extrude(amount=11.0, both=True)
        v_groove_v = v_groove_v_builder.part

        carved = base - v_groove_ring - v_groove_h - v_groove_v
        super().__init__(carved.wrapped)


class LowResPedestal(Compound):
    def __init__(self):
        with BuildPart() as builder:
            with BuildSketch(Plane.XZ):
                with BuildLine():
                    Line((0.0, 0.0), (10.0, 0.0))
                    ThreePointArc((10.0, 0.0), (6.0, 7.5), (5.0, 15.0))
                    Line((5.0, 15.0), (0.0, 15.0))
                    Line((0.0, 15.0), (0.0, 0.0))
                make_face()
            revolve(axis=Axis.Z)
        super().__init__(builder.part.wrapped)


class HighResRibbedPedestal(Compound):
    def __init__(self):
        base_pedestal = LowResPedestal()

        # Rounded raised ribs: sweep one elliptical profile along an outward offset
        # of the pedestal's concave side curve, then polar-pattern it around Z.
        # The path must be built on Plane.XZ; a default-XY path creates unrelated
        # geometry and was the reason an earlier swept cutter looked like a no-op.
        #
        # Keep the offset smaller than the profile size so the ellipse intersects
        # the base pedestal. This makes the ridge emerge from the surface instead
        # of sitting tangent to it.
        rib_offset = 0.22
        rib_tangent_radius = 0.42
        rib_vertical_radius = 0.60
        with BuildPart() as single_rib:
            with BuildLine(Plane.XZ) as rib_path:
                ThreePointArc(
                    (10.0 + rib_offset, 0.0),
                    (6.0 + rib_offset, 7.5),
                    (5.0 + rib_offset, 15.0),
                )
            with BuildSketch(Plane.YZ.offset(10.0 + rib_offset)):
                Ellipse(x_radius=rib_tangent_radius, y_radius=rib_vertical_radius)
            sweep(path=rib_path.line)

        rib_clip = Box(24.0, 24.0, 15.0).translate((0.0, 0.0, 7.5))
        ribs = [
            single_rib.part.rotate(Axis.Z, i * (360.0 / 16)) & rib_clip
            for i in range(16)
        ]
        with BuildPart() as ribbed_builder:
            add(base_pedestal)
            add(ribs)

        ribbed = ribbed_builder.part
        super().__init__(ribbed.wrapped)


class HighResRibbedPedestalNoFold(Compound):
    def __init__(self):
        base_pedestal = LowResPedestal()

        # This variant keeps the ribbed pedestal silhouette but removes the
        # offset loop that introduced an undercut fold in the earlier fixture.
        # The rib follows the exact pedestal side curve and uses a small circular
        # profile, so projection samples should see a single outward-facing value.
        rib_offset = 0.12
        rib_profile_radius = 0.22
        with BuildPart() as single_rib:
            with BuildLine(Plane.XZ) as rib_path:
                ThreePointArc(
                    (10.0, 0.0),
                    (6.0, 7.5),
                    (5.0, 15.0),
                )
            with BuildSketch(Plane.YZ.offset(10.0 + rib_offset)):
                Circle(rib_profile_radius)
            sweep(path=rib_path.line)

        rib_clip = Box(24.0, 24.0, 15.0).translate((0.0, 0.0, 7.5))
        ribs = [
            single_rib.part.rotate(Axis.Z, i * (360.0 / 16)) & rib_clip
            for i in range(16)
        ]
        with BuildPart() as ribbed_builder:
            add(base_pedestal)
            add(ribs)

        ribbed = ribbed_builder.part
        super().__init__(ribbed.wrapped)


def identity_box_pair() -> CadPair:
    low = LowResCuboid()
    high = LowResCuboid()
    return CadPair("identity_box", low, high)


def flat_to_u_groove_pair() -> CadPair:
    return CadPair("flat_to_u_groove", LowResCuboid(), HighResCarvedCuboid())


def flat_to_v_groove_pair() -> CadPair:
    return CadPair("flat_to_v_groove", LowResCuboid(), HighResVCarvedCuboid())


def pedestal_ribs_pair() -> CadPair:
    return CadPair("pedestal_ribs", LowResPedestal(), HighResRibbedPedestal())


def pedestal_ribs_nofold_pair() -> CadPair:
    return CadPair("pedestal_ribs_nofold", LowResPedestal(), HighResRibbedPedestalNoFold())


def all_pairs() -> list[CadPair]:
    return [
        identity_box_pair(),
        flat_to_u_groove_pair(),
        flat_to_v_groove_pair(),
        pedestal_ribs_pair(),
        pedestal_ribs_nofold_pair(),
    ]


def export_pair(pair: CadPair, output_dir: Path) -> dict:
    case_dir = output_dir / pair.name
    case_dir.mkdir(parents=True, exist_ok=True)

    low_path = case_dir / "low.brep"
    high_path = case_dir / "high.brep"
    export_brep(pair.low, low_path)
    export_brep(pair.high, high_path)

    return {
        "name": pair.name,
        "low_brep": str(low_path),
        "high_brep": str(high_path),
        "low_face_count": len(pair.low.faces()),
        "high_face_count": len(pair.high.faces()),
    }
