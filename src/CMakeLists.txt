cmake_minimum_required(VERSION 3.16.2)

project(MotionInterpolation) 

add_executable(MotionInterpolation)

# 🖕🖕🖕🖕🖕🖕
set_target_properties(MotionInterpolation PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)

set(textfiles
  motion.cl
  mesh.frag
  mesh.vert
  meshtextured.frag
  meshtextured.vert
)

target_sources(MotionInterpolation PUBLIC
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