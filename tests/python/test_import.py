def test_import():
    import cad_uv_map

    assert hasattr(cad_uv_map, "describe_brep_faces")
