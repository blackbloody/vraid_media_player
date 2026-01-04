cmake_minimum_required(VERSION 3.16)
project(Librosa VERSION 0.1.0 LANGUAGES CXX)

# ---- C++ setup ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_NAME Librosa)

# ---- Sources ----------------------------------------------------------------
# (GLOB is fine for small libs; add CONFIGURE_DEPENDS so CMake regenerates on file changes)
file(GLOB SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

file(GLOB_RECURSE HDRS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

add_library(${LIB_NAME} SHARED ${SRC} ${HDRS})
target_include_directories(${LIB_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# If you really need this macro, define it per-target (remove if not needed)
target_compile_definitions(${LIB_NAME} PRIVATE __STDC_CONSTANT_MACROS)

# ---- Dependencies ------------------------------------------------------------
find_package(PkgConfig REQUIRED)
find_package(PkgConfig QUIET)
FIND_PACKAGE(OpenCV REQUIRED)

set(STB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/stb")
target_include_directories(${LIB_NAME}
  PUBLIC
    ${STB_DIR}
)

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

# eigen3 -- apt install libeigen3-dev
if(PKG_CONFIG_FOUND)
    pkg_check_modules(EIGEN REQUIRED eigen3)
    target_include_directories(${LIB_NAME} PUBLIC ${EIGEN_INCLUDE_DIRS})
    # target_link_libraries(${LIB_NAME} PUBLIC ${EIGEN_LIBRARY_DIRS})
endif()

# ---- OpenCV (via pkg-config) ----
target_include_directories(${LIB_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${LIB_NAME} PUBLIC ${OpenCV_LIBS})

# Threads (only if you actually use std::thread/pthreads anywhere)
find_package(Threads QUIET)
if(Threads_FOUND)
  target_link_libraries(${LIB_NAME} PRIVATE Threads::Threads)
endif()

# Optional: position-independent code (on by default for SHARED)
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Nice-to-have alias
add_library(${LIB_NAME}::${LIB_NAME} ALIAS ${LIB_NAME})
