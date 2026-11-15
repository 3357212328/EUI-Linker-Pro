cmake_minimum_required(VERSION 3.21)
project(FluentLinkerPro VERSION 3.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

# Configure Slint for crisp Windows Fluent UI (Skia DirectWrite renderer + DLL bundled next to exe)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build Slint as shared runtime DLL beside fluent_linker.exe" FORCE)
set(SLINT_STYLE "fluent" CACHE STRING "Slint widget style" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_TESTING OFF CACHE BOOL "" FORCE)
set(DEFAULT_SLINT_EMBED_RESOURCES "embed-files" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG v1.18.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

set(APP_SOURCES
    src/main.cpp
    src/app_controller.h
    src/linker_types.h
    src/win_linker.h
)

if(WIN32)
    set(FLUENT_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
    if(EXISTS "${FLUENT_ICON_SOURCE}")
        file(TO_CMAKE_PATH "${FLUENT_ICON_SOURCE}" FLUENT_ICON_RC_PATH)
        set(FLUENT_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/fluent_linker_icon.rc")
        file(WRITE "${FLUENT_ICON_RC}" "1 ICON \"${FLUENT_ICON_RC_PATH}\"\n")
        list(APPEND APP_SOURCES "${FLUENT_ICON_RC}")
    endif()
endif()

add_executable(fluent_linker ${APP_SOURCES})

slint_target_sources(fluent_linker
    src/ui/app.slint
)

target_include_directories(fluent_linker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(fluent_linker PRIVATE
    Slint::Slint
)

if(WIN32)
    target_compile_definitions(fluent_linker PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        SLINT_PREFER_SKIA_BACKEND=1
    )
    target_link_libraries(fluent_linker PRIVATE
        shell32
        ole32
        uuid
        advapi32
        user32
        dwmapi
    )
    if(MSVC)
        target_compile_options(fluent_linker PRIVATE /utf-8)
        target_link_options(fluent_linker PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()

    # Copy any runtime DLLs if a shared library configuration is chosen externally
    add_custom_command(
        TARGET fluent_linker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:fluent_linker>
            $<TARGET_FILE_DIR:fluent_linker>
        COMMAND_EXPAND_LISTS
    )
endif()
