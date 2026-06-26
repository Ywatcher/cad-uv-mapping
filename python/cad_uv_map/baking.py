from __future__ import annotations

import numpy as np


def normals_to_rgb(normals: np.ndarray) -> np.ndarray:
    """Pack normal vectors in [-1, 1] into uint8 RGB normal-map pixels."""
    arr = np.asarray(normals, dtype=np.float64)
    if arr.shape[-1] != 3:
        raise ValueError(f"expected last dimension of size 3, got {arr.shape}")
    return np.clip((arr * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)
