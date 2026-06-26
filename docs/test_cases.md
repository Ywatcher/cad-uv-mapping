# Test Cases

Start with deterministic geometry cases before adding large assets.

1. Identity box: low and high are identical.
2. Flat low top to carved high top: U/cross groove cuboid.
3. V-groove cuboid: sharp normal transitions.
4. Ribbed pedestal: curved surface and periodic seam pressure.
5. Cylinder seam: continuity near `u = 0` and `u = 2*pi`.
6. Trimmed face with hole: reject samples outside valid trims.
7. Split/merged topology: low one face, high many faces.
8. Reversed orientation: normals are handled intentionally.
9. No-hit area: missing high coverage reports failure.
10. Parallel determinism: workers 1/2/8/16 produce equivalent results.
