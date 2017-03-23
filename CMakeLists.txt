cmake_minimum_required(VERSION 2.8)

add_subdirectory(lab)
add_subdirectory(microprofile)
add_subdirectory(argparse)
add_subdirectory(vendor/shoom)

project(capsule)

set(shoom_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/shoom/include)

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

if(${CMAKE_C_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	set(CMAKE_COMPILER_IS_CLANG TRUE)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-field-initializers ${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
	set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-braces -Wno-missing-field-initializers ${CMAKE_C_FLAGS} -std=gnu99 -fno-strict-aliasing")
elseif(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

if(WIN32)
add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)
endif() # WIN32

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

    get_filename_component(FFMPEG_LIBRARY_DIR ${DEPS_DIR}/bin REALPATH)

    set(AVUTIL_DLL avutil-55.dll)
    set(AVCODEC_DLL avcodec-57.dll)
    set(AVFORMAT_DLL avformat-57.dll)
    set(SWSCALE_DLL swscale-4.dll)
    set(SWRESAMPLE_DLL swresample-2.dll)

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

set(microprofile_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/microprofile)

add_subdirectory(libcapsule)
add_subdirectory(capsulerun)
