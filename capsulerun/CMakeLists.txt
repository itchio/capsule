cmake_minimum_required(VERSION 2.8)

if(APPLE)
  # MACOSX_RPATH is now enabled by default
  cmake_policy(SET CMP0042 NEW)
endif()

set(capsulerun_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(capsulerun_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(capsulerun_INCLUDE_DIRS SYSTEM)

include_directories(${capsulerun_INCLUDE_DIRS} ${capsulerun_INCLUDE_DIR})

set(capsulerun_SRC
${capsulerun_SOURCE_DIR}/shared/main.cpp)

if(WIN32)
  list(APPEND capsulerun_SRC
    ${capsulerun_SOURCE_DIR}/windows/main.cpp
  )
endif()

if(APPLE)
  list(APPEND capsulerun_SRC
    ${capsulerun_SOURCE_DIR}/macos/main.cpp
  )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  list(APPEND capsulerun_SRC
    ${capsulerun_SOURCE_DIR}/linux/encoder.cpp
    ${capsulerun_SOURCE_DIR}/linux/main.cpp
  )
endif()

add_executable(capsulerun ${capsulerun_SRC})

if(WIN32)
  add_dependencies(capsulerun capsule_deps)
  target_link_libraries(capsulerun ${DEVIARE_INPROC_LIBRARY})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  include(FindPkgConfig)
  set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/Dev/ffmpeg-prefix/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  PKG_CHECK_MODULES(AVUTIL libavutil)
  PKG_CHECK_MODULES(AVCODEC libavcodec)
  PKG_CHECK_MODULES(AVFORMAT libavformat)
  PKG_CHECK_MODULES(SWSCALE libswscale)
  PKG_CHECK_MODULES(X264 x264)
  include_directories(${AVUTIL_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS} ${AVFORMAT_INCLUDE_DIRS})
  target_link_libraries(capsulerun PRIVATE ${AVUTIL_LDFLAGS} ${AVCODEC_LDFLAGS} ${X264_LDFLAGS} ${SWSCALE_LDFLAGS} ${AVFORMAT_LDFLAGS}) 
  target_link_libraries(capsulerun ${AVUTIL_LIBRARIES} ${AVCODEC_LIBRARIES} ${X264_LIBRARIES} ${SWSCALE_LIBRARIES} ${AVFORMAT_LIBRARIES})
  target_link_libraries(capsulerun -lpthread)
endif()
