#include "SurfaceAdaptor.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <GeomAbs_Shape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <mutex>

namespace cad_uv_map::geom {

void SurfaceAdaptor::Load(const TopoDS_Face& face) {
    BRepBuilderAPI_Copy copier(face, /*CopyGeom=*/Standard_True);
    private_face_ = TopoDS::Face(copier.Shape());
    adaptor_.Initialize(private_face_);

    // Pre-warm every BSpline Bezier knot-span cache by evaluating D0 on a
    // grid that covers each C1-continuous interval at least once.
    // After this, concurrent D0/D1 calls from multiple threads are pure reads:
    // they find every span's cache already populated and write nothing.
    // 4 cells per span mirrors OCCT's own BRepTopAdaptor_TopolTool strategy.
    const int nu = std::max(4, adaptor_.NbUIntervals(GeomAbs_C1) * 4);
    const int nv = std::max(4, adaptor_.NbVIntervals(GeomAbs_C1) * 4);
    const double u0 = adaptor_.FirstUParameter(), u1 = adaptor_.LastUParameter();
    const double v0 = adaptor_.FirstVParameter(), v1 = adaptor_.LastVParameter();
    gp_Pnt dummy;
    for (int i = 0; i <= nu; ++i)
        for (int j = 0; j <= nv; ++j)
            adaptor_.D0(u0 + i * (u1 - u0) / nu,
                        v0 + j * (v1 - v0) / nv, dummy);
}

gp_Pnt SurfaceAdaptor::Value(double u, double v) const {
    std::lock_guard<std::mutex> lock(*cache_mutex_);
    return adaptor_.Value(u, v);
}

void SurfaceAdaptor::D0(double u, double v, gp_Pnt& P) const {
    std::lock_guard<std::mutex> lock(*cache_mutex_);
    adaptor_.D0(u, v, P);
}

void SurfaceAdaptor::D1(double u, double v, gp_Pnt& P, gp_Vec& D1U, gp_Vec& D1V) const {
    std::lock_guard<std::mutex> lock(*cache_mutex_);
    adaptor_.D1(u, v, P, D1U, D1V);
}

TopAbs_Orientation SurfaceAdaptor::Orientation() const {
    return private_face_.Orientation();
}

double SurfaceAdaptor::FirstUParameter() const { return adaptor_.FirstUParameter(); }
double SurfaceAdaptor::LastUParameter()  const { return adaptor_.LastUParameter(); }
double SurfaceAdaptor::FirstVParameter() const { return adaptor_.FirstVParameter(); }
double SurfaceAdaptor::LastVParameter()  const { return adaptor_.LastVParameter(); }

int SurfaceAdaptor::NbUIntervals(GeomAbs_Shape continuity) const {
    return adaptor_.NbUIntervals(continuity);
}
int SurfaceAdaptor::NbVIntervals(GeomAbs_Shape continuity) const {
    return adaptor_.NbVIntervals(continuity);
}

} // namespace cad_uv_map::geom
