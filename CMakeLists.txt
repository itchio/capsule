cmake_minimum_required(VERSION 2.8)

project(capsule)

option(OSX_UNIVERSAL "Build an OSX universal binary" ON)

set(CAPSULE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libcapsule/include)

# Universal binary only makes sense on OSX
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  if(OSX_UNIVERSAL)
    message(STATUS "Building universal OSX binaries")
    set(COMPILE_FLAGS "${COMPILE_FLAGS} -arch i386 -arch x86_64")
  endif()
endif()

# custom FindXXX.cmake files, toolchains, etc.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_FIND_FRAMEWORK LAST)

set(CMAKE_C_FLAGS "${COMPILE_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMPILE_FLAGS} ${CMAKE_CXX_FLAGS} -std=c++11")

if(WIN32)
  if(MSVC)
    set(DEPS_PREFIX https://dl.itch.ovh/capsule-deps)
    set(DEPS_DIR ${PROJECT_BINARY_DIR}/deps)

    if(${CMAKE_GENERATOR} MATCHES "Win64$")
      message(STATUS "Using 64-bit windows dependencies")
      set(DEPS_URL ${DEPS_PREFIX}/capsule-deps-windows-amd64.tar.gz)
      get_filename_component(DEVIARE_INPROC_LIBRARY ${DEPS_DIR}/lib/NktHookLib64.lib REALPATH)
    else()
      message(STATUS "Using 32-bit windows dependencies")
      set(DEPS_URL ${DEPS_PREFIX}/capsule-deps-windows-386.tar.gz)
      get_filename_component(DEVIARE_INPROC_LIBRARY ${DEPS_DIR}/lib/NktHookLib.lib REALPATH)
    endif()

    include(ExternalProject)
    ExternalProject_Add(
      capsule_deps
      URL ${DEPS_URL}
      SOURCE_DIR ${PROJECT_BINARY_DIR}/deps
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
    )
    include_directories(${DEPS_DIR}/include)
  elseif()
    message(FATAL_ERROR "Only MSVC is supported on Windows. Sorry!")
  endif()
endif()

add_subdirectory(libcapsule)
add_subdirectory(capsulerun)
