#  @file CMakeLists.txt
#  @author Gaspard Kirira
#
#  Copyright 2026, Gaspard Kirira. All rights reserved.
#  https://github.com/vixcpp/note
#  Use of this source code is governed by a MIT license
#  that can be found in the LICENSE file.
#
# ====================================================================
# Vix Note
# ====================================================================

cmake_minimum_required(VERSION 3.20)

set(VIX_NOTE_MODULE_VERSION 0.2.0)

# ============================================================================
# Re-entry guard
# Prevent the note module from being configured/exported twice
# ============================================================================

get_property(_VIX_NOTE_MODULE_ALREADY_INCLUDED GLOBAL PROPERTY VIX_NOTE_MODULE_ALREADY_INCLUDED)

if(_VIX_NOTE_MODULE_ALREADY_INCLUDED)
  return()
endif()

set_property(GLOBAL PROPERTY VIX_NOTE_MODULE_ALREADY_INCLUDED TRUE)

# ============================================================================
# Standalone vs Umbrella
# ============================================================================

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(vix_note
    VERSION ${VIX_NOTE_MODULE_VERSION}
    DESCRIPTION "Visual executable notes for learning C++ and Vix.cpp faster"
    LANGUAGES CXX
  )
endif()

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

# ============================================================================
# Options
# ============================================================================

option(VIX_NOTE_BUILD_TESTS "Build Vix Note tests" ON)
option(VIX_NOTE_BUILD_EXAMPLES "Build Vix Note examples" ON)
option(VIX_NOTE_ENABLE_LTO "Enable LTO in Release builds" OFF)
option(VIX_NOTE_INSTALL "Enable standalone install/package rules" ON)

if(DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD)
  set(VIX_NOTE_BUILD_TESTS OFF CACHE BOOL "Build Vix Note tests" FORCE)
  set(VIX_NOTE_BUILD_EXAMPLES OFF CACHE BOOL "Build Vix Note examples" FORCE)
  set(VIX_NOTE_INSTALL OFF CACHE BOOL "Enable standalone install/package rules" FORCE)
endif()

# ============================================================================
# C++ standard
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Sources
# ============================================================================

set(VIX_NOTE_SOURCES
  src/core/NoteError.cpp
  src/core/NoteResult.cpp
  src/core/NoteCell.cpp
  src/core/NoteDocument.cpp

  src/parser/NoteParser.cpp

  src/project/ProjectContext.cpp
  src/project/ProjectDetector.cpp

  src/storage/NoteStore.cpp

  src/runtime/CppCellRunner.cpp
  src/runtime/ReplyCellRunner.cpp
  src/runtime/NoteSession.cpp
  src/runtime/NoteKernel.cpp

  src/web/NoteAssets.cpp
  src/web/NoteRoutes.cpp
  src/web/NoteServer.cpp

  src/export/HtmlExporter.cpp
)

# ============================================================================
# Library
# ============================================================================

add_library(vix_note STATIC ${VIX_NOTE_SOURCES})

if(NOT TARGET vix::note)
  add_library(vix::note ALIAS vix_note)
endif()

target_include_directories(vix_note
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(vix_note PUBLIC cxx_std_20)

set_target_properties(vix_note PROPERTIES
  OUTPUT_NAME vix_note
  VERSION ${VIX_NOTE_MODULE_VERSION}
  SOVERSION 0
  EXPORT_NAME note
)

# ============================================================================
# Warnings
# ============================================================================

function(vix_note_add_flag_if_supported target flag)
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" flag_var "${flag}")
  set(test_var "HAVE_VIX_NOTE_${flag_var}")

  check_cxx_compiler_flag("${flag}" ${test_var})

  if(${test_var})
    target_compile_options(${target} PRIVATE "${flag}")
  endif()
endfunction()

if(MSVC)
  vix_note_add_flag_if_supported(vix_note /W4)
  vix_note_add_flag_if_supported(vix_note /permissive-)
else()
  vix_note_add_flag_if_supported(vix_note -Wall)
  vix_note_add_flag_if_supported(vix_note -Wextra)
  vix_note_add_flag_if_supported(vix_note -Wshadow)
  vix_note_add_flag_if_supported(vix_note -Wconversion)
  vix_note_add_flag_if_supported(vix_note -Wformat=2)
  vix_note_add_flag_if_supported(vix_note -Wpedantic)
endif()

# ============================================================================
# Dependency resolution helpers
# ============================================================================

function(vix_note_append_if_exists out_var path_value)
  if(EXISTS "${path_value}")
    set(${out_var} "${${out_var}};${path_value}" PARENT_SCOPE)
  endif()
endfunction()

# ============================================================================
# Reply runtime implementation
# - Reply is used internally by Note's ReplyCellRunner.
# - It is not part of the Vix umbrella export.
# - Therefore we embed Reply sources into vix_note instead of exporting a
#   vix::reply dependency through VixTargets.
# ============================================================================

get_filename_component(_VIX_NOTE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(_VIX_REPLY_DIR "${_VIX_NOTE_MODULES_DIR}/reply")

if(EXISTS "${_VIX_REPLY_DIR}/CMakeLists.txt")
  message(STATUS "[note] Embedding local reply runtime from ${_VIX_REPLY_DIR}")

  file(GLOB_RECURSE VIX_NOTE_REPLY_SOURCES
    CONFIGURE_DEPENDS
    "${_VIX_REPLY_DIR}/src/*.cpp"
  )

  target_sources(vix_note PRIVATE ${VIX_NOTE_REPLY_SOURCES})

  target_include_directories(vix_note
    PRIVATE
      "${_VIX_REPLY_DIR}/include"
  )

  target_compile_definitions(vix_note
    PRIVATE
      VIX_NOTE_HAS_REPLY=1
  )
else()
  message(FATAL_ERROR
    "Missing dependency for vix_note.\n"
    "Expected local sibling module in ../reply because Note uses ReplyCellRunner."
  )
endif()

# ============================================================================
# Version string
# ============================================================================

set(_VIX_NOTE_VERSION "dev")

if(DEFINED VIX_NOTE_VERSION AND NOT "${VIX_NOTE_VERSION}" STREQUAL "")
  set(_VIX_NOTE_VERSION "${VIX_NOTE_VERSION}")

elseif(DEFINED ENV{VIX_NOTE_VERSION} AND NOT "$ENV{VIX_NOTE_VERSION}" STREQUAL "")
  set(_VIX_NOTE_VERSION "$ENV{VIX_NOTE_VERSION}")

elseif(DEFINED VIX_UMBRELLA_VERSION AND NOT "${VIX_UMBRELLA_VERSION}" STREQUAL "")
  set(_VIX_NOTE_VERSION "${VIX_UMBRELLA_VERSION}")

elseif(DEFINED VIX_NOTE_MODULE_VERSION AND NOT "${VIX_NOTE_MODULE_VERSION}" STREQUAL "")
  set(_VIX_NOTE_VERSION "v${VIX_NOTE_MODULE_VERSION}")

elseif(DEFINED PROJECT_VERSION AND NOT "${PROJECT_VERSION}" STREQUAL "")
  set(_VIX_NOTE_VERSION "v${PROJECT_VERSION}")

else()
  find_package(Git QUIET)

  if(Git_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --always
      WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      OUTPUT_VARIABLE _GIT_DESCRIBE
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    if(_GIT_DESCRIBE)
      set(_VIX_NOTE_VERSION "${_GIT_DESCRIBE}")
    endif()
  endif()
endif()

message(STATUS "Vix Note version: ${_VIX_NOTE_VERSION}")

target_compile_definitions(vix_note
  PRIVATE
    VIX_NOTE_VERSION="${_VIX_NOTE_VERSION}"
)

# ============================================================================
# LTO
# ============================================================================

if(VIX_NOTE_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)

  check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)

  if(_ipo_ok)
    set_property(TARGET vix_note PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "[note] IPO/LTO not supported: ${_ipo_msg}")
  endif()
endif()

# ============================================================================
# Tests
# ============================================================================

if(VIX_NOTE_BUILD_TESTS)
  enable_testing()

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
  else()
    message(STATUS "[note] tests/ missing CMakeLists.txt -> skipping")
  endif()
endif()

# ============================================================================
# Examples
# ============================================================================

if(VIX_NOTE_BUILD_EXAMPLES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
  else()
    message(STATUS "[note] examples/ missing CMakeLists.txt -> skipping")
  endif()
endif()

# ============================================================================
# Install for Vix umbrella export
# ============================================================================

if(DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD)
  install(
    TARGETS vix_note
    EXPORT VixTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(
      DIRECTORY assets/
      DESTINATION ${CMAKE_INSTALL_DATADIR}/vix/note/assets
    )
  endif()
endif()

# ============================================================================
# Standalone install/package
# ============================================================================

if(VIX_NOTE_INSTALL AND NOT (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD))
  include(CMakePackageConfigHelpers)

  install(
    TARGETS vix_note
    EXPORT vix_noteTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(
      DIRECTORY assets/
      DESTINATION ${CMAKE_INSTALL_DATADIR}/vix/note/assets
    )
  endif()

  install(
    EXPORT vix_noteTargets
    FILE vix_noteTargets.cmake
    NAMESPACE vix::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vix_note
  )

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/vix_noteConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/vix_noteConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vix_note
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/vix_noteConfigVersion.cmake
    VERSION ${VIX_NOTE_MODULE_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/vix_noteConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/vix_noteConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vix_note
  )
endif()

message(STATUS "Vix Note configured.")
