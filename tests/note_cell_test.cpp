/**
 *
 *  @file note_cell_test.cpp
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

#include <vix/note/core/NoteCell.hpp>
#include <vix/note/core/NoteResult.hpp>

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

int main()
{
  {
    vix::note::NoteCell cell;

    assert(cell.id().empty());
    assert(cell.kind() == vix::note::NoteCellKind::Unknown);
    assert(cell.source().empty());
    assert(cell.title().empty());
    assert(cell.execution_count() == 0);
    assert(cell.outputs().empty());
    assert(cell.empty());
    assert(!cell.executable());
    assert(!cell.has_outputs());
  }

  {
    vix::note::NoteCell cell(
        vix::note::NoteCellKind::Markdown,
        "# Title");

    assert(cell.kind() == vix::note::NoteCellKind::Markdown);
    assert(cell.source() == "# Title");
    assert(!cell.empty());
    assert(!cell.executable());
  }

  {
    vix::note::NoteCell cell(
        "cell-1",
        vix::note::NoteCellKind::Cpp,
        "int main() { return 0; }");

    assert(cell.id() == "cell-1");
    assert(cell.kind() == vix::note::NoteCellKind::Cpp);
    assert(cell.source() == "int main() { return 0; }");
    assert(cell.executable());
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::markdown("Some explanation");

    assert(cell.kind() == vix::note::NoteCellKind::Markdown);
    assert(cell.source() == "Some explanation");
    assert(!cell.executable());
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::reply("x = 1 + 2");

    assert(cell.kind() == vix::note::NoteCellKind::Reply);
    assert(cell.source() == "x = 1 + 2");
    assert(cell.executable());
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::cpp("#include <iostream>");

    assert(cell.kind() == vix::note::NoteCellKind::Cpp);
    assert(cell.source() == "#include <iostream>");
    assert(cell.executable());
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::html("<p>Hello</p>");

    assert(cell.kind() == vix::note::NoteCellKind::Html);
    assert(cell.source() == "<p>Hello</p>");
    assert(!cell.executable());
  }

  {
    vix::note::NoteCell cell;

    cell.set_id("cell-42");
    cell.set_kind(vix::note::NoteCellKind::Reply);
    cell.set_source("println(\"hello\")");
    cell.set_title("Intro cell");

    assert(cell.id() == "cell-42");
    assert(cell.kind() == vix::note::NoteCellKind::Reply);
    assert(cell.source() == "println(\"hello\")");
    assert(cell.title() == "Intro cell");
    assert(cell.executable());
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::cpp("int main() { return 0; }");

    cell.set_execution_count(3);
    assert(cell.execution_count() == 3);

    cell.set_execution_count(-10);
    assert(cell.execution_count() == 0);

    cell.mark_executed(7);
    assert(cell.execution_count() == 7);

    cell.reset_execution();
    assert(cell.execution_count() == 0);
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::reply("println(\"hello\")");

    cell
        .add_output(vix::note::NoteOutput::stdout_text("hello\n"))
        .add_output(vix::note::NoteOutput::error("runtime error"));

    assert(cell.has_outputs());
    assert(cell.outputs().size() == 2);

    assert(cell.outputs()[0].kind == vix::note::NoteOutputKind::Stdout);
    assert(cell.outputs()[0].content == "hello\n");

    assert(cell.outputs()[1].kind == vix::note::NoteOutputKind::Error);
    assert(cell.outputs()[1].content == "runtime error");

    cell.clear_outputs();

    assert(!cell.has_outputs());
    assert(cell.outputs().empty());
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::cpp("int main() { return 0; }");

    std::vector<vix::note::NoteOutput> outputs;
    outputs.push_back(vix::note::NoteOutput::stdout_text("ok"));
    outputs.push_back(vix::note::NoteOutput::stderr_text("warn"));

    cell.set_outputs(outputs);

    assert(cell.has_outputs());
    assert(cell.outputs().size() == 2);
    assert(cell.outputs()[0].content == "ok");
    assert(cell.outputs()[1].content == "warn");
  }

  {
    assert(vix::note::to_string(vix::note::NoteCellKind::Unknown) == "unknown");
    assert(vix::note::to_string(vix::note::NoteCellKind::Markdown) == "markdown");
    assert(vix::note::to_string(vix::note::NoteCellKind::Reply) == "reply");
    assert(vix::note::to_string(vix::note::NoteCellKind::Cpp) == "cpp");
    assert(vix::note::to_string(vix::note::NoteCellKind::Html) == "html");
  }

  {
    assert(vix::note::note_cell_kind_from_string("markdown") == vix::note::NoteCellKind::Markdown);
    assert(vix::note::note_cell_kind_from_string("md") == vix::note::NoteCellKind::Markdown);

    assert(vix::note::note_cell_kind_from_string("reply") == vix::note::NoteCellKind::Reply);
    assert(vix::note::note_cell_kind_from_string("repl") == vix::note::NoteCellKind::Reply);

    assert(vix::note::note_cell_kind_from_string("cpp") == vix::note::NoteCellKind::Cpp);
    assert(vix::note::note_cell_kind_from_string("c++") == vix::note::NoteCellKind::Cpp);

    assert(vix::note::note_cell_kind_from_string("html") == vix::note::NoteCellKind::Html);
    assert(vix::note::note_cell_kind_from_string("unknown-kind") == vix::note::NoteCellKind::Unknown);
  }

  {
    assert(!vix::note::is_executable(vix::note::NoteCellKind::Unknown));
    assert(!vix::note::is_executable(vix::note::NoteCellKind::Markdown));
    assert(vix::note::is_executable(vix::note::NoteCellKind::Reply));
    assert(vix::note::is_executable(vix::note::NoteCellKind::Cpp));
    assert(!vix::note::is_executable(vix::note::NoteCellKind::Html));
  }

  return 0;
}
