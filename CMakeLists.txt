cmake_minimum_required(VERSION 2.8)

project(libfake)

set(SDL2_DIR, deps)
# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

set(MAIN_SRC
    main.cpp)

set(LIBFAKE_SRC
    libfake.cpp
    libfake.h)

if(APPLE)
  list(APPEND MAIN_SRC libfake_cocoa.m)
endif()

add_library(libfake ${LIBFAKE_SRC})
add_executable(main ${MAIN_SRC})
target_link_libraries(main libfake)
