add_executable(rt main.c)
target_link_libraries(rt PUBLIC rt_core)
set_property(TARGET rt PROPERTY INTERPROCEDURAL_OPTIMIZATION ${ENABLE_IPO})
