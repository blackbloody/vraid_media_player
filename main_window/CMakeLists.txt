cmake_minimum_required(VERSION 3.12)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_CONSTANT_MACROS")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -lpthread")

set(LIB_NAME ApplicationWindow)

file(GLOB_RECURSE APPWIN_SRC CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_library(${LIB_NAME} STATIC ${APPWIN_SRC})
# Be explicit: make sure AUTOMOC runs on this target too
set_target_properties(${LIB_NAME} PROPERTIES AUTOMOC ON)

set_property(TARGET ApplicationWindow PROPERTY AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/resources/layout")
set_target_properties(Gl_Spec PROPERTIES AUTOMOC ON AUTORCC ON AUTOUIC ON)
set_target_properties(${LIB_NAME} PROPERTIES AUTORCC ON)

target_include_directories(${LIB_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${LIB_NAME} PUBLIC
  Gl_Spec MainFrameMedia MediaReader Librosa Network_Usage
  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Qml Qt5::Quick Qt5::QuickControls2
)
