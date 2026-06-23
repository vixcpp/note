/**
 *
 *  @file html_exporter_test.cpp
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
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/export/HtmlExporter.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
  std::filesystem::path make_test_root()
  {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("vix-note-html-exporter-test-" + std::to_string(now));
  }

  std::string read_file(const std::filesystem::path &path)
  {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }

  bool contains(const std::string &text, std::string_view needle)
  {
    return text.find(std::string(needle)) != std::string::npos;
  }
}

int main()
{
  const std::filesystem::path root = make_test_root();

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  {
    assert(vix::note::html_escape("hello") == "hello");
    assert(vix::note::html_escape("<tag>") == "&lt;tag&gt;");
    assert(vix::note::html_escape("A & B") == "A &amp; B");
    assert(vix::note::html_escape("\"quote\"") == "&quot;quote&quot;");
    assert(vix::note::html_escape("'single'") == "&#39;single&#39;");
    assert(vix::note::html_escape("<a href=\"x\">A & B</a>") == "&lt;a href=&quot;x&quot;&gt;A &amp; B&lt;/a&gt;");
  }

  {
    const std::string html =
        vix::note::render_note_markdown(
            "# Title\n"
            "\n"
            "First paragraph.\n"
            "Second line.\n"
            "\n"
            "## Section\n"
            "\n"
            "### Subsection\n"
            "\n"
            "<unsafe>");

    assert(contains(html, "<h1>Title</h1>"));
    assert(contains(html, "<p>First paragraph.\nSecond line.</p>"));
    assert(contains(html, "<h2>Section</h2>"));
    assert(contains(html, "<h3>Subsection</h3>"));
    assert(contains(html, "&lt;unsafe&gt;"));
  }

  {
    vix::note::HtmlExporter exporter;

    assert(exporter.options().standalone);
    assert(exporter.options().includeOutputs);
    assert(exporter.options().includeCellTitles);
    assert(exporter.options().includeExecutionCounts);
    assert(exporter.options().defaultTitle == "Vix Note");
    assert(exporter.options().customCss.empty());
  }

  {
    vix::note::HtmlExporterOptions options;
    options.standalone = false;
    options.includeOutputs = false;
    options.includeCellTitles = false;
    options.includeExecutionCounts = false;
    options.defaultTitle = "Custom Title";
    options.customCss = "body { color: red; }";

    vix::note::HtmlExporter exporter(options);

    assert(!exporter.options().standalone);
    assert(!exporter.options().includeOutputs);
    assert(!exporter.options().includeCellTitles);
    assert(!exporter.options().includeExecutionCounts);
    assert(exporter.options().defaultTitle == "Custom Title");
    assert(exporter.options().customCss == "body { color: red; }");
  }

  {
    vix::note::HtmlExporter exporter;

    vix::note::HtmlExporterOptions options;
    options.defaultTitle = "Changed";

    exporter.set_options(options);

    assert(exporter.options().defaultTitle == "Changed");
  }

  {
    const std::string css =
        vix::note::HtmlExporter::default_css();

    assert(contains(css, ".vix-note-export"));
    assert(contains(css, ".vix-note-cell"));
    assert(contains(css, ".vix-note-code"));
    assert(contains(css, ".vix-note-output"));
  }

  {
    vix::note::NoteDocument doc("Learning C++");

    doc.add_markdown(
        "# Learning C++\n"
        "\n"
        "This note explains variables.");

    doc.add_cpp(
        "#include <iostream>\n"
        "int main()\n"
        "{\n"
        "  std::cout << \"hello\" << std::endl;\n"
        "  return 0;\n"
        "}");

    doc.cells()[1].set_execution_count(1);
    doc.cells()[1].add_output(
        vix::note::NoteOutput::stdout_text("hello\n"));

    vix::note::HtmlExporter exporter;

    const std::string html =
        exporter.render(doc);

    assert(contains(html, "<!doctype html>"));
    assert(contains(html, "<title>Learning C++</title>"));
    assert(contains(html, "class=\"vix-note-export\""));
    assert(contains(html, "<h1>Learning C++</h1>"));
    assert(contains(html, "<p>This note explains variables.</p>"));
    assert(contains(html, "C++ cell 2"));
    assert(contains(html, "In [1]"));
    assert(contains(html, "#include &lt;iostream&gt;"));
    assert(contains(html, "hello"));
    assert(contains(html, "vix-note-output--stdout"));
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Untitled Source");

    vix::note::HtmlExporterOptions options;
    options.defaultTitle = "Fallback Title";

    vix::note::HtmlExporter exporter(options);

    const std::string html =
        exporter.render(doc);

    assert(contains(html, "<title>Fallback Title</title>"));
    assert(contains(html, "Fallback Title"));
  }

  {
    vix::note::NoteDocument doc("Fragment");

    doc.add_markdown("# Fragment");
    doc.add_reply("println(\"hello\")");

    vix::note::HtmlExporterOptions options;
    options.standalone = false;

    vix::note::HtmlExporter exporter(options);

    const std::string html =
        exporter.render(doc);

    assert(!contains(html, "<!doctype html>"));
    assert(!contains(html, "<html"));
    assert(contains(html, "<main class=\"vix-note-export\">"));
    assert(contains(html, "Reply cell 2"));
  }

  {
    vix::note::NoteDocument doc("No Outputs");

    doc.add_cpp("int main() { return 0; }");
    doc.cells()[0].add_output(
        vix::note::NoteOutput::stdout_text("should not appear"));

    vix::note::HtmlExporterOptions options;
    options.includeOutputs = false;

    vix::note::HtmlExporter exporter(options);

    const std::string html =
        exporter.render(doc);

    assert(!contains(html, "should not appear"));
    assert(!contains(html, "<div class=\"vix-note-outputs\">"));
  }

  {
    vix::note::NoteDocument doc("No Execution Count");

    doc.add_cpp("int main() { return 0; }");
    doc.cells()[0].set_execution_count(9);

    vix::note::HtmlExporterOptions options;
    options.includeExecutionCounts = false;

    vix::note::HtmlExporter exporter(options);

    const std::string html =
        exporter.render(doc);

    assert(!contains(html, "In [9]"));
  }

  {
    vix::note::NoteDocument doc("Cell Titles");

    vix::note::NoteCell cell =
        vix::note::NoteCell::cpp("int main() { return 0; }");

    cell.set_title("Runnable example");

    doc.add_cell(cell);

    vix::note::HtmlExporter exporter;

    const std::string html =
        exporter.render(doc);

    assert(contains(html, "Runnable example"));

    vix::note::HtmlExporterOptions options;
    options.includeCellTitles = false;

    exporter.set_options(options);

    const std::string withoutTitle =
        exporter.render(doc);

    assert(!contains(withoutTitle, "Runnable example"));
  }

  {
    vix::note::NoteDocument doc("HTML Cell");

    doc.add_html("<section><strong>Raw HTML</strong></section>");

    vix::note::HtmlExporter exporter;

    const std::string html =
        exporter.render(doc);

    assert(contains(html, "<section><strong>Raw HTML</strong></section>"));
    assert(!contains(html, "&lt;section&gt;&lt;strong&gt;Raw HTML"));
  }

  {
    vix::note::NoteDocument doc("Escaped Code");

    doc.add_cpp(
        "int main()\n"
        "{\n"
        "  // <unsafe> & \"quoted\"\n"
        "  return 0;\n"
        "}");

    vix::note::HtmlExporter exporter;

    const std::string html =
        exporter.render(doc);

    assert(contains(html, "&lt;unsafe&gt; &amp; &quot;quoted&quot;"));
  }

  {
    vix::note::NoteDocument doc("Outputs");

    doc.add_reply("println(\"hello\")");

    doc.cells()[0].add_output(
        vix::note::NoteOutput::stdout_text("stdout text"));

    doc.cells()[0].add_output(
        vix::note::NoteOutput::stderr_text("stderr text"));

    doc.cells()[0].add_output(
        vix::note::NoteOutput::error("error text"));

    vix::note::HtmlExporter exporter;

    const std::string html =
        exporter.render(doc);

    assert(contains(html, "vix-note-output--stdout"));
    assert(contains(html, "stdout text"));

    assert(contains(html, "vix-note-output--stderr"));
    assert(contains(html, "stderr text"));

    assert(contains(html, "vix-note-output--error"));
    assert(contains(html, "error text"));
  }

  {
    vix::note::NoteDocument doc("Custom CSS");

    doc.add_markdown("# Custom CSS");

    vix::note::HtmlExporterOptions options;
    options.customCss = ".custom { color: orange; }";

    vix::note::HtmlExporter exporter(options);

    const std::string html =
        exporter.render(doc);

    assert(contains(html, ".custom { color: orange; }"));
    assert(!contains(html, "--note-accent"));
  }

  {
    vix::note::NoteDocument doc("Export File");

    doc.add_markdown("# Export File");
    doc.add_cpp("int main() { return 0; }");

    const std::filesystem::path file =
        root / "exports" / "note.html";

    vix::note::HtmlExporter exporter;

    vix::note::NoteResult result =
        exporter.export_to_file(doc, file);

    assert(result.ok());
    assert(result.message() == "HTML export written");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Text);
    assert(result.outputs()[0].content == file.string());

    assert(std::filesystem::exists(file));

    const std::string content =
        read_file(file);

    assert(contains(content, "<!doctype html>"));
    assert(contains(content, "Export File"));
    assert(contains(content, "int main()"));
  }

  {
    vix::note::NoteDocument doc("Empty Path");

    vix::note::HtmlExporter exporter;

    vix::note::NoteResult result =
        exporter.export_to_file(doc, {});

    assert(result.failed());
    assert(result.message() == "empty HTML export path");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteDocument doc("Throw Export");

    doc.add_markdown("# Throw Export");

    const std::filesystem::path file =
        root / "exports" / "throw.html";

    vix::note::HtmlExporter exporter;

    exporter.export_to_file_or_throw(doc, file);

    assert(std::filesystem::exists(file));
    assert(contains(read_file(file), "Throw Export"));
  }

  {
    vix::note::NoteDocument doc("Throw Empty Path");

    vix::note::HtmlExporter exporter;

    bool thrown = false;

    try
    {
      exporter.export_to_file_or_throw(doc, {});
    }
    catch (const vix::note::NoteError &error)
    {
      thrown = true;

      assert(error.code() == vix::note::NoteErrorCode::Write);
      assert(std::string(error.what()) == "empty HTML export path");
    }

    assert(thrown);
  }

  {
    vix::note::NoteDocument doc("Free Function");

    doc.add_markdown("# Free Function");

    const std::string html =
        vix::note::export_note_html(doc);

    assert(contains(html, "<!doctype html>"));
    assert(contains(html, "Free Function"));
  }

  {
    vix::note::NoteDocument doc("Free File");

    doc.add_markdown("# Free File");

    const std::filesystem::path file =
        root / "free" / "free.html";

    vix::note::NoteResult result =
        vix::note::export_note_html_file(doc, file);

    assert(result.ok());
    assert(std::filesystem::exists(file));
    assert(contains(read_file(file), "Free File"));
  }

  std::filesystem::remove_all(root);

  return 0;
}
