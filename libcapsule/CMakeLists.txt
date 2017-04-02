cmake_minimum_required(VERSION 2.8)

project(libcapsule)

if(APPLE)
  # MACOSX_RPATH is now enabled by default
  cmake_policy(SET CMP0042 NEW)
endif()

set(libcapsule_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(libcapsule_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(WIN32)
    find_package(OpenGL REQUIRED)
    find_package(DirectX REQUIRED)
endif()

set(libcapsule_INCLUDE_DIRS SYSTEM)

include_directories(
  ${shoom_INCLUDE_DIR}
  ${libcapsule_INCLUDE_DIRS}
  ${libcapsule_INCLUDE_DIR}
  ${lab_INCLUDE_DIR}
)

set(libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/logging.cc
    ${libcapsule_SOURCE_DIR}/ensure.cc
    ${libcapsule_SOURCE_DIR}/io.cc
    ${libcapsule_SOURCE_DIR}/capture.cc
    ${libcapsule_SOURCE_DIR}/gl_capture.cc
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/linux/hooks.cc
    ${libcapsule_SOURCE_DIR}/linux/dlopen_hooks.cc
    ${libcapsule_SOURCE_DIR}/linux/gl_hooks.cc
    ${libcapsule_SOURCE_DIR}/linux/alsa_hooks.cc
  )

  find_package(ALSA REQUIRED)
  include_directories(
    ${ALSA_INCLUDE_DIR}
  )
endif()

if(APPLE)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/macos/hooks.cc
    ${libcapsule_SOURCE_DIR}/macos/swizzle.mm
    ${libcapsule_SOURCE_DIR}/macos/ns_application.mm
    ${libcapsule_SOURCE_DIR}/macos/gl_hooks.mm
    ${libcapsule_SOURCE_DIR}/macos/coreaudio_hooks.mm
  )
endif()

if(WIN32)
  list(APPEND libcapsule_SRC
    ${libcapsule_SOURCE_DIR}/windows/hooks.cc
    ${libcapsule_SOURCE_DIR}/windows/gl_hooks.cc
    ${libcapsule_SOURCE_DIR}/windows/process_hooks.cc
    ${libcapsule_SOURCE_DIR}/windows/dxgi_hooks.cc
    ${libcapsule_SOURCE_DIR}/windows/dxgi_vtable_helpers.cc
    ${libcapsule_SOURCE_DIR}/windows/d3d9_hooks.cc
    ${libcapsule_SOURCE_DIR}/windows/d3d9_vtable_helpers.cc
    ${libcapsule_SOURCE_DIR}/windows/d3d9_capture.cc
    ${libcapsule_SOURCE_DIR}/windows/d3d10_capture.cc
    ${libcapsule_SOURCE_DIR}/windows/d3d11_capture.cc
    ${libcapsule_SOURCE_DIR}/windows/gl_capture.cc
    ${libcapsule_SOURCE_DIR}/windows/dc_capture.cc
    ${libcapsule_SOURCE_DIR}/windows/wasapi_hooks.cc
  )
endif()

add_library(capsule SHARED ${libcapsule_SRC})

target_link_libraries(capsule shoom)
target_link_libraries(capsule lab)

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
  # undefined symbol __cpu_model is your new best friend
  target_link_libraries(capsule -lgcc)
endif()

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
set(LIBCAPSULE_ARCH_SUFFIX "64")
else()
set(LIBCAPSULE_ARCH_SUFFIX "32")
endif() # 32/64-bit test

# the macOS version is universal, no suffix needed
if(APPLE)
set(LIBCAPSULE_ARCH_SUFFIX "")
endif()

install(
  FILES $<TARGET_FILE:capsule>
  RENAME "${CMAKE_SHARED_LIBRARY_PREFIX}capsule${LIBCAPSULE_ARCH_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}"
  DESTINATION "${CMAKE_BINARY_DIR}/dist"
)
