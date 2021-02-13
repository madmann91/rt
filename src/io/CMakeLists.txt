add_library(rt_io
    import_obj.c
    import_obj.h
    obj_model.c
    obj_model.h
    png_image.c
    png_image.h)
target_link_libraries(rt_io PUBLIC rt_core rt_scene PNG::PNG)
