add_library(pcg pcg-c-basic/pcg_basic.c pcg-c-basic/pcg_basic.h)
set_target_properties(pcg PROPERTIES C_STANDARD 99)
target_include_directories(pcg INTERFACE pcg-c-basic)
