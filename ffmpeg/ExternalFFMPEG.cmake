
INCLUDE(ExternalProject)

set(FFMPEG_ARCHES i386 x86_64)

set(_LINK_LIBRARY_SUFFIX .so)
set(_LINK_LIBRARY_PREFIX lib)

if(WIN32)
  set(_LINK_LIBRARY_PREFIX )
  set(_LINK_LIBRARY_SUFFIX .dll)
endif()

if(APPLE)
  set(_LINK_LIBRARY_SUFFIX .dylib)
endif()

foreach(FFMPEG_ARCH IN LISTS FFMPEG_ARCHES)
  set(ffmpeg_source_${FFMPEG_ARCH} "${CMAKE_CURRENT_BINARY_DIR}/FFMPEG_${FFMPEG_ARCH}")
  set(ffmpeg_install "${CMAKE_CURRENT_BINARY_DIR}/FFMPEG-install_${FFMPEG_ARCH}")
  set(ffmpeg_install_${FFMPEG_ARCH} ${ffmpeg_install})
  configure_file(${CMAKE_CURRENT_LIST_DIR}/ffmpeg_configure_step.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_configure_step_${FFMPEG_ARCH}.cmake
      @ONLY)

  message(STATUS "install dir: ${ffmpeg_install_${FFMPEG_ARCH}}")

  ExternalProject_Add(FFMPEG_${FFMPEG_ARCH}
    DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-${FFMPEG_ARCH}
    SOURCE_DIR ${ffmpeg_source}
    INSTALL_DIR ${ffmpeg_install_${FFMPEG_ARCH}}
    BUILD_IN_SOURCE 1
    URL ${FFMPEG_URL}/${FFMPEG_BZ2}
    URL_HASH SHA1=${FFMPEG_SHA1}
    PATCH_COMMAND ""
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_configure_step_${FFMPEG_ARCH}.cmake
    )

  foreach(lib in avcodec avformat avutil swscale)
    get_filename_component(installdir ${ffmpeg_install_${FFMPEG_ARCH}} REALPATH)
    set(
      FFMPEG_${lib}_LIBRARY_${FFMPEG_ARCH}
      ${installdir}/lib/${_LINK_LIBRARY_PREFIX}${lib}${_LINK_LIBRARY_SUFFIX}
    )
  endforeach(lib)
endforeach(FFMPEG_ARCH)

add_custom_target(ffmpeg DEPENDS FFMPEG_i386 FFMPEG_x86_64)

# TODO: lipo
set(FFMPEG_INCLUDE_DIR ${ffmpeg_install_x86_64}/include)

message(STATUS "ffmpeg/avcodec library: ${FFMPEG_avcodec_LIBRARY_x86_64}")
message(STATUS "ffmpeg/avutil library: ${FFMPEG_avutil_LIBRARY_x86_64}")

