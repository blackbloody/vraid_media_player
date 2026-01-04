cmake_minimum_required(VERSION 3.16)
project(Network_Usage VERSION 0.1.0 LANGUAGES CXX)

# ---- C++ setup ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_NAME Network_Usage)

# ---- Sources ----------------------------------------------------------------
file(GLOB API_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/api_usage/src/*.cpp")
file(GLOB_RECURSE API_HDRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/api_usage/include/*.h")

file(GLOB WS_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ws_usage/src/*.cpp")
file(GLOB_RECURSE WS_HDRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ws_usage/include/*.h")

add_library(${LIB_NAME} SHARED ${API_SRC} ${WS_SRC} ${API_HDRS} ${WS_HDRS})
target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/api_usage/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ws_usage/include
)

# If you really need this macro, define it per-target (remove if not needed)
target_compile_definitions(${LIB_NAME} PRIVATE __STDC_CONSTANT_MACROS)

# ---- End Sources ------------------------------------------------------------
# ---- Dependencies ------------------------------------------------------------
find_package(PkgConfig QUIET)

set(STB_DIR "${CMAKE_SOURCE_DIR}/libs/stb")
target_include_directories(${LIB_NAME}
  PUBLIC
    ${STB_DIR}
)

find_package(CURL REQUIRED)            # provides target CURL::libcurl
target_link_libraries(${LIB_NAME} PRIVATE CURL::libcurl)

# ---- libsoup: 2.4 ----
pkg_check_modules(SOUP2 REQUIRED libsoup-2.4)
target_include_directories(${LIB_NAME} PUBLIC ${SOUP2_INCLUDE_DIRS})
target_link_libraries(${LIB_NAME} PUBLIC ${SOUP2_LIBRARIES})

# ---- OpenCV (via pkg-config) ----
if(PKG_CONFIG_FOUND AND OpenCV_FOUND)
  # Try opencv4 first, then opencv (older distros)
  pkg_check_modules(OPENCV REQUIRED)
  target_include_directories(${LIB_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(${LIB_NAME} PUBLIC ${OpenCV_LIBS})
endif()

pkg_check_modules(SIGC REQUIRED sigc++-2.0)
message(STATUS "SIGC_INCLUDE_DIRS = ${SIGC_INCLUDE_DIRS}")
message(STATUS "SIGC_LIBRARIES    = ${SIGC_LIBRARIES}")
message(STATUS "SIGC_CFLAGS_OTHER = ${SIGC_CFLAGS_OTHER}")

target_include_directories(${LIB_NAME} PUBLIC ${SIGC_INCLUDE_DIRS})
target_link_libraries(${LIB_NAME} PUBLIC ${SIGC_LIBRARIES})
target_compile_options(${LIB_NAME} PUBLIC ${SIGC_CFLAGS_OTHER})

# Threads (only if you actually use std::thread/pthreads anywhere)
find_package(Threads QUIET)
if(Threads_FOUND)
  target_link_libraries(${LIB_NAME} PRIVATE Threads::Threads)
endif()

# ---- END ----
# Optional: position-independent code (on by default for SHARED)
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Nice-to-have alias
add_library(${LIB_NAME}::${LIB_NAME} ALIAS ${LIB_NAME})
