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

## Why This Shape

The mapping stage is sample-wise because one low face can legitimately map to
different high faces within the same grid.

That means:

- `high_face_id` must stay per-sample, not per-batch
- the normal stage should read `high_face_id` and `high_uv` from each sample
- output order should be preserved by sample index, not by face grouping

The face id lookup itself is cheap. The real cost comes from OCCT surface
adaptor setup and per-sample geometry evaluation. That is why the data model
keeps samples independent, while implementations are free to batch or parallelize
behind the scenes.

## First Algorithms

1. Nearest surface projection.
2. Normal ray projection.
3. Hybrid: normal ray first, nearest projection fallback inside tolerance.
