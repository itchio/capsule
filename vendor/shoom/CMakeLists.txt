
cmake_minimum_required(VERSION 2.8)

project(shoom)

option(SHOOM_BUILD_TESTS "Build tests for shoom" ON)

include_directories("include")

if (SHOOM_BUILD_TESTS)
    add_subdirectory(test)
endif()

enable_testing()

set (SOURCE_DIR src)

if (${CMAKE_GENERATOR} MATCHES "Visual")
set (SOURCES
    ${SOURCE_DIR}/shoom_win32.cc
)
else()
set (SOURCES
    ${SOURCE_DIR}/shoom_unix_darwin.cc
)
endif()

add_library(shoom STATIC ${SOURCES})

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(shoom PUBLIC -W3 -EHsc)
else()
    target_compile_options(shoom PUBLIC -Wall -Wno-missing-braces -std=c++11 -fPIC)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(shoom -lrt) # shm_*
endif() # Linux
