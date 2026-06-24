/**
 *
 *  @file HtmlExporter.cpp
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

#include <vix/note/export/HtmlExporter.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace vix::note
{
  namespace
  {
    std::string trim_copy(std::string_view value)
    {
      std::size_t begin = 0;

      while (begin < value.size() &&
             (value[begin] == ' ' ||
              value[begin] == '\t' ||
              value[begin] == '\n' ||
              value[begin] == '\r'))
      {
        ++begin;
      }

      std::size_t end = value.size();

      while (end > begin &&
             (value[end - 1] == ' ' ||
              value[end - 1] == '\t' ||
              value[end - 1] == '\n' ||
              value[end - 1] == '\r'))
      {
        --end;
      }

      return std::string(value.substr(begin, end - begin));
    }

    bool starts_with(std::string_view value, std::string_view prefix)
    {
      return value.size() >= prefix.size() &&
             value.substr(0, prefix.size()) == prefix;
    }

    bool write_text_file(
        const std::filesystem::path &path,
        const std::string &content,
        std::string &err)
    {
      err.clear();

      const std::filesystem::path parent = path.parent_path();

      if (!parent.empty())
      {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);

        if (ec)
        {
          err = "cannot create export directory: " +
                parent.string() +
                ": " +
                ec.message();

          return false;
        }
      }

      std::ofstream out(path, std::ios::binary | std::ios::trunc);

      if (!out.is_open())
      {
        err = "cannot write HTML export file: " + path.string();
        return false;
      }

      out << content;

      if (!out.good())
      {
        err = "cannot write HTML export file: " + path.string();
        return false;
      }

      return true;
    }

    std::string cell_kind_class(NoteCellKind kind)
    {
      switch (kind)
      {
      case NoteCellKind::Markdown:
        return "markdown";

      case NoteCellKind::Reply:
        return "reply";

      case NoteCellKind::Cpp:
        return "cpp";

      case NoteCellKind::Html:
        return "html";

      case NoteCellKind::Unknown:
      default:
        return "unknown";
      }
    }

    std::string cell_language_label(NoteCellKind kind)
    {
      switch (kind)
      {
      case NoteCellKind::Markdown:
        return "Markdown";

      case NoteCellKind::Reply:
        return "Reply";

      case NoteCellKind::Cpp:
        return "C++";

      case NoteCellKind::Html:
        return "HTML";

      case NoteCellKind::Unknown:
      default:
        return "Unknown";
      }
    }

    std::string output_kind_class(NoteOutputKind kind)
    {
      return std::string(to_string(kind));
    }

    std::string output_kind_label(NoteOutputKind kind)
    {
      switch (kind)
      {
      case NoteOutputKind::Text:
        return "Text";

      case NoteOutputKind::Stdout:
        return "Output";

      case NoteOutputKind::Stderr:
        return "Error stream";

      case NoteOutputKind::Html:
        return "HTML output";

      case NoteOutputKind::Error:
        return "Error";

      case NoteOutputKind::CompilerError:
        return "Compiler error";

      case NoteOutputKind::RuntimeError:
        return "Runtime error";

      case NoteOutputKind::Debug:
        return "Debug";

      case NoteOutputKind::Hint:
        return "Hint";

      case NoteOutputKind::RawLog:
        return "Raw log";
      }

      return "Output";
    }

    std::string code_language_class(NoteCellKind kind)
    {
      switch (kind)
      {
      case NoteCellKind::Cpp:
        return "language-cpp";

      case NoteCellKind::Reply:
        return "language-reply";

      case NoteCellKind::Html:
        return "language-html";

      case NoteCellKind::Markdown:
        return "language-markdown";

      case NoteCellKind::Unknown:
      default:
        return "language-text";
      }
    }

    std::string heading_id(std::size_t index)
    {
      return "section-" + std::to_string(index + 1);
    }
  }

  HtmlExporter::HtmlExporter() = default;

  HtmlExporter::HtmlExporter(HtmlExporterOptions options)
      : options_(std::move(options))
  {
  }

  const HtmlExporterOptions &HtmlExporter::options() const noexcept
  {
    return options_;
  }

  void HtmlExporter::set_options(HtmlExporterOptions options) noexcept
  {
    options_ = std::move(options);
  }

  std::string HtmlExporter::render(const NoteDocument &document) const
  {
    const std::string title =
        document.has_title() ? document.title() : options_.defaultTitle;

    const std::string body = render_body(document);

    if (!options_.standalone)
    {
      return body;
    }

    const std::string css =
        options_.customCss.empty()
            ? default_css()
            : options_.customCss;

    std::ostringstream out;

    out << "<!doctype html>\n";
    out << "<html lang=\"en\">\n";
    out << "<head>\n";
    out << "  <meta charset=\"utf-8\">\n";
    out << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    out << "  <title>" << html_escape(title) << "</title>\n";
    out << "  <style>\n";
    out << css;
    out << "\n";
    out << "  </style>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << body;
    out << "\n";
    out << "</body>\n";
    out << "</html>\n";

    return out.str();
  }

  NoteResult HtmlExporter::export_to_file(
      const NoteDocument &document,
      const std::filesystem::path &path) const
  {
    if (path.empty())
    {
      return NoteResult::failure("empty HTML export path", 1)
          .add_error("empty HTML export path");
    }

    const std::string html = render(document);

    std::string err;

    if (!write_text_file(path, html, err))
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    return NoteResult::success("HTML export written")
        .add_text(path.string());
  }

  void HtmlExporter::export_to_file_or_throw(
      const NoteDocument &document,
      const std::filesystem::path &path) const
  {
    NoteResult result = export_to_file(document, path);

    if (!result.ok())
    {
      throw NoteError(
          NoteErrorCode::Write,
          result.message().empty() ? "failed to export HTML" : result.message());
    }
  }

  std::string HtmlExporter::default_css()
  {
    return R"(    :root {
      color-scheme: light;
      --note-bg: #f6f7fb;
      --note-panel: #ffffff;
      --note-text: #111827;
      --note-muted: #6b7280;
      --note-border: #e5e7eb;
      --note-accent: #d57a2a;
      --note-code-bg: #111827;
      --note-code-text: #f9fafb;
      --note-output-bg: #fefce8;
      --note-error-bg: #fef2f2;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      background: var(--note-bg);
      color: var(--note-text);
      font-family:
        Inter,
        ui-sans-serif,
        system-ui,
        -apple-system,
        BlinkMacSystemFont,
        "Segoe UI",
        sans-serif;
    }

    .vix-note-export {
      width: min(1040px, calc(100% - 32px));
      margin: 0 auto;
      padding: 36px 0;
    }

    .vix-note-export__header {
      margin-bottom: 28px;
    }

    .vix-note-export__eyebrow {
      margin: 0 0 8px;
      color: var(--note-accent);
      font-size: 0.85rem;
      font-weight: 800;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }

    .vix-note-export__title {
      margin: 0;
      font-size: clamp(2rem, 6vw, 4.5rem);
      line-height: 1;
    }

    .vix-note-export__meta {
      margin: 12px 0 0;
      color: var(--note-muted);
    }

    .vix-note-export__cells {
      display: grid;
      gap: 18px;
    }

    .vix-note-cell {
      overflow: hidden;
      border: 1px solid var(--note-border);
      border-radius: 18px;
      background: var(--note-panel);
      box-shadow: 0 18px 45px rgba(15, 23, 42, 0.06);
    }

    .vix-note-cell__bar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      padding: 12px 16px;
      border-bottom: 1px solid var(--note-border);
      color: var(--note-muted);
      font-size: 0.9rem;
      font-weight: 700;
    }

    .vix-note-cell__title {
      color: var(--note-text);
    }

    .vix-note-cell__body {
      padding: 20px;
    }

    .vix-note-cell__body > :first-child {
      margin-top: 0;
    }

    .vix-note-cell__body > :last-child {
      margin-bottom: 0;
    }

    .vix-note-code {
      margin: 0;
      padding: 20px;
      overflow: auto;
      background: var(--note-code-bg);
      color: var(--note-code-text);
      font-size: 0.95rem;
      line-height: 1.6;
    }

    .vix-note-outputs {
      display: grid;
      gap: 10px;
      padding: 16px;
      border-top: 1px solid var(--note-border);
      background: #fafafa;
    }

    .vix-note-output {
      margin: 0;
      padding: 14px;
      border-radius: 12px;
      background: var(--note-output-bg);
      white-space: pre-wrap;
      overflow: auto;
    }

    .vix-note-output--error,
    .vix-note-output--stderr {
      background: var(--note-error-bg);
    }

    .vix-note-export__meta {
      display: flex;
      flex-wrap: wrap;
      gap: 8px 14px;
      margin: 14px 0 0;
      color: var(--note-muted);
      font-size: 0.95rem;
    }

    .vix-note-toc {
      margin: 0 0 24px;
      padding: 18px 20px;
      border: 1px solid var(--note-border);
      border-radius: 18px;
      background: var(--note-panel);
      box-shadow: 0 18px 45px rgba(15, 23, 42, 0.04);
    }

    .vix-note-toc h2 {
      margin: 0 0 10px;
      font-size: 1rem;
    }

    .vix-note-toc ol {
      margin: 0;
      padding-left: 20px;
    }

    .vix-note-toc a {
      color: var(--note-accent);
      font-weight: 700;
      text-decoration: none;
    }

    .vix-note-toc a:hover {
      text-decoration: underline;
    }

    .vix-note-code code {
      font-family:
        "JetBrains Mono",
        "SFMono-Regular",
        Consolas,
        monospace;
    }

    .vix-note-output-wrap {
      display: grid;
      gap: 6px;
    }

    .vix-note-output__label {
      margin: 0;
      color: var(--note-muted);
      font-size: 0.78rem;
      font-weight: 800;
      letter-spacing: 0.06em;
      text-transform: uppercase;
    }

    .vix-note-output--compiler_error,
    .vix-note-output--runtime_error,
    .vix-note-output--error,
    .vix-note-output--stderr {
      background: var(--note-error-bg);
      color: #7f1d1d;
    }

    .vix-note-output--hint {
      background: #ecfdf5;
      color: #065f46;
    }

    .vix-note-output--debug,
    .vix-note-output--raw_log {
      background: #eef2ff;
      color: #312e81;
    }

    @media print {
      body {
        background: #ffffff;
      }

      .vix-note-export {
        width: 100%;
        padding: 0;
      }

      .vix-note-cell,
      .vix-note-toc {
        break-inside: avoid;
        box-shadow: none;
      }

      .vix-note-code {
        white-space: pre-wrap;
      }
    }

    @media (max-width: 720px) {
      .vix-note-export {
        width: min(100% - 20px, 1040px);
        padding: 24px 0;
      }
    })";
  }

  std::string HtmlExporter::render_body(const NoteDocument &document) const
  {
    const std::string title =
        document.has_title() ? document.title() : options_.defaultTitle;

    std::ostringstream out;

    out << "<main class=\"vix-note-export\">\n";
    out << "  <header class=\"vix-note-export__header\">\n";
    out << "    <p class=\"vix-note-export__eyebrow\">Vix Note</p>\n";
    out << "    <h1 class=\"vix-note-export__title\">"
        << html_escape(title)
        << "</h1>\n";

    if (options_.includeDocumentMetadata)
    {
      out << render_document_metadata(document);
    }

    out << "  </header>\n";

    if (options_.includeTableOfContents)
    {
      out << render_table_of_contents(document);
    }

    out << "  <section class=\"vix-note-export__cells\">\n";

    for (std::size_t i = 0; i < document.cells().size(); ++i)
    {
      out << render_cell(document.cells()[i], i);
    }

    out << "  </section>\n";
    out << "</main>";

    return out.str();
  }

  std::string HtmlExporter::render_document_metadata(const NoteDocument &document) const
  {
    std::ostringstream out;

    out << "    <div class=\"vix-note-export__meta\">\n";
    out << "      <span>"
        << document.cell_count()
        << " cell";

    if (document.cell_count() != 1)
    {
      out << "s";
    }

    out << "</span>\n";

    if (!document.path().empty())
    {
      out << "      <span>"
          << html_escape(document.path())
          << "</span>\n";
    }

    out << "      <span>execution count: "
        << document.execution_count()
        << "</span>\n";

    out << "    </div>\n";

    return out.str();
  }

  std::string HtmlExporter::render_table_of_contents(const NoteDocument &document) const
  {
    std::ostringstream items;

    std::size_t count = 0;

    for (std::size_t i = 0; i < document.cells().size(); ++i)
    {
      const NoteCell &cell = document.cells()[i];

      if (cell.kind() != NoteCellKind::Markdown)
      {
        continue;
      }

      std::istringstream in(cell.source());
      std::string line;

      while (std::getline(in, line))
      {
        const std::string trimmed = trim_copy(line);

        if (!starts_with(trimmed, "# "))
        {
          continue;
        }

        items << "      <li><a href=\"#"
              << heading_id(i)
              << "\">"
              << html_escape(trim_copy(std::string_view(trimmed).substr(2)))
              << "</a></li>\n";

        ++count;
        break;
      }
    }

    if (count == 0)
    {
      return {};
    }

    std::ostringstream out;

    out << "  <nav class=\"vix-note-toc\" aria-label=\"Table of contents\">\n";
    out << "    <h2>Contents</h2>\n";
    out << "    <ol>\n";
    out << items.str();
    out << "    </ol>\n";
    out << "  </nav>\n";

    return out.str();
  }

  std::string HtmlExporter::render_cell(
      const NoteCell &cell,
      std::size_t index) const
  {
    std::ostringstream out;

    out << "    <article id=\""
        << heading_id(index)
        << "\" class=\"vix-note-cell vix-note-cell--"
        << cell_kind_class(cell.kind())
        << "\">\n";

    out << "      <div class=\"vix-note-cell__bar\">\n";
    out << "        <span>"
        << cell_language_label(cell.kind())
        << " cell "
        << (index + 1)
        << "</span>\n";

    if (options_.includeExecutionCounts && cell.execution_count() > 0)
    {
      out << "        <span>In ["
          << cell.execution_count()
          << "]</span>\n";
    }

    out << "      </div>\n";

    if (options_.includeCellTitles && !cell.title().empty())
    {
      out << "      <div class=\"vix-note-cell__body vix-note-cell__title\">"
          << html_escape(cell.title())
          << "</div>\n";
    }

    switch (cell.kind())
    {
    case NoteCellKind::Markdown:
      out << "      <div class=\"vix-note-cell__body\">\n";
      out << render_note_markdown(cell.source());
      out << "\n";
      out << "      </div>\n";
      break;

    case NoteCellKind::Html:
      out << "      <div class=\"vix-note-cell__body\">\n";
      out << cell.source();
      out << "\n";
      out << "      </div>\n";
      break;

    case NoteCellKind::Reply:
    case NoteCellKind::Cpp:
    case NoteCellKind::Unknown:
    default:
      out << "      <pre class=\"vix-note-code\"><code class=\""
          << code_language_class(cell.kind())
          << "\">"
          << html_escape(cell.source())
          << "</code></pre>\n";
      ;
      break;
    }

    if (options_.includeOutputs && cell.has_outputs())
    {
      out << render_outputs(cell);
    }

    out << "    </article>\n";

    return out.str();
  }

  std::string HtmlExporter::render_outputs(const NoteCell &cell) const
  {
    std::ostringstream out;

    out << "      <div class=\"vix-note-outputs\">\n";

    for (const NoteOutput &output : cell.outputs())
    {
      out << render_output(output);
    }

    out << "      </div>\n";

    return out.str();
  }

  std::string HtmlExporter::render_output(const NoteOutput &output) const
  {
    std::ostringstream out;

    out << "        <section class=\"vix-note-output-wrap vix-note-output-wrap--"
        << output_kind_class(output.kind)
        << "\">\n";

    if (options_.includeOutputLabels)
    {
      out << "          <p class=\"vix-note-output__label\">"
          << html_escape(output_kind_label(output.kind))
          << "</p>\n";
    }

    if (output.kind == NoteOutputKind::Html)
    {
      out << "          <div class=\"vix-note-output vix-note-output--html\">\n";
      out << output.content;
      out << "\n";
      out << "          </div>\n";
    }
    else
    {
      out << "          <pre class=\"vix-note-output vix-note-output--"
          << output_kind_class(output.kind)
          << "\">"
          << html_escape(output.content)
          << "</pre>\n";
    }

    out << "        </section>\n";

    return out.str();
  }

  std::string html_escape(const std::string &value)
  {
    std::string out;
    out.reserve(value.size() + 16);

    for (char c : value)
    {
      switch (c)
      {
      case '&':
        out += "&amp;";
        break;

      case '<':
        out += "&lt;";
        break;

      case '>':
        out += "&gt;";
        break;

      case '"':
        out += "&quot;";
        break;

      case '\'':
        out += "&#39;";
        break;

      default:
        out.push_back(c);
        break;
      }
    }

    return out;
  }

  std::string render_note_markdown(const std::string &markdown)
  {
    std::istringstream in(markdown);
    std::ostringstream out;
    std::string paragraph;
    std::string line;

    auto flush_paragraph = [&]()
    {
      const std::string trimmed = trim_copy(paragraph);

      if (!trimmed.empty())
      {
        out << "        <p>"
            << html_escape(trimmed)
            << "</p>\n";
      }

      paragraph.clear();
    };

    while (std::getline(in, line))
    {
      const std::string trimmed = trim_copy(line);

      if (trimmed.empty())
      {
        flush_paragraph();
        continue;
      }

      if (starts_with(trimmed, "### "))
      {
        flush_paragraph();

        out << "        <h3>"
            << html_escape(trim_copy(std::string_view(trimmed).substr(4)))
            << "</h3>\n";

        continue;
      }

      if (starts_with(trimmed, "## "))
      {
        flush_paragraph();

        out << "        <h2>"
            << html_escape(trim_copy(std::string_view(trimmed).substr(3)))
            << "</h2>\n";

        continue;
      }

      if (starts_with(trimmed, "# "))
      {
        flush_paragraph();

        out << "        <h1>"
            << html_escape(trim_copy(std::string_view(trimmed).substr(2)))
            << "</h1>\n";

        continue;
      }

      if (!paragraph.empty())
      {
        paragraph += '\n';
      }

      paragraph += trimmed;
    }

    flush_paragraph();

    return out.str();
  }

  std::string export_note_html(const NoteDocument &document)
  {
    return HtmlExporter().render(document);
  }

  NoteResult export_note_html_file(
      const NoteDocument &document,
      const std::filesystem::path &path)
  {
    return HtmlExporter().export_to_file(document, path);
  }
}
