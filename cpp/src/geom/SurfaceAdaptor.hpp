#pragma once

#include <BRepAdaptor_Surface.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace cad_uv_map::geom {

// Thread-safe surface evaluator for a TopoDS_Face.
//
// Load() deep-copies the face's underlying geometry so each SurfaceAdaptor
// holds private OCCT objects with no shared mutable state. Multiple instances
// loaded from the same TopoDS_Face handle can therefore evaluate concurrently
// without racing on OCCT's lazy Bezier-segment cache.
//
// Method names match BRepAdaptor_Surface for familiarity. Each instance must
// be used from one thread at a time; thread-safety comes from having one
// instance per thread, not from locking inside these methods.
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

private:
    TopoDS_Face         private_face_;
    BRepAdaptor_Surface adaptor_;
};

} // namespace cad_uv_map::geom
