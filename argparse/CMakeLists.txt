
project(argparse)

set (SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set (SOURCES
  ${SOURCE_DIR}/argparse.c
)

add_library(argparse STATIC ${SOURCES})

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(microprofile PRIVATE)
else()
    target_compile_options(microprofile PRIVATE -fPIC)
endif()
