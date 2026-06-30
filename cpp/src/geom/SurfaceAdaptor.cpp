#include "SurfaceAdaptor.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <TopoDS.hxx>

namespace cad_uv_map::geom {

void SurfaceAdaptor::Load(const TopoDS_Face& face) {
    BRepBuilderAPI_Copy copier(face, /*CopyGeom=*/Standard_True);
    private_face_ = TopoDS::Face(copier.Shape());
    adaptor_.Initialize(private_face_);
}

gp_Pnt SurfaceAdaptor::Value(double u, double v) const {
    return adaptor_.Value(u, v);
}

void SurfaceAdaptor::D0(double u, double v, gp_Pnt& P) const {
    adaptor_.D0(u, v, P);
}

void SurfaceAdaptor::D1(double u, double v, gp_Pnt& P, gp_Vec& D1U, gp_Vec& D1V) const {
    adaptor_.D1(u, v, P, D1U, D1V);
}

TopAbs_Orientation SurfaceAdaptor::Orientation() const {
    return private_face_.Orientation();
}

double SurfaceAdaptor::FirstUParameter() const { return adaptor_.FirstUParameter(); }
double SurfaceAdaptor::LastUParameter()  const { return adaptor_.LastUParameter(); }
double SurfaceAdaptor::FirstVParameter() const { return adaptor_.FirstVParameter(); }
double SurfaceAdaptor::LastVParameter()  const { return adaptor_.LastVParameter(); }

} // namespace cad_uv_map::geom
