
project(lab)

set (SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set (SOURCES
  ${SOURCE_DIR}/lab/platform.cc
  ${SOURCE_DIR}/lab/env.cc
  ${SOURCE_DIR}/lab/strings.cc
  ${SOURCE_DIR}/lab/paths.cc
  ${SOURCE_DIR}/lab/io.cc
  ${SOURCE_DIR}/lab/packet.cc
)

add_definitions(-D_UNICODE)

add_library(lab STATIC ${SOURCES})

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(lab PRIVATE -W3 -EHsc)
else()
    target_compile_options(lab PRIVATE -Wall -Wno-missing-braces -std=c++11 -fPIC)
endif()
