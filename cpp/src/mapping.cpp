#include "cad_uv_map/mapping.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRep_Tool.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <TopAbs.hxx>
#include <gp_Pnt.hxx>

namespace cad_uv_map {

MappingBatch map_low_face_samples_to_high_faces(
    const TopoDS_Face& low_face,
    std::int32_t low_face_id,
    const std::vector<UvCoord>& low_uv_samples,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingBatch batch;

    // TODO: this is the core single-face implementation.
    // TODO: accept a read-only shared context for tolerances and later cache use.
    //
    // Intended OCCT use:
    // - BRepAdaptor_Surface(low_face).Value(u, v) to recover each low sample point.
    // - BRepBuilderAPI_MakeVertex(point) to build a query point.
    // - BRepExtrema_DistShapeShape(vertex, high_face) to search candidate high faces.
    // - BRep_Tool::Surface_s(high_face) + ShapeAnalysis_Surface::ValueOfUV(point, tol)
    //   to recover native high UV coordinates.
    // - BRepLProp_SLProps(adaptor, u, v, 1, tol) if we want the low sample normal too.
    //
    // What we must do ourselves:
    // - choose the winner among high-face candidates
    // - define ambiguity tolerance
    // - reject outside-trim projections
    // - set MappingStatus consistently
    // - preserve deterministic ordering for tests
    // - fill `low_face_id` and sample UV payloads into each MappingResult

    (void)low_face;
    (void)low_face_id;
    (void)low_uv_samples;
    (void)high_faces;
    (void)shared_context;
    return batch;
}

MappingBatch map_low_face_sample_groups_to_high_faces(
    const FaceUvSampleBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces) {
    MappingBatch batch;

    // TODO: this wrapper should orchestrate the per-face core.
    // TODO: the straightforward version can call the single-face function once
    // per low face; later we can parallelize this loop here.
    //
    // Intended structure:
    // - iterate over FaceUvSamples entries
    // - resolve each low face index to the corresponding TopoDS_Face
    // - call map_low_face_samples_to_high_faces(...)
    // - append results in a deterministic order
    //
    // What we must do ourselves:
    // - decide how to handle invalid face ids
    // - decide whether empty sample groups are allowed
    // - control aggregation order for stable tests

    (void)low_face_samples;
    (void)low_faces;
    (void)high_faces;
    return batch;
}

MappingBatch map_uv_samples_to_high_faces(
    const std::vector<UvSample>& samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappingBatch batch;

    // TODO: this compatibility wrapper should group flat samples by low_face_id
    // and then forward to the face-grouped wrapper.
    //
    // Intended OCCT use is the same as the single-face core, but organized by
    // low-face groups before the call.
    //
    // What we must do ourselves:
    // - group by low_face_id
    // - build FaceUvSampleBatch
    // - call map_low_face_sample_groups_to_high_faces(...)

    (void)samples;
    (void)low_faces;
    (void)high_faces;
    (void)shared_context;
    return batch;
}

SurfaceEvalBatch evaluate_mapped_high_uvs(
    const MappingBatch& mapping,
    const std::vector<TopoDS_Face>& high_faces) {
    SurfaceEvalBatch batch;

    // TODO: this stage should only read the selected high face and UV per sample.
    // TODO: determine whether to cache adaptors per face before parallelizing.
    //
    // Intended OCCT use:
    // - BRepAdaptor_Surface(high_face)
    // - BRepLProp_SLProps(adaptor, u, v, 1, tol)
    // - props.IsNormalDefined()
    // - props.Normal()
    // - BRepAdaptor_Surface::Value(u, v) for the 3D point
    // - face.Orientation() / TopAbs_REVERSED to flip the normal when needed
    //
    // What we must do ourselves:
    // - map a `high_face_id` back to the face list safely
    // - handle missing/failed mappings
    // - define the fallback for undefined normals
    // - convert OCCT vectors into our plain POD result structures

    (void)mapping;
    (void)high_faces;
    return batch;
}

MappedSampleBatch map_and_evaluate_samples(
    const FaceUvSampleBatch& low_face_samples,
    const std::vector<TopoDS_Face>& low_faces,
    const std::vector<TopoDS_Face>& high_faces,
    const MappingContext* shared_context) {
    MappedSampleBatch batch;

    // TODO: this convenience path should just orchestrate the two stages above.
    // TODO: keep this function thin so tests can hit each stage independently.
    //
    // Intended structure:
    // 1. map low UV samples to high faces, one low face at a time.
    // 2. evaluate high UVs to normals.
    // 3. merge sample, mapping, and surface results into one record stream.
    //
    // What we must do ourselves:
    // - decide whether records are joined by index or by explicit sample ids
    // - keep errors visible instead of silently dropping failed samples
    // - decide how to expose partial failures to Python and tests

    (void)low_face_samples;
    (void)low_faces;
    (void)high_faces;
    (void)shared_context;
    return batch;
}

} // namespace cad_uv_map
