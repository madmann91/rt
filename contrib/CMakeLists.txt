add_library(tomlc99 tomlc99/toml.c tomlc99/toml.h)
set_target_properties(tomlc99 PROPERTIES C_STANDARD 99)
target_include_directories(tomlc99 INTERFACE tomlc99)

add_library(pcg pcg-c-basic/pcg_basic.c pcg-c-basic/pcg_basic.h)
set_target_properties(pcg PROPERTIES C_STANDARD 99)
target_include_directories(pcg INTERFACE pcg-c-basic)
