cmake_minimum_required(VERSION 3.16.2)

project(MotionInterpolation) 

# 🖕🖕🖕🖕🖕🖕
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Search for OpenCL and add paths
find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(textfiles
  motion.cl
  mesh.frag
  mesh.vert
  meshtextured.frag
  meshtextured.vert
)
add_executable(MotionInterpolation 
  MotionInterpolationTask.h
  MotionInterpolationTask.cpp
  Wrapper.h
  Wrapper.cpp
  CTriMesh.h
  CTriMesh.cpp
  CLUtil.h
  CLUtil.cpp
  CGLTexture.h
  CGLTexture.cpp
  GLCommon.h
  GLCommon.cpp
  main.cpp
  HLSL.h
  HLSLEx.h
  HLSL.cpp
  util.h
  util.cpp
  ${textfiles}
)

foreach(fname ${textfiles})
  configure_file(${fname} ${fname} COPYONLY)
endforeach(fname)


target_link_libraries(MotionInterpolation
  OpenGL::OpenGL
  OpenGL::GLU
  GLEW::GLEW
  glfw
  PNG::PNG
  OpenCL
)