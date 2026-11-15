cmake_minimum_required(VERSION 3.22)
project(fluent_linker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Auto-detect local Qt installation if CMAKE_PREFIX_PATH was not passed explicitly.
if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6")
    list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.8.3/msvc2022_64")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

set(QML_SOURCES
    src/qml/Main.qml
    src/qml/AppTheme.qml
    src/qml/VecIcon.qml
    src/qml/StatusBadge.qml
    src/qml/FluentButton.qml
    src/qml/FluentCard.qml
    src/qml/FluentInput.qml
    src/qml/AnimatedSegmentBar.qml
    src/qml/FluentScrollView.qml
    src/qml/CreatePage.qml
    src/qml/InspectPage.qml
    src/qml/HistoryPage.qml
    src/qml/GuidePage.qml
)

foreach(qml_file ${QML_SOURCES})
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

qt_add_executable(fluent_linker WIN32
    src/main.cpp
    src/qt_backend.cpp
    src/qt_backend.h
    src/win_linker.h
    src/linker_types.h
    src/app.rc
)

qt_add_qml_module(fluent_linker
    URI FluentLinker
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
    RESOURCES
        assets/icon.png
)

target_include_directories(fluent_linker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(fluent_linker PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    dwmapi
    shell32
    ole32
    uuid
    advapi32
    user32
)

target_compile_definitions(fluent_linker PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(fluent_linker PRIVATE /utf-8 /permissive- /W3)
endif()

# Deploy Qt runtime DLLs and QML plugins beside fluent_linker.exe after build.
get_target_property(_qt_qmake_exe Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qt_qmake_exe}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" REQUIRED)

add_custom_command(TARGET fluent_linker POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
        "$<IF:$<CONFIG:Debug>,--debug,--release>"
        --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/src/qml"
        --no-translations
        "$<TARGET_FILE:fluent_linker>"
    COMMENT "Running windeployqt for fluent_linker..."
    VERBATIM
)
