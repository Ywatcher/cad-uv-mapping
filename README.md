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

Use the same conda environment as the Panda3D CAD experiments:

```bash
/mnt/D/packages/miniconda3/envs/game-qt6-py312/bin/python -m pip install -e . --no-build-isolation
```

You need OpenCascade headers/libraries visible to CMake. In conda environments this
usually means activating the env before building, or setting `OpenCASCADE_DIR`.

## First Milestones

1. Build/import the native module.
2. Load a BREP file and count faces.
3. Sample a low face's UV domain and evaluate points/normals.
4. Project each low sample to high faces.
5. Return deterministic NumPy arrays.
6. Add normal-map/mask baking helpers in Python.



