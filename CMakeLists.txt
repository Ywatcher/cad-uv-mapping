cmake_minimum_required(VERSION 3.24)
project(cad_uv_map LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 CONFIG REQUIRED)
find_package(VTK CONFIG QUIET)
find_package(OpenCASCADE CONFIG REQUIRED)

pybind11_add_module(_native
  cpp/src/bindings.cpp
  cpp/src/occt_io.cpp
  cpp/src/face_info.cpp
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

install(TARGETS _native DESTINATION cad_uv_map)
