cmake_minimum_required(VERSION 2.8)

# MACOSX_RPATH is now enabled by default
cmake_policy(SET CMP0042 NEW)

project(capsule)

# Based on Visual C++ 2015, adapting to 2013 should be trivial
if(MSVC)
  if(${CMAKE_GENERATOR} MATCHES "Win64$")
    message(STATUS "Using 64-bit dependencies")
    set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps/64)
    set(NKT_HOOK_LIBRARY ${CMAKE_PREFIX_PATH}/lib/NktHookLib64_Debug.lib)
  else()
    message(STATUS "Using 32-bit dependencies")
    set(CMAKE_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps/32)
    set(NKT_HOOK_LIBRARY ${CMAKE_PREFIX_PATH}/lib/NktHookLib_Debug.lib)
  endif()
endif()

# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_FIND_FRAMEWORK LAST)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)

if(WIN32)
    find_package(DirectX REQUIRED)
endif()

set(CAPSULE_INCLUDE_DIRS ${SDL2_INCLUDE_DIR} ${GLEW_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} SYSTEM)

if(WIN32)
  list(APPEND CAPSULE_INCLUDE_DIRS ../Deviare-InProc/Include)
endif()
include_directories(${CAPSULE_INCLUDE_DIRS})

set(MAIN_SRC
    main.cpp)

add_definitions(-DBUILD_CAPSULE_DLL)

set(CAPSULE_SRC
    capsule.cpp
    capsule.h)

if(APPLE)
  list(APPEND CAPSULE_SRC capsule_cocoa.m)
endif()

if(WIN32)
  list(APPEND CAPSULE_SRC capsule_d3d8.cpp capsule_d3d11.cpp)
endif()

add_library(capsule SHARED ${CAPSULE_SRC})

if(APPLE)
  find_library(COCOA_LIBRARY Cocoa)
  target_link_libraries(capsule ${COCOA_LIBRARY})
endif()

add_executable(main ${MAIN_SRC})
target_link_libraries(main ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY} ${GLEW_LIBRARY} ${OPENGL_LIBRARY})

if(WIN32)
set(CAPSULERUN_SRC
    capsule-run.cpp)
add_executable(capsule-run ${CAPSULERUN_SRC})
endif()

if(WIN32)
  target_link_libraries(capsule ${NKT_HOOK_LIBRARY})
  target_link_libraries(capsule-run ${NKT_HOOK_LIBRARY})
endif()
