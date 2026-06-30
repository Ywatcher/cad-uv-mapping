#include "ShapeIntersector.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <TopoDS.hxx>

namespace cad_uv_map::geom {

void ShapeIntersector::Load(const TopoDS_Face& face, double tolerance) {
    tolerance_ = tolerance;
    BRepBuilderAPI_Copy copier(face, /*CopyGeom=*/Standard_True);
    private_face_ = TopoDS::Face(copier.Shape());
}

void ShapeIntersector::Perform(const gp_Lin& line, double param_min, double param_max) {
    intersector_.Load(private_face_, tolerance_);
    intersector_.Perform(line, param_min, param_max);
}

Standard_Boolean ShapeIntersector::IsDone() const {
    return intersector_.IsDone();
}

Standard_Integer ShapeIntersector::NbPnt() const {
    return intersector_.NbPnt();
}

Standard_Real ShapeIntersector::WParameter(Standard_Integer index) const {
    return intersector_.WParameter(index);
}

Standard_Real ShapeIntersector::UParameter(Standard_Integer index) const {
    return intersector_.UParameter(index);
}

Standard_Real ShapeIntersector::VParameter(Standard_Integer index) const {
    return intersector_.VParameter(index);
}

gp_Pnt ShapeIntersector::Pnt(Standard_Integer index) const {
    return intersector_.Pnt(index);
}

} // namespace cad_uv_map::geom
