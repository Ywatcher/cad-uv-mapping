#include "RayFaceIntersector.hpp"

#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Curve.hxx>
#include <Precision.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>

namespace cad_uv_map::geom {

namespace {

void build_frame(const gp_Dir& dir, gp_Dir& e1, gp_Dir& e2) {
    const gp_Vec d(dir);
    gp_Vec ref;
    if (std::abs(d.X()) <= std::abs(d.Y()) && std::abs(d.X()) <= std::abs(d.Z()))
        ref = gp_Vec(1.0, 0.0, 0.0);
    else if (std::abs(d.Y()) <= std::abs(d.Z()))
        ref = gp_Vec(0.0, 1.0, 0.0);
    else
        ref = gp_Vec(0.0, 0.0, 1.0);
    e1 = gp_Dir(d.Crossed(ref));
    e2 = gp_Dir(gp_Vec(d).Crossed(gp_Vec(e1)));
}

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

void RayFaceIntersector::Load(const TopoDS_Face& face, double tolerance) {
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
}

// ---------------------------------------------------------------------------
// Extension point 1: mesh resolution
// ---------------------------------------------------------------------------

std::pair<int, int> RayFaceIntersector::mesh_resolution() const {
    // TODO: make adaptive. Replace this body with:
    //
    //   const int nu = std::max(8, adaptor_.NbUIntervals(GeomAbs_C1) * 4);
    //   const int nv = std::max(8, adaptor_.NbVIntervals(GeomAbs_C1) * 4);
    //   return {nu, nv};
    //
    // NbUIntervals(GeomAbs_C1) returns the number of parameter intervals on
    // which the surface is C1-continuous (i.e. the number of knot spans for a
    // BSpline). Using 4 cells per span guarantees that each knot span contains
    // at least 4 triangles, preventing missed hits on tightly-splined surfaces.
    // This mirrors OCCT's own sampling strategy in BRepTopAdaptor_TopolTool.
    //
    // No other code needs to change: build_mesh() already calls this method.
    return {20, 20};
}

// ---------------------------------------------------------------------------
// build_mesh
// ---------------------------------------------------------------------------

void RayFaceIntersector::build_mesh() {
    const auto [NU, NV] = mesh_resolution();
    const int NPU = NU + 1;
    const int NPV = NV + 1;

    points_.resize(NPU * NPV);

    std::vector<double> us(NPU), vs(NPV);
    for (int i = 0; i < NPU; ++i)
        us[i] = u_min_ + i * (u_max_ - u_min_) / NU;
    for (int j = 0; j < NPV; ++j)
        vs[j] = v_min_ + j * (v_max_ - v_min_) / NV;

    // Evaluate surface. Side effect: pre-warms all BSpline Bezier caches so
    // subsequent concurrent D0/D1 calls are pure reads.
    for (int j = 0; j < NPV; ++j)
        for (int i = 0; i < NPU; ++i)
            adaptor_.D0(us[i], vs[j], points_[j * NPU + i]);

    triangles_.reserve(NU * NV * 2);
    boxes_.reserve(NU * NV * 2 * 6);

    auto add_tri = [&](int ia, int ib, int ic,
                       double ua, double va,
                       double ub, double vb,
                       double uc, double vc) {
        triangles_.push_back({ia, ib, ic, ua, va, ub, vb, uc, vc});
        const gp_Pnt& pa = points_[ia];
        const gp_Pnt& pb = points_[ib];
        const gp_Pnt& pc = points_[ic];
        boxes_.push_back(std::min({pa.X(), pb.X(), pc.X()}) - tolerance_);
        boxes_.push_back(std::min({pa.Y(), pb.Y(), pc.Y()}) - tolerance_);
        boxes_.push_back(std::min({pa.Z(), pb.Z(), pc.Z()}) - tolerance_);
        boxes_.push_back(std::max({pa.X(), pb.X(), pc.X()}) + tolerance_);
        boxes_.push_back(std::max({pa.Y(), pb.Y(), pc.Y()}) + tolerance_);
        boxes_.push_back(std::max({pa.Z(), pb.Z(), pc.Z()}) + tolerance_);
    };

    for (int j = 0; j < NV; ++j) {
        for (int i = 0; i < NU; ++i) {
            const int i00 = j * NPU + i;
            const int i10 = j * NPU + (i + 1);
            const int i01 = (j + 1) * NPU + i;
            const int i11 = (j + 1) * NPU + (i + 1);
            add_tri(i00, i10, i11, us[i], vs[j], us[i+1], vs[j],   us[i+1], vs[j+1]);
            add_tri(i00, i11, i01, us[i], vs[j], us[i+1], vs[j+1], us[i],   vs[j+1]);
        }
    }
}

// ---------------------------------------------------------------------------
// build_boundary
// ---------------------------------------------------------------------------

void RayFaceIntersector::build_boundary() {
    // For each wire, sample every PCurve edge in traversal order.
    // Side effect: evaluating PCurves here pre-warms their lazy Bezier caches
    // so that concurrent calls to inside_face() are pure reads.
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

std::optional<RayResult> RayFaceIntersector::try_analytic(
    const gp_Lin& /*line*/, double /*pmin*/, double /*pmax*/) const {
    // TODO: dispatch closed-form intersection for analytic surface types.
    //
    // Check adaptor_.GetType() (returns GeomAbs_SurfaceType) and handle:
    //
    //   GeomAbs_Plane    — IntAna_IntConicQuad (line vs plane, 1 equation)
    //   GeomAbs_Cylinder — IntAna_IntConicQuad (quadratic)
    //   GeomAbs_Sphere   — IntAna_IntConicQuad (quadratic)
    //   GeomAbs_Cone     — IntAna_IntConicQuad (quadratic)
    //   GeomAbs_Torus    — IntAna_IntLinTorus   (quartic)
    //
    // For each case: compute the analytic hit parameter(s), evaluate the
    // surface to get (u,v,pt), filter by pmin/pmax and inside_face(), and
    // return a RayResult directly without entering the mesh path.
    //
    // Most mechanical CAD faces are planes or cylinders. Implementing those
    // two cases alone covers the majority of faces and eliminates mesh lookup
    // for them entirely.
    //
    // Return nullopt to fall through to the mesh-based path (current behaviour).
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Extension point 3: spatial index
// ---------------------------------------------------------------------------

std::vector<std::size_t> RayFaceIntersector::find_candidates(
    const gp_Lin& line) const {
    // TODO: replace linear scan with a BVH for O(log n) lookup.
    //
    // Build the BVH in Load() after build_mesh():
    //   - Morton-sort triangle indices by their AABB centroid
    //   - Build a binary BVH by splitting at the midpoint of the longest axis
    //   - Store as a flat array (2*n - 1 nodes for n leaves)
    //
    // In find_candidates(), traverse the BVH: at each node, slab-test the node
    // AABB against the ray and prune the subtree if the ray misses. This reduces
    // candidate count from O(n) to O(log n) for a balanced BVH.
    //
    // The current flat linear scan is correct and fast enough for a 20×20 mesh
    // (800 triangles). With adaptive sizing a face with 50×50 knot spans could
    // produce ~20000 triangles, at which point O(log n) matters.
    //
    // Inline slab test (current implementation):
    const gp_Dir& dir  = line.Direction();
    const gp_Pnt& orig = line.Location();
    const double dx = dir.X(), dy = dir.Y(), dz = dir.Z();
    const double ox = orig.X(), oy = orig.Y(), oz = orig.Z();

    std::vector<std::size_t> candidates;
    const std::size_t n = triangles_.size();
    candidates.reserve(n / 4);

    for (std::size_t ti = 0; ti < n; ++ti) {
        const double* b = boxes_.data() + ti * 6;

        double tmin = -std::numeric_limits<double>::max();
        double tmax =  std::numeric_limits<double>::max();

        if (std::abs(dx) > 1e-12) {
            double t1 = (b[0] - ox) / dx, t2 = (b[3] - ox) / dx;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
        } else if (ox < b[0] || ox > b[3]) continue;

        if (std::abs(dy) > 1e-12) {
            double t1 = (b[1] - oy) / dy, t2 = (b[4] - oy) / dy;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
        } else if (oy < b[1] || oy > b[4]) continue;

        if (std::abs(dz) > 1e-12) {
            double t1 = (b[2] - oz) / dz, t2 = (b[5] - oz) / dz;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
        } else if (oz < b[2] || oz > b[5]) continue;

        if (tmin <= tmax) candidates.push_back(ti);
    }

    return candidates;
}

// ---------------------------------------------------------------------------
// moller_trumbore
// ---------------------------------------------------------------------------

bool RayFaceIntersector::moller_trumbore(
    const gp_Dir& dir, const gp_Pnt& orig,
    const MeshTri& tri,
    double* w_out, double* s_out, double* t_out) const {
    constexpr double EPS = 1e-12;

    const gp_Vec e1 = gp_Vec(points_[tri.i0], points_[tri.i1]);
    const gp_Vec e2 = gp_Vec(points_[tri.i0], points_[tri.i2]);
    const gp_Vec h  = gp_Vec(dir).Crossed(e2);
    const double a  = e1.Dot(h);
    if (std::abs(a) < EPS) return false;

    const double f   = 1.0 / a;
    const gp_Vec s   = gp_Vec(points_[tri.i0], orig);
    const double s_c = f * s.Dot(h);
    if (s_c < -EPS || s_c > 1.0 + EPS) return false;

    const gp_Vec q   = s.Crossed(e1);
    const double t_c = f * gp_Vec(dir).Dot(q);
    if (t_c < -EPS || s_c + t_c > 1.0 + EPS) return false;

    *w_out = f * e2.Dot(q);
    *s_out = s_c;
    *t_out = t_c;
    return true;
}

// ---------------------------------------------------------------------------
// refine
// ---------------------------------------------------------------------------

bool RayFaceIntersector::refine(
    const gp_Lin& line,
    double u_approx, double v_approx,
    RayHit* hit) const {
    const gp_Vec dir(line.Direction());
    const gp_Pnt& origin = line.Location();

    gp_Dir e1, e2;
    build_frame(line.Direction(), e1, e2);
    const gp_Vec ve1(e1), ve2(e2);

    double u = std::clamp(u_approx, u_min_, u_max_);
    double v = std::clamp(v_approx, v_min_, v_max_);

    for (int iter = 0; iter < 30; ++iter) {
        gp_Pnt P; gp_Vec Pu, Pv;
        adaptor_.D1(u, v, P, Pu, Pv);

        const gp_Vec diff(origin, P);
        const double t  = diff.Dot(dir);
        const gp_Vec r  = diff - t * dir;
        const double g1 = r.Dot(ve1);
        const double g2 = r.Dot(ve2);

        if (std::sqrt(g1 * g1 + g2 * g2) < tolerance_) {
            hit->w = t; hit->u = u; hit->v = v; hit->pt = P;
            return true;
        }

        const double j11 = Pu.Dot(ve1), j12 = Pv.Dot(ve1);
        const double j21 = Pu.Dot(ve2), j22 = Pv.Dot(ve2);
        const double det = j11 * j22 - j12 * j21;
        if (std::abs(det) < Precision::Confusion()) break;

        u = std::clamp(u + (-j22 * g1 + j12 * g2) / det, u_min_, u_max_);
        v = std::clamp(v + ( j21 * g1 - j11 * g2) / det, v_min_, v_max_);
    }

    return false;
}

// ---------------------------------------------------------------------------
// Extension point 4: boundary classification
// ---------------------------------------------------------------------------

bool RayFaceIntersector::inside_face(double u, double v) const {
    // Even-odd crossing-number test on the pre-built 2D boundary polygon.
    // Handles holes correctly by parity without distinguishing outer/inner wires.
    //
    // TODO: for faces with degenerate or self-intersecting boundary curves this
    // test can misclassify. Replace with BRepTopAdaptor_TopolTool::Classify()
    // on private_face_ (whose PCurves are fully pre-warmed in build_boundary(),
    // so concurrent calls would be pure reads). That requires confirming that
    // BRepTopAdaptor_TopolTool allocates no Standard_Transient objects during
    // Classify() — if it does, instantiate and cache one TopolTool per
    // RayFaceIntersector during Load() so Classify() only reads.
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

RayResult RayFaceIntersector::Perform(
    const gp_Lin& line, double pmin, double pmax) const {
    // Extension point 2: analytic surfaces bypass the mesh entirely.
    if (auto r = try_analytic(line, pmin, pmax)) return *r;

    RayResult result;
    result.done = true;

    // Extension point 3: candidate lookup (currently linear, future BVH).
    const std::vector<std::size_t> candidates = find_candidates(line);

    for (const std::size_t ti : candidates) {
        double w, s, t;
        if (!moller_trumbore(line.Direction(), line.Location(), triangles_[ti], &w, &s, &t))
            continue;
        if (w < pmin - tolerance_ || w > pmax + tolerance_) continue;

        const MeshTri& tri  = triangles_[ti];
        const double bary0  = 1.0 - s - t;
        const double u_approx = bary0 * tri.u0 + s * tri.u1 + t * tri.u2;
        const double v_approx = bary0 * tri.v0 + s * tri.v1 + t * tri.v2;

        RayHit hit;
        if (!refine(line, u_approx, v_approx, &hit)) continue;
        if (hit.w < pmin - tolerance_ || hit.w > pmax + tolerance_) continue;

        // Extension point 4: boundary classification.
        if (!inside_face(hit.u, hit.v)) continue;

        result.hits.push_back(hit);
    }

    std::sort(result.hits.begin(), result.hits.end(),
              [](const RayHit& a, const RayHit& b) { return a.w < b.w; });

    const double dedup_tol = tolerance_ * 10.0;
    auto it = std::unique(result.hits.begin(), result.hits.end(),
        [dedup_tol](const RayHit& a, const RayHit& b) {
            return std::abs(a.w - b.w) < dedup_tol;
        });
    result.hits.erase(it, result.hits.end());

    return result;
}

} // namespace cad_uv_map::geom
