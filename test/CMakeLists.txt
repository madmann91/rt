add_executable(thread_pool_reuse    thread_pool_reuse.c)
add_executable(thread_pool_recreate thread_pool_recreate.c)
add_executable(sort                 sort.c)
add_executable(mandelbrot           mandelbrot.c)
find_package(OpenMP QUIET)
if (OpenMP_FOUND)
    add_executable(mandelbrot_omp mandelbrot.c)
    target_compile_definitions(mandelbrot_omp PUBLIC -DUSE_OPENMP)
    target_link_libraries(mandelbrot_omp PUBLIC rt_core OpenMP::OpenMP_C)
    set_property(
        TARGET mandelbrot_omp
        PROPERTY INTERPROCEDURAL_OPTIMIZATION ${ENABLE_IPO})
endif ()
target_link_libraries(thread_pool_reuse    PUBLIC rt_core)
target_link_libraries(thread_pool_recreate PUBLIC rt_core)
target_link_libraries(mandelbrot           PUBLIC rt_core)
target_link_libraries(sort                 PUBLIC rt_core)
set_property(
    TARGET thread_pool_reuse thread_pool_recreate mandelbrot sort
    PROPERTY INTERPROCEDURAL_OPTIMIZATION ${ENABLE_IPO})

add_test(NAME thread_pool_reuse    COMMAND thread_pool_reuse)
add_test(NAME thread_pool_recreate COMMAND thread_pool_recreate)
add_test(NAME mandelbrot           COMMAND mandelbrot)
add_test(NAME sort                 COMMAND sort)
