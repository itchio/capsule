cmake_minimum_required(VERSION 2.8)

project(polyhook)

set(polyhook_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/PolyHook)
set(polyhook_INCLUDE_DIR ${polyhook_SOURCE_DIR})

include_directories(${polyhook_INCLUDE_DIR})

if(${CMAKE_GENERATOR} MATCHES "Win64$")
    set(${capstone_LIBRARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/source/Capstone/msvc/x64/Release)
else()
    set(${capstone_LIBRARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/source/Capstone/msvc/x86/Release)
endif()
link_directories(${capstone_LIBRARY_DIR})

set(polyhook_SOURCES
    ${polyhook_SOURCE_DIR}/PolyHook.cpp)

add_library(polyhook ${polyhook_SOURCES})
