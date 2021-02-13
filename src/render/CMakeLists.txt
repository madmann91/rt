add_library(rt_render render_debug.c render.h)
target_link_libraries(rt_render PUBLIC rt_scene)
