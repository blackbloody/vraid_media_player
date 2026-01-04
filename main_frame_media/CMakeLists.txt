cmake_minimum_required(VERSION 3.12)
project(MainFrameMedia LANGUAGES C CXX)

# Toolchain / language
SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(LIB_NAME MainFrameMedia)

find_package(OpenGL REQUIRED)
find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 UiTools)
find_package(Threads REQUIRED)

set(GLAD_SRC "${CMAKE_SOURCE_DIR}/libs/glad/src/glad.c")
set(GLAD_INC "${CMAKE_SOURCE_DIR}/libs/glad/include")

file(GLOB_RECURSE APPWIN_SRC CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

 add_library(${LIB_NAME} SHARED ${APPWIN_SRC} ${GLAD_SRC})
 # Be explicit: make sure AUTOMOC runs on this target too
 set_target_properties(Gl_Spec PROPERTIES AUTOMOC ON AUTORCC ON AUTOUIC ON)

 target_include_directories(${LIB_NAME} PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${GLAD_INC}
 )

target_link_libraries(${LIB_NAME} PUBLIC
    OpenGL::GL Librosa MediaReader Network_Usage
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Qml Qt5::Quick Qt5::QuickControls2 Qt5::UiTools
    Threads::Threads
    dl
    GL_ABSTRACT_TOOL
)

set_property(TARGET ${LIB_NAME}
  PROPERTY AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/resources/layout")
