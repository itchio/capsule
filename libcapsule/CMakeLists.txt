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
    ${libcapsule_SOURCE_DIR}/shared/capsule.cpp)

if(APPLE)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/macos/capsule_macos_utils.m
    ${libcapsule_SOURCE_DIR}/macos/capsule_NSWindow.m
    ${libcapsule_SOURCE_DIR}/macos/capsule_NSApplication.m
    ${libcapsule_SOURCE_DIR}/macos/capsule_CGL.m
    ${libcapsule_SOURCE_DIR}/macos/capsule_CoreAudio.mm

    ${libcapsule_SOURCE_DIR}/macos/playthrough/CAPlayThrough.cpp
    ${libcapsule_SOURCE_DIR}/macos/playthrough/CARingBuffer.cpp
    ${libcapsule_SOURCE_DIR}/macos/playthrough/CAStreamBasicDescription.cpp
    ${libcapsule_SOURCE_DIR}/macos/playthrough/AudioDevice.cpp
  )
endif()

if(WIN32)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/windows/capsule_d3d8.cpp
    ${libcapsule_SOURCE_DIR}/windows/capsule_d3d11.cpp
  )
endif()

if(WIN32)
  add_subdirectory(PolyHook)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/PolyHook/source/PolyHook/)
  if(MSVC)
    if(${CMAKE_GENERATOR} MATCHES "Win64$")
      message(STATUS "Using 64-bit dependencies")
      set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps/64)
      include_directories(${PROJECT_SOURCE_DIR}/deps/include)
      link_directories(${CMAKE_CURRENT_SOURCE_DIR}/PolyHook/source/Capstone/msvc/x64/Release)
    else()
      message(STATUS "Using 32-bit dependencies")
      set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps/32)
      include_directories(${PROJECT_SOURCE_DIR}/deps/include)
      link_directories(${CMAKE_CURRENT_SOURCE_DIR}/PolyHook/source/Capstone/msvc/x86/Release)
    endif()
  elseif()
    message(FATAL_ERROR "Only MSVC is supported on Windows. Sorry!")
  endif()
endif()

add_library(capsule SHARED ${libcapsule_SRC})

if(APPLE)
  find_library(COCOA_LIBRARY Cocoa)
  target_link_libraries(capsule ${COCOA_LIBRARY})
  find_library(COREGRAPHICS_LIBRARY CoreGraphics)
  target_link_libraries(capsule ${COREGRAPHICS_LIBRARY})

  find_library(OPENGL_LIBRARY OpenGL)
  target_link_libraries(capsule ${OPENGL_LIBRARY})

  find_library(CORESERVICES_LIBRARY CoreServices)
  target_link_libraries(capsule ${CORESERVICES_LIBRARY})
  find_library(COREAUDIO_LIBRARY CoreAudio)
  target_link_libraries(capsule ${COREAUDIO_LIBRARY})
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  target_link_libraries(capsule ${AUDIOUNIT_LIBRARY})
  find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
  target_link_libraries(capsule ${AUDIOTOOLBOX_LIBRARY})
endif()

if(WIN32)
  target_link_libraries(capsule polyhook)
endif()
