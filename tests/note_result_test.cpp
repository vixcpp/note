/**
 *
 *  @file note_result_test.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.
 *  All rights reserved.
 *  https://github.com/vixcpp/note
 *
 *  Use of this source code is governed by a MIT license
 *  that can be found in the LICENSE file.
 *
 *  Vix Note
 *
 */

#include <vix/note/core/NoteResult.hpp>

#include <cassert>
#include <string>
#include <string_view>

int main()
{
  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::text("hello");

    assert(output.kind == vix::note::NoteOutputKind::Text);
    assert(output.content == "hello");
    assert(!output.empty());
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::stdout_text("stdout message");

    assert(output.kind == vix::note::NoteOutputKind::Stdout);
    assert(output.content == "stdout message");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::stderr_text("stderr message");

    assert(output.kind == vix::note::NoteOutputKind::Stderr);
    assert(output.content == "stderr message");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::html("<strong>Hello</strong>");

    assert(output.kind == vix::note::NoteOutputKind::Html);
    assert(output.content == "<strong>Hello</strong>");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::error("runtime error");

    assert(output.kind == vix::note::NoteOutputKind::Error);
    assert(output.content == "runtime error");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::compiler_error("expected ';'");

    assert(output.kind == vix::note::NoteOutputKind::CompilerError);
    assert(output.content == "expected ';'");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::runtime_error("segmentation fault");

    assert(output.kind == vix::note::NoteOutputKind::RuntimeError);
    assert(output.content == "segmentation fault");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::debug("duration_ms=12");

    assert(output.kind == vix::note::NoteOutputKind::Debug);
    assert(output.content == "duration_ms=12");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::hint("Check if you forgot a semicolon.");

    assert(output.kind == vix::note::NoteOutputKind::Hint);
    assert(output.content == "Check if you forgot a semicolon.");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::raw_log("raw compiler output");

    assert(output.kind == vix::note::NoteOutputKind::RawLog);
    assert(output.content == "raw compiler output");
  }

  {
    vix::note::NoteOutput output =
        vix::note::NoteOutput::text("");

    assert(output.empty());
  }

  {
    vix::note::NoteResult result =
        vix::note::NoteResult::success("done");

    assert(result.status() == vix::note::NoteResultStatus::Success);
    assert(result.ok());
    assert(!result.failed());
    assert(!result.was_skipped());
    assert(result.exit_code() == 0);
    assert(result.message() == "done");
    assert(!result.has_outputs());
  }

  {
    vix::note::NoteResult result =
        vix::note::NoteResult::failure("failed", 42);

    assert(result.status() == vix::note::NoteResultStatus::Failure);
    assert(!result.ok());
    assert(result.failed());
    assert(!result.was_skipped());
    assert(result.exit_code() == 42);
    assert(result.message() == "failed");
  }

  {
    vix::note::NoteResult result =
        vix::note::NoteResult::skipped("not needed");

    assert(result.status() == vix::note::NoteResultStatus::Skipped);
    assert(!result.ok());
    assert(!result.failed());
    assert(result.was_skipped());
    assert(result.exit_code() == 0);
    assert(result.message() == "not needed");
  }

  {
    vix::note::NoteResult result;

    result
        .add_text("plain text")
        .add_stdout("stdout text")
        .add_stderr("stderr text")
        .add_html("<p>html</p>")
        .add_error("error text")
        .add_compiler_error("compiler diagnostic")
        .add_runtime_error("runtime diagnostic")
        .add_debug("duration_ms=15")
        .add_hint("Try checking the include path.")
        .add_raw_log("raw log text");

    assert(result.has_outputs());
    assert(result.outputs().size() == 10);

    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Text);
    assert(result.outputs()[0].content == "plain text");

    assert(result.outputs()[1].kind == vix::note::NoteOutputKind::Stdout);
    assert(result.outputs()[1].content == "stdout text");

    assert(result.outputs()[2].kind == vix::note::NoteOutputKind::Stderr);
    assert(result.outputs()[2].content == "stderr text");

    assert(result.outputs()[3].kind == vix::note::NoteOutputKind::Html);
    assert(result.outputs()[3].content == "<p>html</p>");

    assert(result.outputs()[4].kind == vix::note::NoteOutputKind::Error);
    assert(result.outputs()[4].content == "error text");

    assert(result.outputs()[5].kind == vix::note::NoteOutputKind::CompilerError);
    assert(result.outputs()[5].content == "compiler diagnostic");

    assert(result.outputs()[6].kind == vix::note::NoteOutputKind::RuntimeError);
    assert(result.outputs()[6].content == "runtime diagnostic");

    assert(result.outputs()[7].kind == vix::note::NoteOutputKind::Debug);
    assert(result.outputs()[7].content == "duration_ms=15");

    assert(result.outputs()[8].kind == vix::note::NoteOutputKind::Hint);
    assert(result.outputs()[8].content == "Try checking the include path.");

    assert(result.outputs()[9].kind == vix::note::NoteOutputKind::RawLog);
    assert(result.outputs()[9].content == "raw log text");
  }

  {
    vix::note::NoteResult result =
        vix::note::NoteResult::success();

    result.add_output(
        vix::note::NoteOutput::stdout_text("manual output"));

    assert(result.outputs().size() == 1);
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Stdout);
    assert(result.outputs()[0].content == "manual output");
  }

  {
    vix::note::NoteResult result =
        vix::note::NoteResult::failure("compile failed", 1)
            .add_compiler_error("main.cpp:3:10: error: expected ';'")
            .add_hint("A C++ statement usually ends with a semicolon.")
            .add_raw_log("main.cpp:3:10: error: expected ';'\n");

    assert(result.failed());
    assert(result.exit_code() == 1);
    assert(result.message() == "compile failed");
    assert(result.has_outputs());
    assert(result.outputs().size() == 3);

    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::CompilerError);
    assert(result.outputs()[1].kind == vix::note::NoteOutputKind::Hint);
    assert(result.outputs()[2].kind == vix::note::NoteOutputKind::RawLog);
  }

  {
    vix::note::NoteResult result =
        vix::note::NoteResult::failure("runtime failed", 139)
            .add_runtime_error("segmentation fault")
            .add_debug("exit_code=139")
            .add_debug("duration_ms=3");

    assert(result.failed());
    assert(result.exit_code() == 139);
    assert(result.has_outputs());
    assert(result.outputs().size() == 3);

    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::RuntimeError);
    assert(result.outputs()[1].kind == vix::note::NoteOutputKind::Debug);
    assert(result.outputs()[2].kind == vix::note::NoteOutputKind::Debug);
  }

  {
    assert(vix::note::to_string(vix::note::NoteResultStatus::Success) == "success");
    assert(vix::note::to_string(vix::note::NoteResultStatus::Failure) == "failure");
    assert(vix::note::to_string(vix::note::NoteResultStatus::Skipped) == "skipped");
  }

  {
    assert(vix::note::to_string(vix::note::NoteOutputKind::Text) == "text");
    assert(vix::note::to_string(vix::note::NoteOutputKind::Stdout) == "stdout");
    assert(vix::note::to_string(vix::note::NoteOutputKind::Stderr) == "stderr");
    assert(vix::note::to_string(vix::note::NoteOutputKind::Html) == "html");
    assert(vix::note::to_string(vix::note::NoteOutputKind::Error) == "error");
    assert(vix::note::to_string(vix::note::NoteOutputKind::CompilerError) == "compiler_error");
    assert(vix::note::to_string(vix::note::NoteOutputKind::RuntimeError) == "runtime_error");
    assert(vix::note::to_string(vix::note::NoteOutputKind::Debug) == "debug");
    assert(vix::note::to_string(vix::note::NoteOutputKind::Hint) == "hint");
    assert(vix::note::to_string(vix::note::NoteOutputKind::RawLog) == "raw_log");
  }

  return 0;
}
