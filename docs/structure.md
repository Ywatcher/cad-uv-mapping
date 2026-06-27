read model:
```
BRepTools
BRep_Builder

TopoDS
TopExp
TopAbs
```

curve calculation:
```
BRepAdaptor_Surface

BRepLProp_SLProps

Geom_Surface

GeomAdaptor_Surface
```

projection:
```
GeomAPI_ProjectPointOnSurf
or
ShapeAnalysis_Surface
```

distance (from faces):
```
BRepExtrema_DistShapeShape
or
Extrema_ExtPS
```


intersection:
```
IntCurvesFace

BRepClass

BRepAlgoAPI
```

for optimization(optional): bounding box 
```
Bnd_Box

BRepBndLib

```

```text
OpenCASCADE (OCCT)
│
├── TopoDS / TopAbs / TopExp
│   Purpose: Topological representation
│   Examples: Shape, Solid, Face, Edge, Vertex
│
├── BRepTools / BRep_Builder
│   Purpose: Reading and writing BREP models
│
├── Geom / GeomAdaptor / BRepAdaptor
│   Purpose: Accessing the underlying geometric surfaces and curves
│
├── BRepLProp
│   Purpose: Evaluating local geometric properties
│   Examples: Surface normals, tangents, curvature
│
├── GeomAPI / Extrema / BRepExtrema
│   Purpose: Geometric queries such as projection, closest-point search,
│            and distance computation
│
├── Bnd / BRepBndLib
│   Purpose: Bounding box computation for spatial acceleration
│
├── BRepMesh
│   Purpose: Tessellating CAD surfaces into triangle meshes
│
└── ShapeAnalysis
    Purpose: Analysis and validation of shapes and surfaces,
             with utilities for detecting and repairing geometric issues
```

## Current Module Structure

### Python Side

- `python/cad_uv_map/__init__.py`
- `python/cad_uv_map/api.py`

Purpose:

- expose a small Python-facing entry surface
- convert build123d / OCP shapes into bytes for the native bridge
- keep notebook and test helpers thin

### Native C++ Side

- `cpp/src/bindings.cpp`
- `cpp/src/mapping.cpp`
- `cpp/src/face_info.cpp`
- `cpp/src/occt_io.cpp`

Headers:

- `cpp/include/cad_uv_map/indexed_record.hpp`
- `cpp/include/cad_uv_map/mapping.hpp`
- `cpp/include/cad_uv_map/mapping_context.hpp`
- `cpp/include/cad_uv_map/mapping_types.hpp`
- `cpp/include/cad_uv_map/sample.hpp`
- `cpp/include/cad_uv_map/surface_eval.hpp`
- `cpp/include/cad_uv_map/face_info.hpp`
- `cpp/include/cad_uv_map/occt_io.hpp`

Purpose:

- `bindings.cpp` exposes the native module to Python
- `occt_io.cpp` reads BREP bytes/files and extracts faces
- `face_info.cpp` prints and describes face metadata for debug use
- `mapping.cpp` is the mapping layer skeleton and future core algorithm home
- `indexed_record.hpp` carries stable sample indices so grouped or parallel work
  can write back into deterministic NumPy array order

### Runtime Order

The current intended execution order is:

1. Python prepares a low-face sample request or test fixture.
2. Python converts the shape to BREP bytes when needed.
3. `bindings.cpp` receives the bytes and forwards them to native code.
4. `occt_io.cpp` reconstructs the OCCT shape and collects faces.
5. `mapping.cpp` maps low-face UV samples to high faces.
6. `mapping.cpp` evaluates UV-to-normal data on the selected high faces.
7. `indexed_record.hpp`-backed result rows preserve the original sample order.
8. Python receives arrays or record structs for tests, baking, or debugging.

Why it is arranged this way:

- face lookup stays cheap by indexing into the face list directly
- OCCT surface evaluation is the expensive part, so it sits inside the native
  batch functions
- sample order is preserved by carrying an explicit index through every stage
- grouping by face is an implementation detail, not the public data model
- the normal stage can run per sample first, then be optimized later by face
  grouping if profiling says it matters

For the current development flow, the single-low-face mapping core should be
the innermost unit, with multi-face batch wrappers sitting above it.

## Possible Future Separations

- Split `mapping.cpp` into `sampling.cpp`, `mapping.cpp`, and `surface_eval.cpp`
  if those responsibilities grow independently.
- Split `mapping.hpp` into smaller headers if the public API gets too broad.
- Add a dedicated `shape_io.cpp` if file/bytes/shape loading expands beyond BREP.
- Add a small `context.hpp` or `config.hpp` if mapping tolerances and policies
  grow beyond the current `MappingContext`.
- Add a separate `atlas.cpp` or `bake.cpp` if atlas packing and texture output
  become first-class tasks.
- Keep debug-only face inspection separate from algorithm code so the runtime
  path stays focused on mapping and evaluation.
