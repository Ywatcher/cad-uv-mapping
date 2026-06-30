#pragma once

#include <Standard_Integer.hxx>
#include <Standard_Real.hxx>
#include <Standard_Boolean.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

#include <vector>

namespace cad_uv_map::geom {

// Thread-safe replacement for BRepExtrema_DistShapeShape + ShapeAnalysis_Surface.
//
// Load() deep-copies the face's underlying geometry so each instance holds
// private OCCT objects with no shared mutable state. Concurrent calls from
// different objects on the same TopoDS_Face handle are safe.
//
// Perform() takes a gp_Pnt directly (builds the internal vertex internally)
// and resolves UV coordinates in addition to the 3-D hit so the caller does
// not need a separate ShapeAnalysis_Surface step.
//
// IsDone(), NbSolution(), Value(), and PointOnShape2() match the methods on
// BRepExtrema_DistShapeShape. ValueOfUV() is an extension that returns the
// face UV coordinate for the i-th solution.
class DistShapeShape {
public:
    // Deep-copy face geometry into private storage.
    // Safe to call concurrently on different objects from the same face handle.
    void Load(const TopoDS_Face& face, double tolerance);

    // Compute nearest point on the loaded face to the given 3-D point.
    void Perform(const gp_Pnt& point);

    Standard_Boolean IsDone()                             const;
    Standard_Integer NbSolution()                         const;
    Standard_Real    Value()                              const;
    gp_Pnt           PointOnShape2(Standard_Integer index) const;

    // UV coordinate on the face for the i-th nearest-point solution.
    gp_Pnt2d         ValueOfUV(Standard_Integer index)    const;

private:
    struct Hit {
        gp_Pnt   point3d;
        gp_Pnt2d uv;
    };

    TopoDS_Face private_face_;
    double tolerance_{1e-7};
    bool done_{false};
    double distance_{0.0};
    std::vector<Hit> hits_;
};

} // namespace cad_uv_map::geom
