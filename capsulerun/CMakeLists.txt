cmake_minimum_required(VERSION 2.8)

project(capsulerun)

set (capsulerun_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set (capsulerun_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(capsulerun_SOURCES
  ${capsulerun_SOURCE_DIR}/main.cpp)

add_executable(capsulerun ${capsulerun_SOURCES})
target_link_libraries(capsulerun capsule)

