#  @file CMakeLists.txt
#  @author Gaspard Kirira
#
#  Copyright 2026, Gaspard Kirira. All rights reserved.
#  https://github.com/vixcpp/note
#  Use of this source code is governed by a MIT license
#  that can be found in the LICENSE file.
#
# ====================================================================
# Vix Note Examples
# ====================================================================

cmake_minimum_required(VERSION 3.20)

set(VIX_NOTE_EXAMPLE_FILES
  hello.vixnote
  learning_cpp.vixnote
  vix_ui_note.vixnote
)

set(VIX_NOTE_EXAMPLES_OUTPUT_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/vixnote"
)

file(MAKE_DIRECTORY "${VIX_NOTE_EXAMPLES_OUTPUT_DIR}")

set(VIX_NOTE_EXAMPLE_OUTPUT_FILES "")

foreach(example_file IN LISTS VIX_NOTE_EXAMPLE_FILES)
  set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${example_file}")
  set(output_file "${VIX_NOTE_EXAMPLES_OUTPUT_DIR}/${example_file}")

  if(NOT EXISTS "${source_file}")
    message(FATAL_ERROR "[note/examples] missing example file: ${source_file}")
  endif()

  configure_file(
    "${source_file}"
    "${output_file}"
    COPYONLY
  )

  list(APPEND VIX_NOTE_EXAMPLE_OUTPUT_FILES "${output_file}")
endforeach()

add_custom_target(vix_note_examples
  ALL
  DEPENDS ${VIX_NOTE_EXAMPLE_OUTPUT_FILES}
  SOURCES ${VIX_NOTE_EXAMPLE_FILES}
)

message(STATUS "[note/examples] examples copied to: ${VIX_NOTE_EXAMPLES_OUTPUT_DIR}")

if(VIX_NOTE_INSTALL AND NOT (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD))
  install(
    FILES ${VIX_NOTE_EXAMPLE_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vix/note/examples
  )
endif()
