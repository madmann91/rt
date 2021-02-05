add_library(rt_bvh bvh.c bvh.h tri.c tri.h)
target_link_libraries(rt_bvh PUBLIC rt_core)
