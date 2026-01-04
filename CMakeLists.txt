cmake_minimum_required(VERSION 3.12)
project(vraid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2)

file(GLOB_RECURSE UI CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/layout/*.ui")

add_executable(${PROJECT_NAME}
    main.cpp
    ${UI}

)

set_property(TARGET ${PROJECT_NAME}
  PROPERTY AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/resources/layout")

file(GLOB_RECURSE MySubdirectories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} */CMakeLists.txt)
foreach(subdir ${MySubdirectories})
    get_filename_component(subdir_path ${subdir} DIRECTORY)
    if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${subdir_path})
        ADD_SUBDIRECTORY(${subdir_path})
        TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/${subdir_path}/include)
        MESSAGE(STATUS "PATH-DIRECTORY ${subdir_path} = ${CMAKE_SOURCE_DIR}/${subdir_path}/include")
    endif ()
endforeach ()

set(CollectedLibraries "")
foreach(subdir ${MySubdirectories})
    get_filename_component(subdir_path ${subdir} DIRECTORY)
    get_directory_property(subdir_libs DIRECTORY ${subdir_path} DEFINITION LIB_NAME)
    if (subdir_libs)
        MESSAGE(STATUS "Library found in ${subdir_libs}")
        list(APPEND CollectedLibraries ${subdir_libs})
    endif ()
endforeach ()

file(COPY resources/ DESTINATION ${CMAKE_BINARY_DIR}/resources/)
#add_compile_definitions(RESOURCE_PATH="${CMAKE_BINARY_DIR}/resources/")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_BINARY_DIR}/resources)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CollectedLibraries}
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Qml Qt5::Quick Qt5::QuickControls2
)

