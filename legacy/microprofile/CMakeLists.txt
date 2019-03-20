
project(microprofile)

set (SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set (SOURCES
  ${SOURCE_DIR}/microprofile.cpp
)

add_definitions(-DMICROPROFILE_USE_CONFIG)

add_library(microprofile STATIC ${SOURCES})

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(microprofile PRIVATE)
else()
    target_compile_options(microprofile PRIVATE -fPIC -std=c++11)
endif()

if(WIN32)
target_link_libraries(microprofile ws2_32.lib)
endif()
