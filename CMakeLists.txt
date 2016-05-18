cmake_minimum_required(VERSION 2.8)

project(libfake)

set(SDL2_DIR, deps)
# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

add_subdirectory(glew/build/cmake)
add_subdirectory(MologieDetours)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

include_directories(SYSTEM ${SDL2_INCLUDE_DIR} glew/include MologieDetours)

set(MAIN_SRC
    main.cpp)

add_definitions(-DBUILD_LIBFAKE_DLL)

set(LIBFAKE_SRC
    libfake.cpp
    libfake.h)

if(APPLE)
  list(APPEND MAIN_SRC libfake_cocoa.m)
endif()

add_library(libfake ${LIBFAKE_SRC})
target_link_libraries(libfake MologieDetours)

add_executable(main ${MAIN_SRC})
target_link_libraries(main libfake ${SDL2_LIBRARY} glew)

add_dependencies(libfake MologieDetours)
add_dependencies(libfake glew)

MESSAGE(STATUS "glew_LIBRARY:         " ${glew_LIBRARY})
