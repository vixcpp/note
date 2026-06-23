/**
 *
 *  @file note_document_test.cpp
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

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>

int main()
{
  {
    vix::note::NoteDocument doc;

    assert(doc.id().empty());
    assert(doc.title().empty());
    assert(doc.path().empty());
    assert(doc.cells().empty());
    assert(doc.cell_count() == 0);
    assert(doc.execution_count() == 0);
    assert(doc.empty());
    assert(!doc.has_title());
    assert(!doc.has_executable_cells());
    assert(doc.executable_cell_count() == 0);
  }

  {
    vix::note::NoteDocument doc("Learning C++");

    assert(doc.title() == "Learning C++");
    assert(doc.has_title());
    assert(doc.empty());
  }

  {
    vix::note::NoteDocument doc = vix::note::NoteDocument::create("Pointers");

    assert(doc.title() == "Pointers");
    assert(doc.has_title());
    assert(doc.empty());
  }

  {
    vix::note::NoteDocument doc;

    doc.set_id("doc-1");
    doc.set_title("Variables");
    doc.set_path("examples/variables.vixnote");

    assert(doc.id() == "doc-1");
    assert(doc.title() == "Variables");
    assert(doc.path() == "examples/variables.vixnote");
  }

  {
    vix::note::NoteDocument doc;

    doc.set_execution_count(3);
    assert(doc.execution_count() == 3);

    doc.set_execution_count(-10);
    assert(doc.execution_count() == 0);

    assert(doc.next_execution_count() == 1);
    assert(doc.next_execution_count() == 2);
    assert(doc.execution_count() == 2);

    doc.reset_execution_count();
    assert(doc.execution_count() == 0);
  }

  {
    vix::note::NoteDocument doc;

    vix::note::NoteCell &cell =
        doc.add_cell(
            vix::note::NoteCell(
                "cell-1",
                vix::note::NoteCellKind::Markdown,
                "# Intro"));

    assert(doc.cell_count() == 1);
    assert(!doc.empty());
    assert(cell.id() == "cell-1");
    assert(cell.kind() == vix::note::NoteCellKind::Markdown);
    assert(cell.source() == "# Intro");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Lesson");
    doc.add_reply("x = 1 + 2");
    doc.add_cpp("int main() { return 0; }");
    doc.add_html("<p>Hello</p>");

    assert(doc.cell_count() == 4);
    assert(doc.cells()[0].kind() == vix::note::NoteCellKind::Markdown);
    assert(doc.cells()[1].kind() == vix::note::NoteCellKind::Reply);
    assert(doc.cells()[2].kind() == vix::note::NoteCellKind::Cpp);
    assert(doc.cells()[3].kind() == vix::note::NoteCellKind::Html);

    assert(doc.has_executable_cells());
    assert(doc.executable_cell_count() == 2);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("first");
    doc.add_markdown("third");

    const bool inserted =
        doc.insert_cell(
            1,
            vix::note::NoteCell::reply("second"));

    assert(inserted);
    assert(doc.cell_count() == 3);
    assert(doc.cells()[0].source() == "first");
    assert(doc.cells()[1].source() == "second");
    assert(doc.cells()[2].source() == "third");

    const bool insertedAtEnd =
        doc.insert_cell(
            doc.cell_count(),
            vix::note::NoteCell::cpp("fourth"));

    assert(insertedAtEnd);
    assert(doc.cell_count() == 4);
    assert(doc.cells()[3].source() == "fourth");

    const bool badInsert =
        doc.insert_cell(
            99,
            vix::note::NoteCell::markdown("bad"));

    assert(!badInsert);
    assert(doc.cell_count() == 4);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("first");
    doc.add_reply("second");
    doc.add_cpp("third");

    assert(doc.remove_cell(1));
    assert(doc.cell_count() == 2);
    assert(doc.cells()[0].source() == "first");
    assert(doc.cells()[1].source() == "third");

    assert(!doc.remove_cell(42));
    assert(doc.cell_count() == 2);
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
            vix::note::NoteCellKind::Reply,
            "B"));

    assert(doc.remove_cell_by_id("a"));
    assert(doc.cell_count() == 1);
    assert(doc.cells()[0].id() == "b");

    assert(!doc.remove_cell_by_id("missing"));
    assert(doc.cell_count() == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cell-1",
            vix::note::NoteCellKind::Markdown,
            "first"));

    doc.add_cell(
        vix::note::NoteCell(
            "cell-2",
            vix::note::NoteCellKind::Cpp,
            "second"));

    vix::note::NoteCell *cell = doc.cell_at(1);

    assert(cell != nullptr);
    assert(cell->id() == "cell-2");
    assert(cell->source() == "second");

    assert(doc.cell_at(99) == nullptr);

    const vix::note::NoteDocument &constDoc = doc;
    const vix::note::NoteCell *constCell = constDoc.cell_at(0);

    assert(constCell != nullptr);
    assert(constCell->id() == "cell-1");
    assert(constDoc.cell_at(99) == nullptr);
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

    vix::note::NoteCell *cell = doc.find_cell("run");

    assert(cell != nullptr);
    assert(cell->id() == "run");
    assert(cell->kind() == vix::note::NoteCellKind::Reply);

    assert(doc.find_cell("missing") == nullptr);

    const vix::note::NoteDocument &constDoc = doc;
    const vix::note::NoteCell *constCell = constDoc.find_cell("intro");

    assert(constCell != nullptr);
    assert(constCell->id() == "intro");
    assert(constDoc.find_cell("missing") == nullptr);
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

    std::optional<std::size_t> a = doc.cell_index("a");
    std::optional<std::size_t> b = doc.cell_index("b");
    std::optional<std::size_t> c = doc.cell_index("c");

    assert(a.has_value());
    assert(b.has_value());
    assert(!c.has_value());

    assert(*a == 0);
    assert(*b == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_reply("println(\"hello\")")
        .add_output(vix::note::NoteOutput::stdout_text("hello\n"));

    doc.add_cpp("int main() { return 0; }")
        .add_output(vix::note::NoteOutput::stdout_text("ok\n"));

    assert(doc.cells()[0].has_outputs());
    assert(doc.cells()[1].has_outputs());

    doc.clear_outputs();

    assert(!doc.cells()[0].has_outputs());
    assert(!doc.cells()[1].has_outputs());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("one");
    doc.add_reply("two");

    assert(doc.cell_count() == 2);

    doc.clear_cells();

    assert(doc.empty());
    assert(doc.cell_count() == 0);
    assert(!doc.has_executable_cells());
  }

  {
    vix::note::NoteDocument doc;

    std::vector<vix::note::NoteCell> &cells = doc.cells();

    cells.push_back(vix::note::NoteCell::markdown("from mutable cells"));
    cells.push_back(vix::note::NoteCell::cpp("int main() { return 0; }"));

    assert(doc.cell_count() == 2);
    assert(doc.cells()[0].kind() == vix::note::NoteCellKind::Markdown);
    assert(doc.cells()[1].kind() == vix::note::NoteCellKind::Cpp);
    assert(doc.has_executable_cells());
  }

  return 0;
}
