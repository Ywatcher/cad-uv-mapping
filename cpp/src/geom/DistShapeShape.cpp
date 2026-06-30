#include "DistShapeShape.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <TopoDS.hxx>

namespace cad_uv_map::geom {

void DistShapeShape::Load(const TopoDS_Face& face, double tolerance) {
    tolerance_ = tolerance;
    BRepBuilderAPI_Copy copier(face, /*CopyGeom=*/Standard_True);
    private_face_ = TopoDS::Face(copier.Shape());
    done_ = false;
    hits_.clear();
    distance_ = 0.0;
}

void DistShapeShape::Perform(const gp_Pnt& point) {
    done_ = false;
    hits_.clear();
    distance_ = 0.0;

    BRepBuilderAPI_MakeVertex vertex_builder(point);
    BRepExtrema_DistShapeShape dist(vertex_builder.Vertex(), private_face_, tolerance_);
    dist.Perform();

    if (!dist.IsDone() || dist.NbSolution() == 0) {
        return;
    }

    done_ = true;
    distance_ = dist.Value();

    ShapeAnalysis_Surface surface_analysis(BRep_Tool::Surface(private_face_));
    hits_.reserve(static_cast<std::size_t>(dist.NbSolution()));
    for (Standard_Integer i = 1; i <= dist.NbSolution(); ++i) {
        const gp_Pnt hit3d = dist.PointOnShape2(i);
        const gp_Pnt2d uv = surface_analysis.ValueOfUV(hit3d, tolerance_);
        hits_.push_back({hit3d, uv});
    }
}

Standard_Boolean DistShapeShape::IsDone() const {
    return done_ ? Standard_True : Standard_False;
}

Standard_Integer DistShapeShape::NbSolution() const {
    return static_cast<Standard_Integer>(hits_.size());
}

Standard_Real DistShapeShape::Value() const {
    return distance_;
}

gp_Pnt DistShapeShape::PointOnShape2(Standard_Integer index) const {
    return hits_[static_cast<std::size_t>(index - 1)].point3d;
}

gp_Pnt2d DistShapeShape::ValueOfUV(Standard_Integer index) const {
    return hits_[static_cast<std::size_t>(index - 1)].uv;
}

} // namespace cad_uv_map::geom
