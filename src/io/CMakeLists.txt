add_library(rt_loaders
    obj_model.c
    obj_model.h
    png_image.c
    png_image.h)

target_link_libraries(rt_loaders PUBLIC rt_core PNG::PNG)
