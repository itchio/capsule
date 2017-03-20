cmake_minimum_required(VERSION 2.8)

if(APPLE)
  # MACOSX_RPATH is now enabled by default
  cmake_policy(SET CMP0042 NEW)
endif()

set(capsulerun_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(libcapsule_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcapsule/src)
set(capsulerun_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(capsulerun_INCLUDE_DIRS SYSTEM)

set(libcapsule_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libcapsule/include)

include_directories(
  ${shoom_INCLUDE_DIR}
  ${capsulerun_INCLUDE_DIRS}
  ${capsulerun_INCLUDE_DIR}
  ${libcapsule_INCLUDE_DIR}
  ${microprofile_INCLUDE_DIR}
)

set(capsulerun_SRC
  ${capsulerun_SOURCE_DIR}/shared/main.cpp
  ${capsulerun_SOURCE_DIR}/shared/encoder.cpp
  ${capsulerun_SOURCE_DIR}/shared/argparse.c
  ${capsulerun_SOURCE_DIR}/shared/MainLoop.cpp
  ${capsulerun_SOURCE_DIR}/shared/VideoReceiver.cpp
  ${capsulerun_SOURCE_DIR}/shared/Session.cpp
  ${capsulerun_SOURCE_DIR}/shared/Connection.cpp
  ${capsulerun_SOURCE_DIR}/shared/FPSCounter.cpp
  ${capsulerun_SOURCE_DIR}/shared/OverlayRenderer.cpp
  ${microprofile_SOURCE_DIR}/microprofile.cpp
)

if(WIN32)
  list(APPEND capsulerun_SRC
    ${capsulerun_SOURCE_DIR}/windows/strings.cpp
    ${capsulerun_SOURCE_DIR}/windows/WasapiReceiver.cpp
    ${capsulerun_SOURCE_DIR}/windows/quote.cpp
    ${capsulerun_SOURCE_DIR}/windows/hotkey.cpp
    ${capsulerun_SOURCE_DIR}/windows/main.cpp
  )
endif()

if(APPLE)
  list(APPEND capsulerun_SRC
    ${capsulerun_SOURCE_DIR}/shared/env.cpp
    ${capsulerun_SOURCE_DIR}/macos/DDHotKeyCenter.m
    ${capsulerun_SOURCE_DIR}/macos/DDHotKeyUtilities.m
    ${capsulerun_SOURCE_DIR}/macos/hotkey.mm
    ${capsulerun_SOURCE_DIR}/macos/main.cpp
  )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  list(APPEND capsulerun_SRC
    ${capsulerun_SOURCE_DIR}/shared/env.cpp
    ${capsulerun_SOURCE_DIR}/linux/PulseReceiver.cpp
    ${capsulerun_SOURCE_DIR}/linux/hotkey.cpp
    ${capsulerun_SOURCE_DIR}/linux/main.cpp
  )
  add_definitions(-D__STDC_CONSTANT_MACROS)
endif()

add_executable(capsulerun ${capsulerun_SRC})

target_link_libraries(capsulerun shoom)

if(WIN32)
  add_dependencies(capsulerun capsule_deps)
  target_link_libraries(capsulerun ${DEVIARE_INPROC_LIBRARY})

  foreach(NEEDED_LIB avutil.lib avcodec.lib avformat.lib swscale.lib swresample.lib)
    target_link_libraries(capsulerun ${FFMPEG_LIBRARY_DIR}/${NEEDED_LIB})
  endforeach(NEEDED_LIB)

  foreach(NEEDED_DLL ${AVUTIL_DLL} ${AVCODEC_DLL} ${AVFORMAT_DLL} ${SWSCALE_DLL} ${SWRESAMPLE_DLL})
    install(
      FILES "${FFMPEG_LIBRARY_DIR}/${NEEDED_DLL}"
      DESTINATION "${CMAKE_BINARY_DIR}/dist"
    )
  endforeach(NEEDED_DLL)

  # needed for microprofile
  target_link_libraries(capsulerun ws2_32.lib)
endif()

if(APPLE)
  include(FindPkgConfig)
  foreach(NEEDED_LIB libavutil libavcodec libavformat libswscale libswresample x264)
    PKG_CHECK_MODULES(${NEEDED_LIB}_PKG ${NEEDED_LIB})
    include_directories(${${NEEDED_LIB}_PKG_INCLUDE_DIRS})
    target_link_libraries(capsulerun ${${NEEDED_LIB}_PKG_LDFLAGS} ${${NEEDED_LIB}_PKG_LIBRARIES})
  endforeach(NEEDED_LIB)

  find_library(COCOA_LIBRARY Cocoa)
  target_link_libraries(capsulerun ${COCOA_LIBRARY})
  find_library(CARBON_LIBRARY Carbon)
  target_link_libraries(capsulerun ${CARBON_LIBRARY})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  include(FindPkgConfig)
  set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/Dev/ffmpeg-prefix/lib/pkgconfig:${CAPSULE_FFMPEG_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

  foreach(NEEDED_LIB libavutil libavcodec libavformat libswscale libswresample x264 libpulse libpulse-simple)
    PKG_CHECK_MODULES(${NEEDED_LIB}_PKG ${NEEDED_LIB})
    include_directories(${${NEEDED_LIB}_PKG_INCLUDE_DIRS})
    target_link_libraries(capsulerun ${${NEEDED_LIB}_PKG_LDFLAGS} ${${NEEDED_LIB}_PKG_LIBRARIES})
  endforeach(NEEDED_LIB)

  # needed on old Debian for reasons? (probably because of libpulse-simple?)
  target_link_libraries(capsulerun -lpthread)
  target_link_libraries(capsulerun -lX11)
endif()

install(TARGETS capsulerun
  DESTINATION "${CMAKE_BINARY_DIR}/dist"
)
