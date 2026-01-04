cmake_minimum_required(VERSION 3.16)
project(MediaReader VERSION 0.1.0 LANGUAGES CXX)

# ---- C++ setup ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_NAME MediaReader)

# ---- Sources ----------------------------------------------------------------
# (GLOB is fine for small libs; add CONFIGURE_DEPENDS so CMake regenerates on file changes)
file(GLOB WAV_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/audio_reader/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_reader/*.cpp"
)

file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/media_obj/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/audio_reader/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_reader/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/callback/*.h"
)

add_library(${LIB_NAME} SHARED ${WAV_SRC} ${HDRS})

target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_reader/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_reader/include
    ${CMAKE_CURRENT_SOURCE_DIR}/media_obj
    ${CMAKE_CURRENT_SOURCE_DIR}/callback
)

# If you really need this macro, define it per-target (remove if not needed)
target_compile_definitions(${LIB_NAME} PRIVATE
    __STDC_CONSTANT_MACROS
    __STDC_LIMIT_MACROS
)

# ---- Dependencies ------------------------------------------------------------
find_package(PkgConfig QUIET)

# libsndfile
if(PKG_CONFIG_FOUND)
  pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
  target_link_libraries(${LIB_NAME} PRIVATE PkgConfig::SNDFILE)
else()
  find_path(LIBSNDFILE_INCLUDE_DIR sndfile.h)
  find_library(LIBSNDFILE_LIBRARY NAMES sndfile libsndfile-1)
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(LibSndFile DEFAULT_MSG
    LIBSNDFILE_INCLUDE_DIR LIBSNDFILE_LIBRARY)
  target_include_directories(${LIB_NAME} PRIVATE ${LIBSNDFILE_INCLUDE_DIR})
  target_link_libraries(${LIB_NAME} PRIVATE ${LIBSNDFILE_LIBRARY})
endif()

# libsamplerate
if(PKG_CONFIG_FOUND)
  pkg_check_modules(SAMPLERATE REQUIRED IMPORTED_TARGET samplerate)
  target_link_libraries(${LIB_NAME} PRIVATE PkgConfig::SAMPLERATE)
else()
  find_path(LIBSAMPLERATE_INCLUDE_DIR samplerate.h)
  find_library(LIBSAMPLERATE_LIBRARY NAMES samplerate)
  target_include_directories(${LIB_NAME} PRIVATE ${LIBSAMPLERATE_INCLUDE_DIR})
  target_link_libraries(${LIB_NAME} PRIVATE ${LIBSAMPLERATE_LIBRARY})
endif()

# ALSA (optional)
find_package(ALSA QUIET) # provides ALSA::ALSA on modern CMake
if(ALSA_FOUND)
  if(TARGET ALSA::ALSA)
    target_link_libraries(${LIB_NAME} PRIVATE ALSA::ALSA)
  else()
    target_include_directories(${LIB_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(${LIB_NAME} PRIVATE ${ALSA_LIBRARIES})
  endif()
endif()

pkg_check_modules(SIGC REQUIRED sigc++-2.0)
message(STATUS "SIGC_INCLUDE_DIRS = ${SIGC_INCLUDE_DIRS}")
message(STATUS "SIGC_LIBRARIES    = ${SIGC_LIBRARIES}")
message(STATUS "SIGC_CFLAGS_OTHER = ${SIGC_CFLAGS_OTHER}")
target_include_directories(${LIB_NAME} PUBLIC ${SIGC_INCLUDE_DIRS})
target_link_libraries(${LIB_NAME} PUBLIC ${SIGC_LIBRARIES})
target_compile_options(${LIB_NAME} PUBLIC ${SIGC_CFLAGS_OTHER})

# ---------- FFmpeg (libav*) for video / metadata -----------------------------
# On Ubuntu/Mint the pkg-config names are: libavformat, libavcodec, libavutil,
# libswscale, libswresample, libavdevice.
if(PKG_CONFIG_FOUND)
    pkg_check_modules(AVFORMAT   REQUIRED libavformat)
    pkg_check_modules(AVCODEC    REQUIRED libavcodec)
    pkg_check_modules(AVUTIL     REQUIRED libavutil)
    pkg_check_modules(SWSCALE    REQUIRED libswscale)
    pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

    pkg_check_modules(AVDEVICE QUIET libavdevice)

    target_include_directories(${LIB_NAME} PRIVATE
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVCODEC_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
        ${SWRESAMPLE_INCLUDE_DIRS}
        ${AVDEVICE_INCLUDE_DIRS}
    )

    target_link_libraries(${LIB_NAME} PRIVATE
        ${AVFORMAT_LIBRARIES}
        ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWSCALE_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
        ${AVDEVICE_LIBRARIES}
    )

    target_compile_options(${LIB_NAME} PRIVATE
        ${AVFORMAT_CFLAGS_OTHER}
        ${AVCODEC_CFLAGS_OTHER}
        ${AVUTIL_CFLAGS_OTHER}
        ${SWSCALE_CFLAGS_OTHER}
        ${SWRESAMPLE_CFLAGS_OTHER}
        ${AVDEVICE_CFLAGS_OTHER}
    )

endif()


# Threads (only if you actually use std::thread/pthreads anywhere)
find_package(Threads QUIET)
if(Threads_FOUND)
  target_link_libraries(${LIB_NAME} PRIVATE Threads::Threads)
endif()

# Optional: position-independent code (on by default for SHARED)
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Nice-to-have alias
add_library(${LIB_NAME}::${LIB_NAME} ALIAS ${LIB_NAME})

#######
####### sudo apt install libsndfile1-dev libsamplerate0-dev libasound2-dev pkg-config

# sudo apt install \
#   libsndfile1-dev \
#   libsamplerate0-dev \
#   libasound2-dev \
#   libsigc++-2.0-dev \
#   libavformat-dev \
#   libavcodec-dev \
#   libavutil-dev \
#   libswscale-dev \
#   libswresample-dev \
#   libavdevice-dev \
#   pkg-config
