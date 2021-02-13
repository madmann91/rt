add_library(rt_scene
    scene.c
    scene.h
    geometry.c
    geometry.h
    mesh.c
    mesh.h
    bsdf.c
    bsdf.h
    attr.h
    camera.c
    camera.h)
target_link_libraries(rt_scene PUBLIC rt_core rt_accel)
