cmake_policy(SET CMP0048 NEW)

project(sia_glrenderer)

cmake_minimum_required(VERSION 3.2.0)

add_subdirectory(ext ext_build)

include_directories(
  # include files
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  # Eigen linear algebra library
  SYSTEM ${EIGEN_INCLUDE_DIR}
  # GLFW library for OpenGL context creation
  SYSTEM ${GLFW_INCLUDE_DIR}
  # SOIL2 library for OpenGL textures
  SYSTEM ${SOIL_INCLUDE_DIR}
  # GLBINDING library for OpenGL type checking
  SYSTEM ${GLBINDING_INCLUDE_DIR}
  SYSTEM ${GLBINDING_AUX_INCLUDE_DIR}
  # PMP-library for halfedge mesh
  SYSTEM ${PMP_INCLUDE_DIR}
  # Dear Imgui library for the GUI
  SYSTEM ${IMGUI_INCLUDE_DIR}
)

file(GLOB SOURCES src/*.cpp)
file(GLOB HEADERS src/*.h)

add_definitions(-DDATA_DIR="${PROJECT_SOURCE_DIR}/data")

add_executable(sia_glrenderer ${SOURCES} ${HEADERS} ${IMGUI_SOURCE_FILES})

target_link_libraries(sia_glrenderer PRIVATE glfw pmp glbinding::glbinding glbinding::glbinding-aux SOIL)

function(IndicateExternalFile _target)
    foreach(_file IN ITEMS ${ARGN})
        if ((IS_ABSOLUTE "${_file}" AND EXISTS "${_file}") OR
            (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_file}"))
            target_sources(${_target} PRIVATE "${_file}")
        endif()
    endforeach()
endfunction()

file(GLOB SHADERS "data/shaders/*.vert" "data/shaders/*.frag")
IndicateExternalFile("sia_glrenderer" ${SHADERS})

target_compile_features(sia_glrenderer PRIVATE cxx_std_14)