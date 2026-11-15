cmake_minimum_required(VERSION 3.16)
project(FluentLinkerPro VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

add_subdirectory(3rd/EUI-NEO)

add_executable(fluent_linker
    src/main.cpp
    src/app_actions.h
    src/linker_types.h
    src/win_linker.h
    src/ui_theme.h
    src/pages/create_page.h
    src/pages/inspect_page.h
    src/pages/history_page.h
    src/pages/guide_page.h
)

target_include_directories(fluent_linker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(WIN32)
    set(FLUENT_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
    if(EXISTS "${FLUENT_ICON_SOURCE}")
        file(TO_CMAKE_PATH "${FLUENT_ICON_SOURCE}" FLUENT_ICON_RC_PATH)
        set(FLUENT_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/fluent_linker_icon.rc")
        file(WRITE "${FLUENT_ICON_RC}" "1 ICON \"${FLUENT_ICON_RC_PATH}\"\n")
        set_property(GLOBAL PROPERTY EUI_NEO_APP_ICON_RESOURCE "${FLUENT_ICON_RC}")
    endif()
    target_compile_definitions(fluent_linker PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
    target_link_libraries(fluent_linker PRIVATE
        shell32
        ole32
        uuid
        advapi32
        user32
    )
endif()

eui_neo_configure_app(fluent_linker)
eui_neo_copy_app_assets(fluent_linker "${CMAKE_CURRENT_SOURCE_DIR}/assets")
