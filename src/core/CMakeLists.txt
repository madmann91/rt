configure_file(config.h.in config.h @ONLY)
add_library(rt_core
    bbox.h
    ray.h
    rgb.h
    vec2.h
    vec3.h
    utils.h
    alloc.c
    alloc.h
    image.h
    random.h
    mem_pool.c
    mem_pool.h
    radix_sort.c
    radix_sort.h
    thread_pool.c
    thread_pool.h)
find_package(Threads REQUIRED)

include(CheckLibraryExists)
check_library_exists(m sin "" HAS_LIBM)
set(LIBM)
if (HAS_LIBM)
    set(LIBM m)
endif ()

target_link_libraries(rt_core PUBLIC pcg Threads::Threads ${LIBM})
