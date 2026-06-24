/**
 *
 *  @file note_parser_test.cpp
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
#include <vix/note/core/NoteError.hpp>
#include <vix/note/parser/NoteParser.hpp>

#include <cassert>
#include <stdexcept>
#include <string>

int main()
{
  {
    vix::note::NoteParseResult result =
        vix::note::parse_note("");

    assert(result.ok);
    assert(result.error.empty());
    assert(!result.has_diagnostics());
    assert(result.document.empty());
    assert(result.document.cell_count() == 0);
    assert(!result.document.has_title());
  }

  {
    const std::string source =
        "# Learning C++\n"
        "\n"
        "This note explains variables.\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.has_title());
    assert(result.document.title() == "Learning C++");
    assert(result.document.cell_count() == 1);

    const vix::note::NoteCell *cell =
        result.document.cell_at(0);

    assert(cell != nullptr);
    assert(cell->id() == "cell-1");
    assert(cell->kind() == vix::note::NoteCellKind::Markdown);
    assert(cell->source() == "# Learning C++\n\nThis note explains variables.");
    assert(!cell->executable());
  }

  {
    const std::string source =
        "# Intro\n"
        "\n"
        "Before the code.\n"
        "\n"
        "```reply\n"
        "x = 1 + 2\n"
        "println(x)\n"
        "```\n"
        "\n"
        "After the code.\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.title() == "Intro");
    assert(result.document.cell_count() == 3);

    const vix::note::NoteCell *markdown1 =
        result.document.cell_at(0);

    const vix::note::NoteCell *reply =
        result.document.cell_at(1);

    const vix::note::NoteCell *markdown2 =
        result.document.cell_at(2);

    assert(markdown1 != nullptr);
    assert(reply != nullptr);
    assert(markdown2 != nullptr);

    assert(markdown1->id() == "cell-1");
    assert(markdown1->kind() == vix::note::NoteCellKind::Markdown);
    assert(markdown1->source() == "# Intro\n\nBefore the code.");

    assert(reply->id() == "cell-2");
    assert(reply->kind() == vix::note::NoteCellKind::Reply);
    assert(reply->source() == "x = 1 + 2\nprintln(x)");
    assert(reply->executable());

    assert(markdown2->id() == "cell-3");
    assert(markdown2->kind() == vix::note::NoteCellKind::Markdown);
    assert(markdown2->source() == "After the code.");
  }

  {
    const std::string source =
        "```cpp\n"
        "#include <iostream>\n"
        "\n"
        "int main()\n"
        "{\n"
        "  std::cout << \"Hello\" << std::endl;\n"
        "  return 0;\n"
        "}\n"
        "```\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.cell_count() == 1);

    const vix::note::NoteCell *cell =
        result.document.cell_at(0);

    assert(cell != nullptr);
    assert(cell->id() == "cell-1");
    assert(cell->kind() == vix::note::NoteCellKind::Cpp);
    assert(cell->executable());

    assert(
        cell->source() ==
        "#include <iostream>\n"
        "\n"
        "int main()\n"
        "{\n"
        "  std::cout << \"Hello\" << std::endl;\n"
        "  return 0;\n"
        "}");
  }

  {
    const std::string source =
        "```c++\n"
        "int main() { return 0; }\n"
        "```\n"
        "\n"
        "```repl\n"
        "println(\"ok\")\n"
        "```\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.cell_count() == 2);

    assert(result.document.cells()[0].kind() == vix::note::NoteCellKind::Cpp);
    assert(result.document.cells()[0].source() == "int main() { return 0; }");

    assert(result.document.cells()[1].kind() == vix::note::NoteCellKind::Reply);
    assert(result.document.cells()[1].source() == "println(\"ok\")");
  }

  {
    const std::string source =
        "```html\n"
        "<section>\n"
        "  <h1>Hello</h1>\n"
        "</section>\n"
        "```\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.cell_count() == 1);

    const vix::note::NoteCell *cell =
        result.document.cell_at(0);

    assert(cell != nullptr);
    assert(cell->kind() == vix::note::NoteCellKind::Html);
    assert(!cell->executable());

    assert(
        cell->source() ==
        "<section>\n"
        "  <h1>Hello</h1>\n"
        "</section>");
  }

  {
    const std::string source =
        "```python\n"
        "print('hello')\n"
        "```\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.cell_count() == 1);

    const vix::note::NoteCell *cell =
        result.document.cell_at(0);

    assert(cell != nullptr);
    assert(cell->kind() == vix::note::NoteCellKind::Markdown);

    assert(
        cell->source() ==
        "```python\n"
        "print('hello')\n"
        "```");
  }

  {
    const std::string source =
        "# Title disabled\n"
        "\n"
        "Content.\n";

    vix::note::NoteParseOptions options;
    options.inferTitle = false;

    vix::note::NoteParser parser(options);
    vix::note::NoteParseResult result =
        parser.parse(source);

    assert(result.ok);
    assert(!result.document.has_title());
    assert(result.document.title().empty());
    assert(result.document.cell_count() == 1);
  }

  {
    const std::string source =
        "# Manual ids disabled\n"
        "\n"
        "```reply\n"
        "println(\"hello\")\n"
        "```\n";

    vix::note::NoteParseOptions options;
    options.assignCellIds = false;

    vix::note::NoteParser parser(options);
    vix::note::NoteParseResult result =
        parser.parse(source);

    assert(result.ok);
    assert(result.document.cell_count() == 2);

    assert(result.document.cells()[0].id().empty());
    assert(result.document.cells()[1].id().empty());
  }

  {
    const std::string source =
        "<!-- vixnote:cell id=\"intro\" kind=\"markdown\" -->\n"
        "\n"
        "# Metadata Intro\n"
        "\n"
        "This markdown cell keeps its id.\n"
        "\n"
        "<!-- vixnote:cell id=\"cpp-cell\" kind=\"cpp\" -->\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n"
        "\n"
        "<!-- vixnote:cell id=\"reply-cell\" kind=\"reply\" -->\n"
        "\n"
        "```reply\n"
        "println(\"hello\")\n"
        "```\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.title() == "Metadata Intro");
    assert(result.document.cell_count() == 3);

    assert(result.document.cells()[0].id() == "intro");
    assert(result.document.cells()[0].kind() == vix::note::NoteCellKind::Markdown);
    assert(result.document.cells()[0].source() == "# Metadata Intro\n\nThis markdown cell keeps its id.");

    assert(result.document.cells()[1].id() == "cpp-cell");
    assert(result.document.cells()[1].kind() == vix::note::NoteCellKind::Cpp);
    assert(result.document.cells()[1].source() == "int main() { return 0; }");

    assert(result.document.cells()[2].id() == "reply-cell");
    assert(result.document.cells()[2].kind() == vix::note::NoteCellKind::Reply);
    assert(result.document.cells()[2].source() == "println(\"hello\")");
  }

  {
    const std::string source =
        "<!-- vixnote:cell id=\"manual-markdown\" kind=\"markdown\" -->\n"
        "\n"
        "# Manual Metadata\n"
        "\n"
        "<!-- vixnote:cell id=\"manual-cpp\" kind=\"cpp\" -->\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n";

    vix::note::NoteParseOptions options;
    options.assignCellIds = false;
    options.readCellMetadata = true;

    vix::note::NoteParser parser(options);
    vix::note::NoteParseResult result =
        parser.parse(source);

    assert(result.ok);
    assert(result.document.cell_count() == 2);

    assert(result.document.cells()[0].id() == "manual-markdown");
    assert(result.document.cells()[0].kind() == vix::note::NoteCellKind::Markdown);

    assert(result.document.cells()[1].id() == "manual-cpp");
    assert(result.document.cells()[1].kind() == vix::note::NoteCellKind::Cpp);
  }

  {
    const std::string source =
        "<!-- vixnote:cell id=\"visible-comment\" kind=\"markdown\" -->\n"
        "\n"
        "# Metadata Disabled\n";

    vix::note::NoteParseOptions options;
    options.readCellMetadata = false;

    vix::note::NoteParser parser(options);
    vix::note::NoteParseResult result =
        parser.parse(source);

    assert(result.ok);
    assert(result.document.cell_count() == 1);

    assert(result.document.cells()[0].id() == "cell-1");
    assert(result.document.cells()[0].kind() == vix::note::NoteCellKind::Markdown);

    assert(
        result.document.cells()[0].source() ==
        "<!-- vixnote:cell id=\"visible-comment\" kind=\"markdown\" -->\n\n"
        "# Metadata Disabled");
  }

  {
    const std::string source =
        "<!-- vixnote:cell id=\"html-cell\" kind=\"html\" -->\n"
        "\n"
        "```html\n"
        "<section>HTML</section>\n"
        "```\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(result.ok);
    assert(result.document.cell_count() == 1);

    assert(result.document.cells()[0].id() == "html-cell");
    assert(result.document.cells()[0].kind() == vix::note::NoteCellKind::Html);
    assert(result.document.cells()[0].source() == "<section>HTML</section>");
  }

  {
    const std::string source =
        "# Broken\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n";

    vix::note::NoteParseResult result =
        vix::note::parse_note(source);

    assert(!result.ok);
    assert(!result.error.empty());
    assert(result.error == "unterminated fenced cell starting at line 3");
  }

  {
    const std::string source =
        "```reply\n"
        "println(\"hello\")\n";

    bool thrown = false;

    try
    {
      (void)vix::note::parse_note_or_throw(source);
    }
    catch (const vix::note::NoteError &error)
    {
      thrown = true;

      assert(error.code() == vix::note::NoteErrorCode::Parse);
      assert(std::string(error.what()) == "unterminated fenced cell starting at line 1");
    }

    assert(thrown);
  }

  {
    const std::string source =
        "# Parsed with method\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n";

    vix::note::NoteParser parser;
    vix::note::NoteDocument doc =
        parser.parse_or_throw(source);

    assert(doc.title() == "Parsed with method");
    assert(doc.cell_count() == 2);
    assert(doc.cells()[0].kind() == vix::note::NoteCellKind::Markdown);
    assert(doc.cells()[1].kind() == vix::note::NoteCellKind::Cpp);
  }

  return 0;
}
