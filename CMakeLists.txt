cmake_minimum_required(VERSION 3.9)
project(rt)

add_subdirectory(contrib)

option(USE_DOUBLE_PRECISION
    "Use double precision everywhere in the renderer" OFF)
option(USE_ROBUST_BVH_TRAVERSAL
    "Use a more robust version of BVH traversal that is a bit slower but prevents false misses" OFF)
option(USE_64_BIT_MORTON_CODES
    "Uses 64-bit morton codes instead of 32-bit ones, which may increase precision during BVH construction" OFF)

set(CMAKE_C_STANDARD 11)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src)

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(test)
endif ()
