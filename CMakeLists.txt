cmake_minimum_required(VERSION 3.9)
project(rt)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED LANGUAGES C)

option(USE_DOUBLE_PRECISION
    "Use double precision everywhere in the renderer" OFF)
option(USE_ROBUST_BVH_TRAVERSAL
    "Use a more robust version of BVH traversal that is a bit slower but prevents false misses" OFF)
option(USE_64_BIT_MORTON_CODES
    "Uses 64-bit morton codes instead of 32-bit ones, which may increase precision during BVH construction" OFF)
option(USE_LINK_TIME_OPTIMIZATION
    "Enables link-time optimization in 'Release' mode" ${IPO_SUPPORTED})

if (USE_LINK_TIME_OPTIMIZATION AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ENABLE_IPO ON)
else ()
    set(ENABLE_IPO OFF)
endif ()

# Build 3rd-party libraries
add_subdirectory(contrib)

set(CMAKE_C_STANDARD 11)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

find_package(PNG 1.6 REQUIRED)

add_subdirectory(src)

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(test)
endif ()
