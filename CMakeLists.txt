cmake_minimum_required(VERSION 3.20)

project(orc-plugin-skeleton
    VERSION 1.0.12
    DESCRIPTION "Decode-Orc external plugin skeleton"
    LANGUAGES CXX
)

set(ORC_PLUGIN_VERSION "${PROJECT_VERSION}" CACHE STRING
    "Plugin version string embedded in StagePluginDescriptor")

# On tagged CI builds, derive plugin version from tag names like v1.2.3
# unless the caller explicitly overrides ORC_PLUGIN_VERSION.
if(ORC_PLUGIN_VERSION STREQUAL PROJECT_VERSION)
    if(DEFINED ENV{GITHUB_REF_TYPE} AND "$ENV{GITHUB_REF_TYPE}" STREQUAL "tag" AND DEFINED ENV{GITHUB_REF_NAME})
        set(_orc_ref_name "$ENV{GITHUB_REF_NAME}")
        if(_orc_ref_name MATCHES "^v(.+)$")
            set(ORC_PLUGIN_VERSION "${CMAKE_MATCH_1}")
            set(ORC_PLUGIN_VERSION "${ORC_PLUGIN_VERSION}" CACHE STRING
                "Plugin version string embedded in StagePluginDescriptor" FORCE)
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build skeleton plugin tests" ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DecodeOrcPluginSDKHelpers.cmake")

if(DEFINED ORC_INTREE_SDK_DIR)
    # Build the SDK targets (headers + the orc-sdk-support static library)
    # from a decode-orc checkout. No host library is built or linked.
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DecodeOrcInTreeSDK.cmake")
else()
    find_package(decode-orc-plugin-sdk REQUIRED)
endif()

set(SKELETON_STAGE_NAME "skeleton_passthrough")
set(SKELETON_PLUGIN_TARGET "orc-stage-plugin-skeleton-passthrough")

orc_add_stage_plugin(
    ${SKELETON_PLUGIN_TARGET}
    OUTPUT_NAME ${SKELETON_PLUGIN_TARGET}
    PLUGIN_VERSION ${ORC_PLUGIN_VERSION}
    SOURCES
        src/plugin.cpp
        src/skeleton_passthrough_stage.cpp
)

target_include_directories(${SKELETON_PLUGIN_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# The view binary: the Inspector window the stage's Skeleton Inspector tool
# opens in orc-gui. A separate library because it links Qt and the stage
# binary must not. Its file name must start with "orc-view_" (no "lib"
# prefix): that is how hosts tell it from a stage binary without opening it.
# Build it against the oldest Qt 6 minor you support; orc-gui refuses a view
# built for a newer Qt minor than its own.
option(BUILD_VIEW "Build the Inspector view binary (needs Qt 6 Widgets)" ON)
set(SKELETON_VIEW_TARGET "orc-view_skeleton")
if(BUILD_VIEW)
    find_package(Qt6 COMPONENTS Widgets)
    if(NOT Qt6Widgets_FOUND)
        message(STATUS "Qt 6 Widgets not found: building without the view binary")
        set(BUILD_VIEW OFF)
    endif()
endif()

if(TARGET orc::plugin-sdk)
    set(_orc_sdk_target orc::plugin-sdk)
else()
    set(_orc_sdk_target orc-plugin-sdk)
endif()

if(BUILD_VIEW)
    add_library(${SKELETON_VIEW_TARGET} SHARED view/skeleton_view.cpp)
    # Only Core, Gui and Widgets: the modules every orc-gui ships.
    target_link_libraries(${SKELETON_VIEW_TARGET} PRIVATE ${_orc_sdk_target} Qt6::Widgets)
    target_compile_definitions(${SKELETON_VIEW_TARGET} PRIVATE
        ORC_VIEW_PLUGIN_VERSION="${ORC_PLUGIN_VERSION}")
    get_target_property(_skeleton_plugin_dir ${SKELETON_PLUGIN_TARGET} LIBRARY_OUTPUT_DIRECTORY)
    set_target_properties(${SKELETON_VIEW_TARGET} PROPERTIES PREFIX "")
    if(_skeleton_plugin_dir)
        # Beside the stage binary, so one ORC_STAGE_PLUGIN_PATHS folder
        # gives orc-gui both halves.
        set_target_properties(${SKELETON_VIEW_TARGET} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${_skeleton_plugin_dir}"
            RUNTIME_OUTPUT_DIRECTORY "${_skeleton_plugin_dir}")
    endif()
endif()

# Build-info emitter for the release manifest (host ABI 12 and later).
# Compiled with the same toolchain and SDK headers as the plugin, it prints
# the descriptor's ABI version and toolchain tag so scripts/package_local.sh
# can declare them in orc-plugin-manifest.yaml without guessing.
add_executable(orc-plugin-build-info tools/plugin_build_info.cpp)
target_link_libraries(orc-plugin-build-info PRIVATE ${_orc_sdk_target})
if(BUILD_VIEW)
    # The view's manifest entry declares the Qt it was built against.
    target_compile_definitions(orc-plugin-build-info PRIVATE
        ORC_VIEW_QT_VERSION="${Qt6Widgets_VERSION_MAJOR}.${Qt6Widgets_VERSION_MINOR}")
endif()
target_include_directories(orc-plugin-build-info PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(orc-plugin-build-info PRIVATE
    ORC_STAGE_PLUGIN_VERSION="${ORC_PLUGIN_VERSION}")
set_target_properties(orc-plugin-build-info PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
