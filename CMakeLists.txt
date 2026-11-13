cmake_minimum_required(VERSION 3.20)

project(orc-plugin-skeleton
    VERSION 1.0.10
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

# Build-info emitter for the release manifest (host ABI 12 and later).
# Compiled with the same toolchain and SDK headers as the plugin, it prints
# the descriptor's ABI version and toolchain tag so scripts/package_local.sh
# can declare them in orc-plugin-manifest.yaml without guessing.
if(TARGET orc::plugin-sdk)
    set(_orc_build_info_sdk_target orc::plugin-sdk)
else()
    set(_orc_build_info_sdk_target orc-plugin-sdk)
endif()

add_executable(orc-plugin-build-info tools/plugin_build_info.cpp)
target_link_libraries(orc-plugin-build-info PRIVATE ${_orc_build_info_sdk_target})
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
