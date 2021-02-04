add_library(rt_bvh build.c intersect.c bvh.h tri.c tri.h)
target_link_libraries(rt_bvh PUBLIC rt_core)
