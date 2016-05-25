cmake_minimum_required(VERSION 2.8)

# MACOSX_RPATH is now enabled by default
cmake_policy(SET CMP0042 NEW)

project(libcapsule)

set (libcapsule_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set (libcapsule_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(OpenGL REQUIRED)

if(WIN32)
    find_package(DirectX REQUIRED)
endif()

set(libcapsule_INCLUDE_DIRS SYSTEM)

include_directories(${libcapsule_INCLUDE_DIRS} ${libcapsule_INCLUDE_DIR})

add_definitions(-DBUILD_CAPSULE_DLL)

set(libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/capsule.cpp)

if(APPLE)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/capsule_cocoa.m
  )
endif()

if(WIN32)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/capsule_d3d8.cpp
    ${libcapsule_SOURCE_DIR}/capsule_d3d11.cpp
  )
endif()

add_library(capsule SHARED ${libcapsule_SRC})

if(WIN32)
  if(MSVC)
    if(${CMAKE_GENERATOR} MATCHES "Win64$")
      message(STATUS "Using 64-bit dependencies")
      set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps/64)
      include_directories(${PROJECT_SOURCE_DIR}/deps/include)
    else()
      message(STATUS "Using 32-bit dependencies")
      set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps/32)
      include_directories(${PROJECT_SOURCE_DIR}/deps/include)
    endif()
  elseif()
    message(FATAL_ERROR "Only MSVC is supported on Windows. Sorry!")
  endif()
endif()

if(APPLE)
  find_library(COCOA_LIBRARY Cocoa)
  target_link_libraries(capsule ${COCOA_LIBRARY})
endif()

