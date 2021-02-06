add_library(rt_scene scene.c scene.h camera.c camera.h)
target_link_libraries(rt_scene PUBLIC rt_core rt_loaders rt_bvh)
