add_library(lz4 STATIC lz4.h lz4.c)
set_property(TARGET lz4 PROPERTY POSITION_INDEPENDENT_CODE ON)