cmake_minimum_required(VERSION 2.8)

project(capsule-test)

set(opengl-test_SOURCES src/opengl.cpp)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)

set(capsule-test_INCLUDE_DIRS ${SDL2_INCLUDE_DIR} ${GLEW_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} SYSTEM)
include_directories(${capsule-test_INCLUDE_DIRS})

add_executable(opengl-test ${opengl-test_SOURCES})
target_link_libraries(opengl-test ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY} ${GLEW_LIBRARY} ${OPENGL_LIBRARY})

