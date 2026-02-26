cmake_minimum_required(VERSION 3.16)
project(african-head)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release by default for 60+ FPS
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(OpenMP)

add_executable(african-head
    main.cpp
    model.cpp
    renderer.cpp
)

target_link_libraries(african-head ${OpenCV_LIBS})
if(OpenMP_CXX_FOUND)
    target_link_libraries(african-head OpenMP::OpenMP_CXX)
endif()

# Copy assets to build directory
configure_file(african_head.obj african_head.obj COPYONLY)
configure_file(african_head_diffuse.tga african_head_diffuse.tga COPYONLY)
configure_file(african_head_nm_tangent.tga african_head_nm_tangent.tga COPYONLY)
configure_file(african_head_spec.tga african_head_spec.tga COPYONLY)
configure_file(african_head_eye_inner.obj african_head_eye_inner.obj COPYONLY)
configure_file(african_head_eye_inner_diffuse.tga african_head_eye_inner_diffuse.tga COPYONLY)
configure_file(african_head_eye_inner_nm_tangent.tga african_head_eye_inner_nm_tangent.tga COPYONLY)
configure_file(african_head_eye_inner_spec.tga african_head_eye_inner_spec.tga COPYONLY)
configure_file(african_head_eye_outer.obj african_head_eye_outer.obj COPYONLY)
configure_file(african_head_eye_outer_diffuse.tga african_head_eye_outer_diffuse.tga COPYONLY)
configure_file(african_head_eye_outer_nm_tangent.tga african_head_eye_outer_nm_tangent.tga COPYONLY)
configure_file(african_head_eye_outer_spec.tga african_head_eye_outer_spec.tga COPYONLY)
