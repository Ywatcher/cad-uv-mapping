# cad-uv-map

Native OpenCascade-backed UV/surface mapping experiments for baking high-detail CAD
surfaces onto low-detail CAD faces.

The package is intentionally baking-agnostic: the native core should produce mapping
arrays such as high face id, high UV, world position, normal, distance, and status.
Normal maps, masks, height maps, and other textures are downstream consumers.

## Initial Compatibility Model

Start with a stable file/data boundary:

```text
Python build123d/OCP shape -> BREP/STEP file -> C++ OCCT mapper -> NumPy arrays
```

Avoid accepting live `OCP.TopoDS_Face` objects until the package and OCCT versioning
story is stable.

## Development Install


```bash
python -m pip install -e . --no-build-isolation
```

You need OpenCascade headers/libraries visible to CMake. In conda environments this
usually means activating the env before building, or setting `OpenCASCADE_DIR`.

## Current Features

The native pipeline currently provides:

1. UV sampling on OCCT faces, including grouped batches and deterministic sample indices.
2. Low-to-high UV mapping, with nearest-surface and ray-based projection paths.
3. High-face surface evaluation, returning point, normal, and normal-defined flags.
4. BREP bytes/file loading and face extraction for Python-to-C++ bridging.
5. NumPy-friendly result containers and structured exports for tests, notebooks, and rendering.
6. Notebook and debug helpers for inspecting faces, samples, mapping results, and evaluation output.

These pieces are already enough to drive rendering-oriented workflows such as normal-map baking and other per-sample surface queries.

## Docs

- [API reference](docs/api_reference.md): step APIs, arguments, return values,
  and the matching native entry points.
- [Module structure](docs/structure.md): file layout and runtime order.
- [Test flows](docs/test_flows.md): reference methods and comparison paths.

## Future Work

Planned follow-up work includes:

1. Handle the fold / hangover edge case more robustly.
2. Explore an outer-envelope or one-sided cage prepass for ambiguous folds.
3. Add bent normal outputs for shading-oriented consumers.
4. Add ambient occlusion outputs for shading and visibility hints.
5. Expand the projection policy when a single low-face sample can plausibly map to multiple high-face regions.
6. Split additional preprocessing or atlas/baking utilities into their own modules if those workflows grow.

## First Milestones

1. Build/import the native module.
2. Load a BREP file and count faces.
3. Sample a low face's UV domain and evaluate points/normals.
4. Project each low sample to high faces.
5. Return deterministic NumPy arrays.
6. Add normal-map/mask baking helpers in Python.


