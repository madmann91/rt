add_library(rt_accel accel.h bvh.c bvh.h)
target_link_libraries(rt_accel PUBLIC rt_core)
