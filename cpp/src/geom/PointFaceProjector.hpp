#pragma once

#include "SurfaceAdaptor.hpp"

#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

#include <optional>
#include <vector>

namespace cad_uv_map::geom {

struct PointHit {
    double  distance; // 3D distance from query point to surface
    double  u, v;     // surface UV parameters at the nearest point
    gp_Pnt  pt;       // 3D nearest point on the surface
};

struct PointResult {
    bool done = false;
    std::vector<PointHit> hits; // sorted by ascending distance
};

// Thread-safe nearest-point projector for a TopoDS_Face.
//
// Load() deep-copies the face geometry, samples the surface on a UV grid,
// and pre-warms all lazy caches. After Load() the object is read-only.
//
// Perform() projects a query point onto the face using four phases, each in
// its own method so future improvements replace one method at a time:
//
//   try_analytic()      — [stub] closed-form dispatch for planes, cylinders…
//   find_candidates()   — K nearest grid points as Newton seeds
//   refine_nearest()    — Gauss-Newton minimisation of |S(u,v) − P|²
//   inside_face()       — even-odd crossing-number boundary classification
//
// Multiple threads may call Perform() concurrently on the same loaded instance.
//
// Improvement roadmap (each item is a single-method replacement):
//
//   1. Adaptive mesh  (mesh_resolution)
//      Same as RayFaceIntersector: query NbUIntervals(GeomAbs_C1) × 4.
//
//   2. Analytic dispatch  (try_analytic)
//      GeomAbs_Plane   — project via normal, one dot product.
//      GeomAbs_Cylinder/Sphere/Cone — closed-form extremal distance.
//      These eliminate the grid search entirely for the common face types.
//
//   3. Candidate search  (find_candidates)
//      Currently a full O(n) scan returning K nearest grid points.
//      Replace with a 3D BVH over grid cells for O(log n) nearest lookup.
//
//   4. Boundary classifier  (inside_face)
//      Same as RayFaceIntersector: BRepTopAdaptor_TopolTool::Classify()
//      once PCurve thread-safety is confirmed.
class PointFaceProjector {
public:
    // Deep-copies face geometry and builds all internal state.
    // Not concurrency-safe with itself — call once per instance, in serial.
    void Load(const TopoDS_Face& face, double tolerance);

    // Returns the nearest surface point(s) as a value without mutating the object.
    // Thread-safe: const, touches no mutable state.
    PointResult Perform(const gp_Pnt& query) const;

private:
    // --- Extension point 1: mesh resolution ---------------------------------
    std::pair<int, int> mesh_resolution() const;

    void build_mesh();
    void build_boundary();

    // --- Extension point 2: analytic dispatch --------------------------------
    std::optional<PointResult> try_analytic(const gp_Pnt& query) const;

    // --- Extension point 3: candidate search ---------------------------------
    // Returns indices into points_ of the K grid points nearest to query.
    std::vector<std::size_t> find_candidates(const gp_Pnt& query, int k) const;

    // --- Phase: Gauss-Newton refinement --------------------------------------
    // Minimises |S(u,v) - query|². Returns false if it fails to converge.
    bool refine_nearest(const gp_Pnt& query,
                        double u_start, double v_start,
                        PointHit* hit) const;

    // --- Extension point 4: boundary classifier ------------------------------
    bool inside_face(double u, double v) const;

    SurfaceAdaptor adaptor_;
    TopoDS_Face    private_face_;
    double         tolerance_{1e-7};

    double u_min_{}, u_max_{}, v_min_{}, v_max_{};

    std::vector<gp_Pnt> points_; // 3D grid vertices (NPU × NPV, row-major)
    std::vector<double> grid_u_; // U parameter at column i  (length NPU)
    std::vector<double> grid_v_; // V parameter at row j     (length NPV)
    int npu_{}, npv_{};

    std::vector<std::vector<gp_Pnt2d>> boundary_wires_;
};

} // namespace cad_uv_map::geom
