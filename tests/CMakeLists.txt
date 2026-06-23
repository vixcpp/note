#  @file CMakeLists.txt
#  @author Gaspard Kirira
#
#  Copyright 2026, Gaspard Kirira. All rights reserved.
#  https://github.com/vixcpp/note
#  Use of this source code is governed by a MIT license
#  that can be found in the LICENSE file.
#
# ====================================================================
# Vix Note Tests
# ====================================================================

cmake_minimum_required(VERSION 3.20)

if(NOT TARGET vix::note)
  message(FATAL_ERROR "[note/tests] missing required target: vix::note")
endif()

function(vix_note_add_test target source)
  add_executable(${target}
    ${source}
  )

  target_compile_features(${target}
    PRIVATE
      cxx_std_20
  )

  target_link_libraries(${target}
    PRIVATE
      vix::note
  )

  if(VIX_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(${target}
      PRIVATE
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    )

    target_link_options(${target}
      PRIVATE
        -fsanitize=address,undefined
    )
  endif()

  add_test(
    NAME note.${target}
    COMMAND ${target}
  )
endfunction()

vix_note_add_test(note_error_test note_error_test.cpp)
vix_note_add_test(note_result_test note_result_test.cpp)
vix_note_add_test(note_cell_test note_cell_test.cpp)
vix_note_add_test(note_document_test note_document_test.cpp)

vix_note_add_test(note_parser_test note_parser_test.cpp)

vix_note_add_test(note_store_test note_store_test.cpp)

vix_note_add_test(cpp_cell_runner_test cpp_cell_runner_test.cpp)
vix_note_add_test(note_session_test note_session_test.cpp)
vix_note_add_test(note_kernel_test note_kernel_test.cpp)

vix_note_add_test(note_assets_test note_assets_test.cpp)
vix_note_add_test(note_routes_test note_routes_test.cpp)
vix_note_add_test(note_server_test note_server_test.cpp)

vix_note_add_test(html_exporter_test html_exporter_test.cpp)
