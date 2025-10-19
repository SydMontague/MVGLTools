add_library(lz4 STATIC lz4.h lz4.c lz4hc.h lz4hc.c)
set_property(TARGET lz4 PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(lz4
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
