# **********************************************************************
#
#  App (test)
#
# **********************************************************************

cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 OLD) # not set MSVC default args

project(app LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(USE_LOCAL_MUSE_FRAMEWORK ON)

if (NOT USE_LOCAL_MUSE_FRAMEWORK)
    set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/_deps)
    include(FetchContent)
    FetchContent_Declare(
      muse_framework
      GIT_REPOSITORY git@github.com:musescore/framework_tmp.git
      GIT_TAG        HEAD
    )

    FetchContent_GetProperties(muse_framework)
    if(NOT muse_framework_POPULATED)
      FetchContent_Populate(muse_framework)
    endif()
else()

    set(muse_framework_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
    set(muse_framework_BINARY_DIR ${PROJECT_BINARY_DIR}/muse_framework)

endif()

message("muse_framework_SOURCE_DIR: ${muse_framework_SOURCE_DIR}")

set(MUSE_FRAMEWORK_PATH ${muse_framework_SOURCE_DIR})
set(MUSE_FRAMEWORK_SRC_PATH ${muse_framework_SOURCE_DIR}/framework)

set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}
    ${MUSE_FRAMEWORK_PATH}/buildscripts/cmake
    ${CMAKE_MODULE_PATH}
    )

###########################################
# Setup option and build settings
###########################################
include(GetPaths)

set(APP_BUILD_CONFIGURATION "app" CACHE STRING "Build configuration")
# Possible values:
# - app             - for desktop app
# - app-portable    - for desktop portable app (Windows build for PortableApps.com)
# - utest           - for unit tests (for CI)

set(APP_BUILD_MODE "dev" CACHE STRING "Build mode")
# Possible values:
# - dev     - for development/nightly builds
# - testing - for testing versions (alpha, beta, RC)
# - release - for stable release builds

set(APP_REVISION "" CACHE STRING "Build revision")

include(${MUSE_FRAMEWORK_SRC_PATH}/cmake/MuseDeclareOptions.cmake)

set(MUSE_MODULE_AUTOBOT OFF)
set(MUSE_MODULE_DRAW OFF)
set(MUSE_MODULE_UPDATE OFF)

# Modules (alphabetical order please)
option(MU_BUILD_APPSHELL_MODULE "Build appshell module" OFF)

# === Setup ===

# === Pack ===
option(MU_RUN_LRELEASE "Generate .qm files" ON)


# === Compile ===
option(MU_COMPILE_BUILD_MACOS_APPLE_SILICON "Build for Apple Silicon architecture. Only applicable on Macs with Apple Silicon, and requires suitable Qt version." OFF)
option(MU_COMPILE_INSTALL_QTQML_FILES "Whether to bundle qml files along with the installation (relevant on MacOS only)" ON)
option(MU_COMPILE_USE_PCH "Use precompiled headers." ON)
option(MU_COMPILE_USE_UNITY "Use unity build." ON)
option(MU_COMPILE_USE_COMPILER_CACHE "Try to use compiler cache: tries ccache, sccache, buildcache in that order. Use COMPILER_CACHE_PROGRAM to specify a specific compiler cache program." ON)

# === Debug ===
option(MU_ENABLE_STRING_DEBUG_HACK "Enable string debug hack (only clang)" ON)


###########################################
# Setup Configure
###########################################
add_definitions(-DQT_SUPPORT)
add_definitions(-DKORS_LOGGER_QT_SUPPORT)

set(MUSE_APP_INSTALL_NAME "\"App\"")
set(MUSE_APP_INSTALL_PREFIX "\"\"")
set(MUSE_APP_INSTALL_SUFFIX "\"\"")
set(MUSE_APP_VERSION "4.0.0")

include(${MUSE_FRAMEWORK_SRC_PATH}/cmake/MuseSetupConfiguration.cmake)

###########################################
# Setup compiler and build environment
###########################################

include(SetupBuildEnvironment)
include(GetPlatformInfo)
if (MU_COMPILE_USE_COMPILER_CACHE)
    include(SetupCompilerCache)
endif(MU_COMPILE_USE_COMPILER_CACHE)


###########################################
# Setup external dependencies
###########################################
set(QT_MIN_VERSION "6.2.4")
include(FindQt6)

###########################################
# Add source tree
###########################################

add_subdirectory(${muse_framework_SOURCE_DIR}/framework ${muse_framework_BINARY_DIR})
add_subdirectory(src)

###########################################
# Setup Packaging
###########################################
