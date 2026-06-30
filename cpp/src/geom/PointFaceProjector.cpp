#include "PointFaceProjector.hpp"

#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <GeomAbs_Shape.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Curve.hxx>
#include <Precision.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

#include <algorithm>
#include <cmath>

namespace cad_uv_map::geom {

namespace {

int curve_sample_count(const Handle(Geom2d_Curve)& c) {
    Handle(Geom2d_BSplineCurve) bs = Handle(Geom2d_BSplineCurve)::DownCast(c);
    if (!bs.IsNull())
        return std::max(24, (bs->NbKnots() - 1) * 6);
    return 24;
}

} // namespace

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

void PointFaceProjector::Load(const TopoDS_Face& face, double tolerance) {
    tolerance_ = tolerance;

    BRepBuilderAPI_Copy copier(face, /*CopyGeom=*/Standard_True);
    private_face_ = TopoDS::Face(copier.Shape());

    adaptor_.Load(private_face_);

    u_min_ = adaptor_.FirstUParameter();
    u_max_ = adaptor_.LastUParameter();
    v_min_ = adaptor_.FirstVParameter();
    v_max_ = adaptor_.LastVParameter();

    build_mesh();
    build_boundary();

    // Build bbox from the grid points already computed in build_mesh().
    // Avoids BRepBndLib which may trigger OCCT triangulation internally.
    for (const gp_Pnt& p : points_)
        bbox_.Add(p);
}

// ---------------------------------------------------------------------------
// CanReach
// ---------------------------------------------------------------------------

bool PointFaceProjector::CanReach(const gp_Pnt& query, double max_dist) const {
    if (bbox_.IsVoid()) return true;
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox_.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    // Distance from query to nearest point on the box (zero if inside).
    const double dx = std::max({xmin - query.X(), 0.0, query.X() - xmax});
    const double dy = std::max({ymin - query.Y(), 0.0, query.Y() - ymax});
    const double dz = std::max({zmin - query.Z(), 0.0, query.Z() - zmax});
    return dx * dx + dy * dy + dz * dz <= max_dist * max_dist;
}

// ---------------------------------------------------------------------------
// Extension point 1: mesh resolution
// ---------------------------------------------------------------------------

std::pair<int, int> PointFaceProjector::mesh_resolution() const {
    return mesh_resolution_adaptive();
}

std::pair<int, int> PointFaceProjector::mesh_resolution_adaptive() const {
    // 4 cells per C1-continuous knot span so every span is sampled at least
    // once, giving Newton seeds dense enough to reach the global nearest point.
    // Minimum of 8 keeps cost low for simple (plane/cylinder) faces.
    const int nu = std::max(8, adaptor_.NbUIntervals(GeomAbs_C1) * 4);
    const int nv = std::max(8, adaptor_.NbVIntervals(GeomAbs_C1) * 4);
    return {nu, nv};
}

std::pair<int, int> PointFaceProjector::mesh_resolution_fixed(int nu, int nv) {
    return {nu, nv};
}

// ---------------------------------------------------------------------------
// build_mesh
// ---------------------------------------------------------------------------

void PointFaceProjector::build_mesh() {
    const auto [NU, NV] = mesh_resolution();
    npu_ = NU + 1;
    npv_ = NV + 1;

    grid_u_.resize(npu_);
    grid_v_.resize(npv_);
    for (int i = 0; i < npu_; ++i)
        grid_u_[i] = u_min_ + i * (u_max_ - u_min_) / NU;
    for (int j = 0; j < npv_; ++j)
        grid_v_[j] = v_min_ + j * (v_max_ - v_min_) / NV;

    points_.resize(static_cast<std::size_t>(npu_ * npv_));

    // Evaluating every grid point pre-warms all BSpline Bezier caches so
    // subsequent concurrent D1 calls in Perform() are pure reads.
    for (int j = 0; j < npv_; ++j)
        for (int i = 0; i < npu_; ++i)
            adaptor_.D0(grid_u_[i], grid_v_[j],
                        points_[static_cast<std::size_t>(j * npu_ + i)]);
}

// ---------------------------------------------------------------------------
// build_boundary  (identical logic to RayFaceIntersector::build_boundary)
// ---------------------------------------------------------------------------

void PointFaceProjector::build_boundary() {
    boundary_wires_.clear();

    TopExp_Explorer wire_exp(private_face_, TopAbs_WIRE);
    for (; wire_exp.More(); wire_exp.Next()) {
        std::vector<gp_Pnt2d> wire_poly;

        TopExp_Explorer edge_exp(wire_exp.Current(), TopAbs_EDGE);
        for (; edge_exp.More(); edge_exp.Next()) {
            const TopoDS_Edge& edge = TopoDS::Edge(edge_exp.Current());
            Standard_Real first = 0.0, last = 1.0;
            Handle(Geom2d_Curve) c2 =
                BRep_Tool::CurveOnSurface(edge, private_face_, first, last);
            if (c2.IsNull()) continue;

            const bool reversed = (edge.Orientation() == TopAbs_REVERSED);
            const int N = curve_sample_count(c2);
            for (int k = 0; k <= N; ++k) {
                const double t = reversed
                    ? last  - k * (last - first) / N
                    : first + k * (last - first) / N;
                gp_Pnt2d pt;
                c2->D0(t, pt);
                wire_poly.push_back(pt);
            }
        }

        if (!wire_poly.empty())
            boundary_wires_.push_back(std::move(wire_poly));
    }
}

// ---------------------------------------------------------------------------
// Extension point 2: analytic dispatch
// ---------------------------------------------------------------------------

std::optional<PointResult> PointFaceProjector::try_analytic(
    const gp_Pnt& /*query*/) const {
    // TODO: dispatch closed-form nearest-point for analytic surface types.
    //
    // Check adaptor_.GetType() and handle:
    //
    //   GeomAbs_Plane    — nearest point = query + (plane_normal · (plane_origin - query))
    //                      * plane_normal  (one dot product, exact)
    //   GeomAbs_Cylinder — project query to axis, clamp, then extend to cylinder radius
    //   GeomAbs_Sphere   — normalize the vector from sphere centre to query,
    //                      scale to radius
    //   GeomAbs_Cone     — closed form via parametric distance to axis line
    //
    // For each case: compute the nearest surface 3D point, invert to UV, check
    // inside_face(), and return a PointResult directly.
    //
    // Planes and cylinders dominate typical mechanical CAD — implementing them
    // eliminates the grid scan entirely for the majority of faces.
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Extension point 3: candidate search
// ---------------------------------------------------------------------------

std::vector<std::size_t> PointFaceProjector::find_candidates(
    const gp_Pnt& query, int k) const {
    // TODO: replace O(n) scan with a 3D BVH for O(log n) nearest lookup.
    //
    // Build the BVH over grid-cell AABBs in Load() after build_mesh().
    // In find_candidates(), traverse the BVH: at each node, compute the
    // minimum possible 3D distance from query to the node AABB and prune if
    // it exceeds the current k-th best distance. This produces a priority-queue
    // branch-and-bound that visits O(log n) nodes for well-clustered points.
    //
    // For the current fixed 20×20 grid (441 points) the O(n) scan is fast
    // enough. With adaptive mesh sizes (potentially thousands of points on
    // complex surfaces), O(log n) becomes important.
    //
    // Current implementation: return the k nearest grid points by 3D distance.
    const std::size_t n = points_.size();
    const int take = std::min(k, static_cast<int>(n));

    std::vector<std::pair<double, std::size_t>> dists;
    dists.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        dists.push_back({points_[i].SquareDistance(query), i});

    std::partial_sort(dists.begin(), dists.begin() + take, dists.end());

    std::vector<std::size_t> result;
    result.reserve(take);
    for (int i = 0; i < take; ++i)
        result.push_back(dists[i].second);
    return result;
}

// ---------------------------------------------------------------------------
// Gauss-Newton refinement for nearest point
// ---------------------------------------------------------------------------
//
// Minimises f(u,v) = |S(u,v) − P|².
//
// First-order conditions: ∇f = 2 [(S−P)·Su, (S−P)·Sv] = 0.
// Gauss-Newton step (approximating the Hessian by J^T J, i.e. dropping the
// second-derivative term (S−P)·S_uu which is small near the solution):
//
//   J^T J · [Δu, Δv]^T = −J^T r
//
// where J = [Su, Sv] (3×2 Jacobian), r = S−P, so:
//
//   [[Su·Su,  Su·Sv],   [Δu]   =  −[Su·(S−P)]
//    [Su·Sv,  Sv·Sv]] · [Δv]      −[Sv·(S−P)]
//
// The 2×2 Gram matrix is symmetric positive-definite at non-degenerate points.

bool PointFaceProjector::refine_nearest(
    const gp_Pnt& query,
    double u_start, double v_start,
    PointHit* hit) const {
    double u = std::clamp(u_start, u_min_, u_max_);
    double v = std::clamp(v_start, v_min_, v_max_);

    for (int iter = 0; iter < 30; ++iter) {
        gp_Pnt  P; gp_Vec Su, Sv;
        adaptor_.D1(u, v, P, Su, Sv);

        const gp_Vec diff(query, P);   // P − query
        const double f1 = diff.Dot(Su);
        const double f2 = diff.Dot(Sv);

        const double a   = Su.Dot(Su); // |Su|²
        const double b   = Su.Dot(Sv);
        const double c   = Sv.Dot(Sv); // |Sv|²
        const double det = a * c - b * b;

        if (std::abs(det) < Precision::Confusion()) break;

        const double du = -(c * f1 - b * f2) / det;
        const double dv = -(a * f2 - b * f1) / det;

        u = std::clamp(u + du, u_min_, u_max_);
        v = std::clamp(v + dv, v_min_, v_max_);

        if (std::hypot(du, dv) < 1e-9) {
            gp_Pnt P_final;
            adaptor_.D0(u, v, P_final);
            hit->distance = P_final.Distance(query);
            hit->u = u; hit->v = v; hit->pt = P_final;
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Extension point 4: boundary classification (shared with RayFaceIntersector)
// ---------------------------------------------------------------------------

bool PointFaceProjector::inside_face(double u, double v) const {
    // Even-odd crossing-number test. See RayFaceIntersector::inside_face() for
    // the same implementation and the TODO for BRepTopAdaptor_TopolTool upgrade.
    int cn = 0;
    for (const auto& wire : boundary_wires_) {
        const int n = static_cast<int>(wire.size());
        for (int i = 0; i < n - 1; ++i) {
            const double x1 = wire[i].X(),     y1 = wire[i].Y();
            const double x2 = wire[i + 1].X(), y2 = wire[i + 1].Y();
            if ((y1 <= v && y2 > v) || (y2 <= v && y1 > v)) {
                const double x_cross = x1 + (v - y1) * (x2 - x1) / (y2 - y1);
                if (u < x_cross) ++cn;
            }
        }
    }
    return (cn % 2) == 1;
}

// ---------------------------------------------------------------------------
// Perform
// ---------------------------------------------------------------------------

PointResult PointFaceProjector::Perform(const gp_Pnt& query) const {
    // Extension point 2: analytic surfaces bypass the grid entirely.
    if (auto r = try_analytic(query)) return *r;

    PointResult result;
    result.done = true;

    // Extension point 3: K nearest grid points as Newton seeds.
    constexpr int K = 8;
    const std::vector<std::size_t> candidates = find_candidates(query, K);

    for (const std::size_t idx : candidates) {
        const int i = static_cast<int>(idx) % npu_;
        const int j = static_cast<int>(idx) / npu_;
        const double u_seed = grid_u_[static_cast<std::size_t>(i)];
        const double v_seed = grid_v_[static_cast<std::size_t>(j)];

        PointHit hit;
        if (!refine_nearest(query, u_seed, v_seed, &hit)) continue;
        // Extension point 4: boundary classification.
        if (!inside_face(hit.u, hit.v)) continue;

        result.hits.push_back(hit);
    }

    // Sort by distance and remove duplicate convergence points.
    std::sort(result.hits.begin(), result.hits.end(),
              [](const PointHit& a, const PointHit& b) {
                  return a.distance < b.distance;
              });

    const double dedup_tol = tolerance_ * 10.0;
    auto it = std::unique(result.hits.begin(), result.hits.end(),
        [dedup_tol](const PointHit& a, const PointHit& b) {
            return std::abs(a.u - b.u) < dedup_tol &&
                   std::abs(a.v - b.v) < dedup_tol;
        });
    result.hits.erase(it, result.hits.end());

    return result;
}

} // namespace cad_uv_map::geom
