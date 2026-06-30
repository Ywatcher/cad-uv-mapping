cmake_minimum_required(VERSION 3.24)
project(cad_uv_map LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_TSAN "Build with ThreadSanitizer (detects data races)" OFF)
option(ENABLE_ASAN "Build with AddressSanitizer (detects memory corruption)" OFF)

find_package(pybind11 CONFIG REQUIRED)
find_package(VTK CONFIG QUIET)
find_package(OpenCASCADE CONFIG REQUIRED)

pybind11_add_module(_native
  cpp/src/bindings.cpp
  cpp/src/occt_io.cpp
  cpp/src/face_info.cpp
  cpp/src/sample.cpp
  cpp/src/compat.cpp
  cpp/src/numpy_exports.cpp
  cpp/src/geom/SurfaceAdaptor.cpp
  cpp/src/geom/PointFaceProjector.cpp
  cpp/src/geom/RayFaceIntersector.cpp
  cpp/src/geom/ShapeIntersector.cpp
  cpp/src/geom/DistShapeShape.cpp
  cpp/src/projection/mapping_projection.cpp
  cpp/src/projection/projection_nearest.cpp
  cpp/src/projection/projection_ray.cpp
  cpp/src/mapping.cpp
)

target_include_directories(_native PRIVATE
  cpp/include
  ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(_native PRIVATE
  ${OpenCASCADE_FoundationClasses_LIBRARIES}
  ${OpenCASCADE_ModelingData_LIBRARIES}
  ${OpenCASCADE_ModelingAlgorithms_LIBRARIES}
)

if(ENABLE_TSAN)
    target_compile_options(_native PRIVATE -fsanitize=thread -fno-omit-frame-pointer -g)
    target_link_options(_native PRIVATE -fsanitize=thread)
endif()

if(ENABLE_ASAN)
    target_compile_options(_native PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(_native PRIVATE -fsanitize=address)
endif()

install(TARGETS _native DESTINATION cad_uv_map)
