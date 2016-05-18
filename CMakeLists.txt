cmake_minimum_required(VERSION 2.8)

project(libfake)

find_package(OpenGL REQUIRED)

set(MAIN_SRC
    main.cpp
    main.h)

set(LIBFAKE_SRC
    libfake.cpp
    libfake.h)

if(APPLE)
  list(APPEND MAIN_SRC libfake_cocoa.m)
endif()

add_library(libfake ${LIBFAKE_SRC})
add_executable(main ${MAIN_SRC})
target_link_libraries(main libfake)
