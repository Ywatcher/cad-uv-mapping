# Mapping Model

The native mapper should produce a sample-wise relation from low CAD faces to high
CAD surfaces.

## Input

- Low faces: render mesh source faces.
- High faces: detail source faces.
- Samples: low face id plus low `(u, v)` sample coordinates.

## Output

For every low-face sample:

- `status`: hit, no hit, ambiguous, outside trim, failed.
- `high_face_id`: selected high-detail face.
- `high_uv`: native high face `(u, v)`.
- `position`: selected world-space high point.
- `normal`: selected world-space high normal.
- `distance`: projection/ray distance from the low sample.

## First Algorithms

1. Nearest surface projection.
2. Normal ray projection.
3. Hybrid: normal ray first, nearest projection fallback inside tolerance.
