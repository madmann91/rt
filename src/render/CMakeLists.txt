add_library(rt_render debug.c integrators.h)
target_link_libraries(rt_render PUBLIC rt_scene)
