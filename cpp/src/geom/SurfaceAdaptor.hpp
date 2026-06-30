#pragma once

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_Shape.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <memory>
#include <mutex>

namespace cad_uv_map::geom {

// Thread-safe surface evaluator for a TopoDS_Face.
//
// Load() deep-copies the face geometry and pre-warms the BSpline span cache.
// After Load(), Value/D0/D1 are safe to call from any number of concurrent
// threads on the SAME instance: a per-instance mutex serialises access to
// GeomAdaptor_Surface::mySurfaceCache (a single-entry mutable field that
// stores Bezier data for the most-recently-evaluated knot span).
//
// For performance: give each parallel worker its OWN SurfaceAdaptor so
// threads access independent caches with no mutex contention at all.
// The shared-instance path is correct but serialises cache writes.
class SurfaceAdaptor {
public:
    // Deep-copy face geometry into private storage.
    // Call in serial before launching parallel workers.
    void Load(const TopoDS_Face& face);

    gp_Pnt Value(double u, double v)                                        const;
    void   D0   (double u, double v, gp_Pnt& P)                            const;
    void   D1   (double u, double v, gp_Pnt& P, gp_Vec& D1U, gp_Vec& D1V) const;

    TopAbs_Orientation Orientation() const;

    double FirstUParameter() const;
    double LastUParameter()  const;
    double FirstVParameter() const;
    double LastVParameter()  const;

    // Number of parameter intervals on which the surface is C<n>-continuous.
    // For a BSpline surface this equals the number of knot spans.
    // Needed by mesh_resolution() to guarantee every span is pre-warmed.
    int NbUIntervals(GeomAbs_Shape continuity) const;
    int NbVIntervals(GeomAbs_Shape continuity) const;

private:
    TopoDS_Face         private_face_;
    BRepAdaptor_Surface adaptor_;

    // Serialises concurrent Value/D0/D1 calls to protect the single-entry
    // BSplSLib_Cache stored inside GeomAdaptor_Surface (mySurfaceCache).
    // unique_ptr keeps SurfaceAdaptor moveable for std::vector compatibility.
    mutable std::unique_ptr<std::mutex> cache_mutex_ =
        std::make_unique<std::mutex>();
};

} // namespace cad_uv_map::geom
