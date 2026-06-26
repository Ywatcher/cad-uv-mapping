import numpy as np

from cad_uv_map.baking import normals_to_rgb


def test_normals_to_rgb_flat_z():
    rgb = normals_to_rgb(np.array([0.0, 0.0, 1.0]))

    assert rgb.tolist() == [127, 127, 255]
