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
        .add_error("error text");

    assert(result.has_outputs());
    assert(result.outputs().size() == 5);

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
  }

  return 0;
}
