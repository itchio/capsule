cmake_minimum_required(VERSION 2.8)

# MACOSX_RPATH is now enabled by default
cmake_policy(SET CMP0042 NEW)

project(capsule)

option(BUILD_TESTS "Build tests" OFF)
option(OSX_UNIVERSAL "Build an OSX universal binary" ON)

set(CAPSULE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libcapsule/include)

# Universal binary only makes sense on OSX
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  if(OSX_UNIVERSAL)
    message(STATUS "Building universal OSX binaries")
    set(COMPILE_FLAGS "${COMPILE_FLAGS} -arch i386 -arch x86_64")
  endif()
endif()

# includes cmake/FindSDL2.cmake, etc.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_FIND_FRAMEWORK LAST)

find_package(OpenGL REQUIRED)

set(FFMPEG_URL http://ffmpeg.org/releases/)
set(FFMPEG_BZ2 ffmpeg-3.1.2.tar.bz2)
set(FFMPEG_SHA1 dce05dedce6332867c996a33c596385ae6934a37)
include(ffmpeg/ExternalFFMPEG.cmake)

if(WIN32)
    find_package(DirectX REQUIRED)
endif()

set(CAPSULE_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR} ${SDL2_INCLUDE_DIR} ${GLEW_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} SYSTEM)

if(WIN32)
    list(APPEND CAPSULE_INCLUDE_DIRS PolyHook/source/PolyHook)
endif()
include_directories(${CAPSULE_INCLUDE_DIRS} ${CAPSULE_INCLUDE_DIR})

SET(CMAKE_C_FLAGS "${COMPILE_FLAGS} ${CMAKE_C_FLAGS}")
SET(CMAKE_CXX_FLAGS "${COMPILE_FLAGS} ${CMAKE_CXX_FLAGS}")

add_subdirectory(libcapsule)
add_subdirectory(capsulerun)
if(BUILD_TESTS)
  add_subdirectory(test)
endif()

add_dependencies(capsulerun capsule)
