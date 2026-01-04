cmake_minimum_required(VERSION 3.16)
project(GL_ABSTRACT_TOOL VERSION 0.1.0 LANGUAGES C CXX)

# ---- C++ setup ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_NAME GL_ABSTRACT_TOOL)

set(GLAD_SRC "${CMAKE_SOURCE_DIR}/libs/glad/src/glad.c")
set(GLAD_INC "${CMAKE_SOURCE_DIR}/libs/glad/include")

# file(GLOB_RECURSE APPWIN_SRC CONFIGURE_DEPENDS
#      "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
#      "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB APPWIN_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# add_library(${LIB_NAME} STATIC ${APPWIN_SRC} ${GLAD_SRC})
add_library(${LIB_NAME} SHARED ${APPWIN_SRC} ${GLAD_SRC})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${APPWIN_SRC})
# Be explicit: make sure AUTOMOC runs on this target too
# set_target_properties(${LIB_NAME} PROPERTIES AUTOMOC ON AUTORCC ON AUTOUIC ON)

set(STB_DIR "${CMAKE_SOURCE_DIR}/libs/stb")
set(GLM_DIR "${CMAKE_SOURCE_DIR}/libs/glm")
target_include_directories(${LIB_NAME}
  PUBLIC
    ${STB_DIR}
    ${GLM_DIR}
)

target_include_directories(${LIB_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${GLAD_INC}
)

# Threads (only if you actually use std::thread/pthreads anywhere)
find_package(Threads QUIET)
if(Threads_FOUND)
  target_link_libraries(${LIB_NAME} PRIVATE Threads::Threads)
endif()

file(COPY resources/ DESTINATION ${CMAKE_BINARY_DIR}/resources/)
TARGET_INCLUDE_DIRECTORIES(${LIB_NAME} PUBLIC ${CMAKE_BINARY_DIR}/resources)

# Optional: position-independent code (on by default for SHARED)
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Nice-to-have alias
add_library(${LIB_NAME}::${LIB_NAME} ALIAS ${LIB_NAME})
