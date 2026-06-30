#pragma once

#include <IntCurvesFace_ShapeIntersector.hxx>
#include <Standard_Integer.hxx>
#include <Standard_Real.hxx>
#include <Standard_Boolean.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>

namespace cad_uv_map::geom {

// Thread-safe replacement for IntCurvesFace_ShapeIntersector.
//
// Load() deep-copies the face's underlying Geom_Surface and Geom2d_Curve
// objects so each ShapeIntersector holds entirely private OCCT objects.
// Concurrent calls to Load() on different ShapeIntersector instances from
// the same TopoDS_Face handle are therefore safe: no shared lazy cache is
// written in parallel.
//
// Method signatures and return types match IntCurvesFace_ShapeIntersector.
// Perform() is not const — a single object must be used from one thread.
class ShapeIntersector {
public:
    // Deep-copy face geometry into private storage.
    // Safe to call concurrently on different objects from the same face handle.
    void Load(const TopoDS_Face& face, double tolerance);

    void Perform(const gp_Lin& line, double param_min, double param_max);

    Standard_Boolean IsDone()                         const;
    Standard_Integer NbPnt()                          const;
    Standard_Real    WParameter(Standard_Integer index) const;
    Standard_Real    UParameter(Standard_Integer index) const;
    Standard_Real    VParameter(Standard_Integer index) const;
    gp_Pnt           Pnt(Standard_Integer index)      const;

private:
    TopoDS_Face private_face_;
    double tolerance_{1e-7};
    IntCurvesFace_ShapeIntersector intersector_;
};

} // namespace cad_uv_map::geom
