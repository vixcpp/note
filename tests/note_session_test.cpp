/**
 *
 *  @file note_session_test.cpp
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
#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/runtime/NoteSession.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>

int main()
{
  {
    vix::note::NoteSession session;

    assert(session.empty());
    assert(session.cell_count() == 0);
    assert(!session.has_cell(0));
    assert(!session.can_execute_cell(0));
    assert(session.records().empty());
    assert(!session.has_records());

    assert(session.options().clearOutputsBeforeRun);
    assert(!session.options().stopOnFirstFailure);
  }

  {
    vix::note::NoteDocument doc("Learning C++");
    doc.add_markdown("# Learning C++");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteSession session(doc);

    assert(!session.empty());
    assert(session.cell_count() == 2);
    assert(session.document().title() == "Learning C++");

    assert(session.has_cell(0));
    assert(session.has_cell(1));
    assert(!session.has_cell(2));

    assert(!session.can_execute_cell(0));
    assert(session.can_execute_cell(1));
  }

  {
    vix::note::NoteDocument doc;
    doc.add_reply("println(\"hello\")");

    vix::note::NoteSessionOptions options;
    options.clearOutputsBeforeRun = false;
    options.stopOnFirstFailure = true;

    vix::note::NoteSession session(doc, options);

    assert(!session.options().clearOutputsBeforeRun);
    assert(session.options().stopOnFirstFailure);
  }

  {
    vix::note::NoteSession session;

    vix::note::NoteSessionOptions options;
    options.clearOutputsBeforeRun = false;
    options.stopOnFirstFailure = true;

    session.set_options(options);

    assert(!session.options().clearOutputsBeforeRun);
    assert(session.options().stopOnFirstFailure);
  }

  {
    vix::note::NoteSession session;

    vix::note::NoteDocument doc("New Document");
    doc.add_cpp("int main() { return 0; }");

    session.set_document(doc);

    assert(!session.empty());
    assert(session.cell_count() == 1);
    assert(session.document().title() == "New Document");
    assert(session.can_execute_cell(0));
    assert(!session.has_records());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_cell(
        vix::note::NoteCell(
            "intro",
            vix::note::NoteCellKind::Markdown,
            "# Intro"));

    doc.add_cell(
        vix::note::NoteCell(
            "run",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteSession session(doc);

    vix::note::NoteCell *cell =
        session.cell_at(1);

    assert(cell != nullptr);
    assert(cell->id() == "run");
    assert(cell->kind() == vix::note::NoteCellKind::Reply);

    assert(session.cell_at(99) == nullptr);

    const vix::note::NoteSession &constSession = session;
    const vix::note::NoteCell *constCell =
        constSession.cell_at(0);

    assert(constCell != nullptr);
    assert(constCell->id() == "intro");
    assert(constSession.cell_at(99) == nullptr);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "a",
            vix::note::NoteCellKind::Markdown,
            "A"));

    doc.add_cell(
        vix::note::NoteCell(
            "b",
            vix::note::NoteCellKind::Cpp,
            "B"));

    vix::note::NoteSession session(doc);

    vix::note::NoteCell *cell =
        session.find_cell("b");

    assert(cell != nullptr);
    assert(cell->id() == "b");
    assert(cell->kind() == vix::note::NoteCellKind::Cpp);

    assert(session.find_cell("missing") == nullptr);

    const vix::note::NoteSession &constSession = session;
    const vix::note::NoteCell *constCell =
        constSession.find_cell("a");

    assert(constCell != nullptr);
    assert(constCell->id() == "a");
    assert(constSession.find_cell("missing") == nullptr);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "first",
            vix::note::NoteCellKind::Markdown,
            "A"));

    doc.add_cell(
        vix::note::NoteCell(
            "second",
            vix::note::NoteCellKind::Reply,
            "B"));

    vix::note::NoteSession session(doc);

    std::optional<std::size_t> first =
        session.cell_index("first");

    std::optional<std::size_t> second =
        session.cell_index("second");

    std::optional<std::size_t> missing =
        session.cell_index("missing");

    assert(first.has_value());
    assert(second.has_value());
    assert(!missing.has_value());

    assert(*first == 0);
    assert(*second == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "run",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteSession session(doc);

    vix::note::NoteResult cellResult =
        vix::note::NoteResult::success("ok")
            .add_stdout("hello\n");

    vix::note::NoteResult applied =
        session.apply_result(0, cellResult);

    assert(applied.ok());
    assert(applied.message() == "cell result applied");

    const vix::note::NoteCell *cell =
        session.cell_at(0);

    assert(cell != nullptr);
    assert(cell->execution_count() == 1);
    assert(cell->has_outputs());
    assert(cell->outputs().size() == 1);
    assert(cell->outputs()[0].kind == vix::note::NoteOutputKind::Stdout);
    assert(cell->outputs()[0].content == "hello\n");

    assert(session.document().execution_count() == 1);
    assert(session.has_records());
    assert(session.records().size() == 1);

    assert(session.records()[0].cellIndex == 0);
    assert(session.records()[0].cellId == "run");
    assert(session.records()[0].executionCount == 1);
    assert(session.records()[0].result.ok());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp",
            vix::note::NoteCellKind::Cpp,
            "int main() { return 0; }"));

    vix::note::NoteSession session(doc);

    vix::note::NoteResult first =
        vix::note::NoteResult::success("first")
            .add_stdout("first\n");

    vix::note::NoteResult second =
        vix::note::NoteResult::success("second")
            .add_stdout("second\n");

    assert(session.apply_result("cpp", first).ok());
    assert(session.apply_result("cpp", second).ok());

    const vix::note::NoteCell *cell =
        session.find_cell("cpp");

    assert(cell != nullptr);
    assert(cell->execution_count() == 2);
    assert(cell->outputs().size() == 1);
    assert(cell->outputs()[0].content == "second\n");

    assert(session.document().execution_count() == 2);
    assert(session.records().size() == 2);
    assert(session.records()[0].executionCount == 1);
    assert(session.records()[1].executionCount == 2);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp",
            vix::note::NoteCellKind::Cpp,
            "int main() { return 0; }"));

    vix::note::NoteSessionOptions options;
    options.clearOutputsBeforeRun = false;

    vix::note::NoteSession session(doc, options);

    vix::note::NoteResult first =
        vix::note::NoteResult::success("first")
            .add_stdout("first\n");

    vix::note::NoteResult second =
        vix::note::NoteResult::success("second")
            .add_stdout("second\n");

    assert(session.apply_result(0, first).ok());
    assert(session.apply_result(0, second).ok());

    const vix::note::NoteCell *cell =
        session.cell_at(0);

    assert(cell != nullptr);
    assert(cell->outputs().size() == 2);
    assert(cell->outputs()[0].content == "first\n");
    assert(cell->outputs()[1].content == "second\n");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "bad",
            vix::note::NoteCellKind::Reply,
            "println(\"bad\")"));

    vix::note::NoteSession session(doc);

    vix::note::NoteResult failed =
        vix::note::NoteResult::failure("runtime failed", 9)
            .add_error("runtime failed\n");

    vix::note::NoteResult applied =
        session.apply_result(0, failed);

    assert(applied.failed());
    assert(applied.exit_code() == 9);
    assert(applied.message() == "cell result applied with failure");

    const vix::note::NoteCell *cell =
        session.cell_at(0);

    assert(cell != nullptr);
    assert(cell->execution_count() == 1);
    assert(cell->has_outputs());
    assert(cell->outputs()[0].kind == vix::note::NoteOutputKind::Error);
    assert(cell->outputs()[0].content == "runtime failed\n");

    assert(session.records().size() == 1);
    assert(session.records()[0].result.failed());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "skip",
            vix::note::NoteCellKind::Reply,
            "println(\"skip\")"));

    vix::note::NoteSession session(doc);

    vix::note::NoteResult skipped =
        vix::note::NoteResult::skipped("not needed");

    vix::note::NoteResult applied =
        session.apply_result(0, skipped);

    assert(applied.was_skipped());
    assert(applied.message() == "cell result applied as skipped");

    const vix::note::NoteCell *cell =
        session.cell_at(0);

    assert(cell != nullptr);
    assert(cell->execution_count() == 1);

    assert(session.records().size() == 1);
    assert(session.records()[0].result.was_skipped());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_markdown("# Not executable");

    vix::note::NoteSession session(doc);

    vix::note::NoteResult applied =
        session.apply_result(
            0,
            vix::note::NoteResult::success("ok"));

    assert(applied.failed());
    assert(applied.message() == "cell is not executable");
    assert(applied.has_outputs());
    assert(applied.outputs()[0].kind == vix::note::NoteOutputKind::Error);

    assert(!session.has_records());
    assert(session.document().execution_count() == 0);
  }

  {
    vix::note::NoteSession session;

    vix::note::NoteResult applied =
        session.apply_result(
            99,
            vix::note::NoteResult::success("ok"));

    assert(applied.failed());
    assert(applied.message() == "cell index out of range");
    assert(applied.has_outputs());
    assert(applied.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteDocument doc;
    doc.add_reply("println(\"hello\")");

    vix::note::NoteSession session(doc);

    vix::note::NoteResult applied =
        session.apply_result(
            "missing",
            vix::note::NoteResult::success("ok"));

    assert(applied.failed());
    assert(applied.message() == "cell not found: missing");
    assert(applied.has_outputs());
    assert(applied.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_reply("println(\"hello\")")
        .add_output(vix::note::NoteOutput::stdout_text("hello\n"));

    doc.add_cpp("int main() { return 0; }")
        .add_output(vix::note::NoteOutput::stdout_text("ok\n"));

    vix::note::NoteSession session(doc);

    assert(session.document().cells()[0].has_outputs());
    assert(session.document().cells()[1].has_outputs());

    session.clear_outputs();

    assert(!session.document().cells()[0].has_outputs());
    assert(!session.document().cells()[1].has_outputs());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_reply("println(\"hello\")");
    doc.add_cpp("int main() { return 0; }");

    vix::note::NoteSession session(doc);

    assert(session.apply_result(0, vix::note::NoteResult::success("ok")).ok());
    assert(session.apply_result(1, vix::note::NoteResult::success("ok")).ok());

    assert(session.document().execution_count() == 2);
    assert(session.document().cells()[0].execution_count() == 1);
    assert(session.document().cells()[1].execution_count() == 2);

    session.reset_execution();

    assert(session.document().execution_count() == 0);
    assert(session.document().cells()[0].execution_count() == 0);
    assert(session.document().cells()[1].execution_count() == 0);

    assert(session.has_records());

    session.clear_records();

    assert(!session.has_records());
    assert(session.records().empty());
  }

  return 0;
}
