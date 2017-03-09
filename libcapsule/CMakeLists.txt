cmake_minimum_required(VERSION 2.8)

if(APPLE)
  # MACOSX_RPATH is now enabled by default
  cmake_policy(SET CMP0042 NEW)
endif()

set(libcapsule_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(libcapsule_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    find_package(OpenGL REQUIRED)
    find_package(DirectX REQUIRED)
endif()

set(libcapsule_INCLUDE_DIRS SYSTEM)

include_directories(${libcapsule_INCLUDE_DIRS} ${libcapsule_INCLUDE_DIR})

add_definitions(-DBUILD_CAPSULE_DLL)

set(libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/shared/capsule.cpp
    ${libcapsule_SOURCE_DIR}/shared/gl-capture.cpp
    ${libcapsule_SOURCE_DIR}/shared/io.cpp
    ${libcapsule_SOURCE_DIR}/shared/timer.cpp
    ${libcapsule_SOURCE_DIR}/shared/logging.cpp
    ${libcapsule_SOURCE_DIR}/shared/stb_image.cpp
)

if(APPLE)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/macos/capsule_macos_utils.mm
    ${libcapsule_SOURCE_DIR}/macos/capsule_NSWindow.mm
    ${libcapsule_SOURCE_DIR}/macos/capsule_NSApplication.mm
    ${libcapsule_SOURCE_DIR}/macos/capsule_CGL.mm
    ${libcapsule_SOURCE_DIR}/macos/capsule_CoreAudio.mm

    ${libcapsule_SOURCE_DIR}/macos/playthrough/CAPlayThrough.cpp
    ${libcapsule_SOURCE_DIR}/macos/playthrough/CARingBuffer.cpp
    ${libcapsule_SOURCE_DIR}/macos/playthrough/CAStreamBasicDescription.cpp
    ${libcapsule_SOURCE_DIR}/macos/playthrough/AudioDevice.cpp
  )
endif()

if(WIN32)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/windows/hooks.cpp
    ${libcapsule_SOURCE_DIR}/windows/process-hooks.cpp
    ${libcapsule_SOURCE_DIR}/windows/dxgi-hooks.cpp
    ${libcapsule_SOURCE_DIR}/windows/dxgi-vtable-helpers.cpp
    ${libcapsule_SOURCE_DIR}/windows/d3d9-hooks.cpp
    ${libcapsule_SOURCE_DIR}/windows/d3d9-vtable-helpers.cpp
    ${libcapsule_SOURCE_DIR}/windows/d3d9-capture.cpp
    ${libcapsule_SOURCE_DIR}/windows/d3d10-capture.cpp
    ${libcapsule_SOURCE_DIR}/windows/d3d11-capture.cpp
    ${libcapsule_SOURCE_DIR}/windows/gl-capture.cpp
    ${libcapsule_SOURCE_DIR}/windows/ddraw-hooks.cpp
    ${libcapsule_SOURCE_DIR}/windows/ddraw-capture.cpp
    ${libcapsule_SOURCE_DIR}/windows/dc-capture.cpp
  )
endif()

add_library(capsule SHARED ${libcapsule_SRC})

if(WIN32)
  add_dependencies(capsule capsule_deps)
  target_link_libraries(capsule ${DEVIARE_INPROC_LIBRARY})
endif()

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

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # dlopen, dlsym
  target_link_libraries(capsule -ldl)
  # needed by pulse-simple
  target_link_libraries(capsule -lpthread)
  # shm_open, etc.
  target_link_libraries(capsule -lrt)
endif()
