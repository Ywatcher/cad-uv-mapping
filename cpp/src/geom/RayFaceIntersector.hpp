#pragma once

#include "SurfaceAdaptor.hpp"

#include <TopoDS_Face.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

#include <optional>
#include <vector>

namespace cad_uv_map::geom {

struct RayHit {
    double w;     // signed ray parameter (distance along ray direction)
    double u, v;  // surface UV at the hit
    gp_Pnt pt;    // 3D hit point
};

struct RayResult {
    bool done = false;
    std::vector<RayHit> hits;  // sorted by ascending w
};

// Thread-safe ray-face intersector.
//
// Load() deep-copies the face geometry, samples the surface on a UV grid to
// build a triangle mesh and bounding boxes, and pre-warms every lazy cache in
// the surface and boundary PCurves. After Load() the object is read-only.
//
// Perform() runs four phases in sequence, each isolated in its own method so
// future improvements replace one method without touching the rest:
//
//   try_analytic()    — [stub] closed-form dispatch for planes, cylinders, etc.
//   find_candidates() — coarse screen via per-triangle AABB (currently linear)
//   moller_trumbore() — exact ray-triangle test
//   refine()          — 2D Newton iteration on the true surface via D1()
//   inside_face()     — even-odd crossing-number boundary classification
//
// Multiple threads may call Perform() concurrently on the same loaded instance.
//
// Improvement roadmap (each item is a single-method replacement):
//
//   1. Adaptive mesh  (mesh_resolution)
//      Currently fixed 20×20. Query adaptor_.NbUIntervals(GeomAbs_C1) and
//      NbVIntervals(GeomAbs_C1) and return {n_u * 4, n_v * 4} (min 8 each).
//      This matches OCCT's sampling strategy and prevents missed hits on
//      surfaces with many knot spans.
//
//   2. Analytic dispatch  (try_analytic)
//      Currently returns nullopt. Add cases for GeomAbs_Plane (one equation),
//      GeomAbs_Cylinder / GeomAbs_Sphere / GeomAbs_Cone (quadratic), and
//      GeomAbs_Torus (quartic via IntAna_IntLinTorus). These bypass the mesh
//      entirely and return exact results. Most mechanical CAD faces are planes
//      or cylinders, so this is the highest-impact improvement.
//
//   3. Spatial index  (find_candidates)
//      Currently a flat O(n) slab-test scan over all triangles. Replace with
//      a BVH (e.g. std::vector organised as a binary heap over Morton-sorted
//      boxes) for O(log n) lookup. Matters when mesh grows with adaptive sizing.
//
//   4. Boundary classifier  (inside_face)
//      Currently an even-odd crossing-number test on the sampled 2D polygon.
//      Replace with BRepTopAdaptor_TopolTool::Classify() on pre-warmed private
//      face for exact PCurve-based classification on degenerate boundaries.
class RayFaceIntersector {
public:
    // Deep-copies face geometry and builds all internal state.
    // Not concurrency-safe with itself — call once per instance, in serial.
    void Load(const TopoDS_Face& face, double tolerance);

    // Returns all hits in ascending ray-parameter order.
    // Thread-safe: const, returns by value, touches no mutable state.
    RayResult Perform(const gp_Lin& line, double pmin, double pmax) const;

private:
    struct MeshTri {
        int i0, i1, i2;  // indices into points_
        double u0, v0;   // UV at vertex i0
        double u1, v1;   // UV at vertex i1
        double u2, v2;   // UV at vertex i2
    };

    // --- Extension point 1: mesh resolution ---------------------------------
    // Returns {nu, nv} grid dimensions used by build_mesh().
    // Currently fixed. See roadmap item 1 above for the adaptive version.
    std::pair<int, int> mesh_resolution() const;

    void build_mesh();
    void build_boundary();

    // --- Extension point 2: analytic dispatch --------------------------------
    // If the surface type admits a closed-form solution, returns it directly
    // so Perform() can skip the mesh path entirely. Currently always nullopt.
    // See roadmap item 2 above.
    std::optional<RayResult> try_analytic(
        const gp_Lin& line, double pmin, double pmax) const;

    // --- Extension point 3: spatial index ------------------------------------
    // Returns the indices of triangles whose AABB is not ruled out by the ray.
    // Currently a flat linear scan. See roadmap item 3 above for a BVH version.
    std::vector<std::size_t> find_candidates(const gp_Lin& line) const;

    // --- Phase: exact triangle test ------------------------------------------
    // Möller–Trumbore; writes barycentric (s,t) and ray parameter w.
    // Barycentric: vertex i0 weight = 1-s-t, i1 = s, i2 = t.
    bool moller_trumbore(const gp_Dir& dir, const gp_Pnt& orig,
                         const MeshTri& tri,
                         double* w, double* s, double* t) const;

    // --- Phase: Newton refinement --------------------------------------------
    // 2D Newton on the true surface; returns false when it fails to converge.
    bool refine(const gp_Lin& line,
                double u_approx, double v_approx,
                RayHit* hit) const;

    // --- Extension point 4: boundary classification --------------------------
    // Even-odd crossing-number test on the pre-built 2D boundary polygon.
    // See roadmap item 4 above for the topology-tool version.
    bool inside_face(double u, double v) const;

    SurfaceAdaptor adaptor_;
    TopoDS_Face    private_face_;
    double         tolerance_{1e-7};

    double u_min_{}, u_max_{}, v_min_{}, v_max_{};

    std::vector<gp_Pnt>  points_;    // 3D mesh vertices
    std::vector<MeshTri> triangles_; // triangle connectivity + UV corners
    std::vector<double>  boxes_;     // per-triangle AABB: 6 doubles per tri

    // 2D boundary polygon, one entry per wire, ordered PCurve sample points.
    std::vector<std::vector<gp_Pnt2d>> boundary_wires_;
};

} // namespace cad_uv_map::geom
