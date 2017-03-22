
set (SOURCE_DIR src)

set (SOURCES
  ${SOURCE_DIR}/lab/platform.cc
  ${SOURCE_DIR}/lab/env.cc
  ${SOURCE_DIR}/lab/strings.cc
  ${SOURCE_DIR}/lab/paths.cc
)

add_library(lab STATIC ${SOURCES})

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(shoom PRIVATE -W3 -EHsc)
else()
    target_compile_options(shoom PRIVATE -Wall -Wno-missing-braces -std=c++11 -fPIC)
endif()
