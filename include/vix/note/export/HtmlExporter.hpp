/**
 *
 *  @file HtmlExporter.hpp
 *  @author Gaspard Kirira
 *
 *  @brief HTML exporter for Vix Note documents.
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

#ifndef VIX_NOTE_EXPORT_HTML_EXPORTER_HPP
#define VIX_NOTE_EXPORT_HTML_EXPORTER_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteError.hpp>
#include <vix/note/core/NoteResult.hpp>

#include <filesystem>
#include <string>

namespace vix::note
{
  /**
   * @brief Options controlling HTML export.
   */
  struct HtmlExporterOptions
  {
    /**
     * @brief Generates a complete standalone HTML document.
     *
     * When false, only the inner note markup is generated.
     */
    bool standalone = true;

    /**
     * @brief Includes cell outputs in the exported document.
     */
    bool includeOutputs = true;

    /**
     * @brief Includes cell titles when available.
     */
    bool includeCellTitles = true;

    /**
     * @brief Includes execution counts for executable cells.
     */
    bool includeExecutionCounts = true;

    /**
     * @brief Includes document metadata in standalone exports.
     */
    bool includeDocumentMetadata = true;

    /**
     * @brief Includes a simple table of contents when headings are found.
     */
    bool includeTableOfContents = true;

    /**
     * @brief Includes visible labels above cell outputs.
     */
    bool includeOutputLabels = true;

    /**
     * @brief Adds print-friendly CSS rules to standalone exports.
     */
    bool printableLayout = true;

    /**
     * @brief Default page title used when the document has no title.
     */
    std::string defaultTitle = "Vix Note";

    /**
     * @brief Optional custom CSS inserted into standalone exports.
     *
     * When empty, the built-in default export CSS is used.
     */
    std::string customCss;
  };

  /**
   * @brief Exports Vix Note documents to static HTML.
   *
   * HtmlExporter is used for read-only sharing and documentation output. It
   * does not execute cells. It only renders the current document state, source
   * cells, and optional outputs.
   */
  class HtmlExporter
  {
  public:
    /**
     * @brief Creates an exporter with default options.
     */
    HtmlExporter();

    /**
     * @brief Creates an exporter with custom options.
     *
     * @param options Exporter options.
     */
    explicit HtmlExporter(HtmlExporterOptions options);

    /**
     * @brief Returns the current exporter options.
     *
     * @return Exporter options.
     */
    const HtmlExporterOptions &options() const noexcept;

    /**
     * @brief Replaces the current exporter options.
     *
     * @param options New exporter options.
     */
    void set_options(HtmlExporterOptions options) noexcept;

    /**
     * @brief Renders a note document to HTML.
     *
     * @param document Document to render.
     * @return Rendered HTML.
     */
    std::string render(const NoteDocument &document) const;

    /**
     * @brief Exports a note document to an HTML file.
     *
     * @param document Document to export.
     * @param path     Target HTML file path.
     * @return Export result.
     */
    NoteResult export_to_file(
        const NoteDocument &document,
        const std::filesystem::path &path) const;

    /**
     * @brief Exports a note document to an HTML file or throws on failure.
     *
     * @param document Document to export.
     * @param path     Target HTML file path.
     *
     * @throws NoteError when writing fails.
     */
    void export_to_file_or_throw(
        const NoteDocument &document,
        const std::filesystem::path &path) const;

    /**
     * @brief Returns the default export CSS.
     *
     * @return CSS content.
     */
    static std::string default_css();

  private:
    /**
     * @brief Renders the inner note body.
     *
     * @param document Document to render.
     * @return Rendered body markup.
     */
    std::string render_body(const NoteDocument &document) const;

    /**
     * @brief Renders document metadata.
     *
     * @param document Document to render metadata for.
     * @return Rendered metadata markup.
     */
    std::string render_document_metadata(const NoteDocument &document) const;

    /**
     * @brief Renders a simple table of contents.
     *
     * @param document Document used to build the table of contents.
     * @return Rendered table of contents markup.
     */
    std::string render_table_of_contents(const NoteDocument &document) const;

    /**
     * @brief Renders a single note cell.
     *
     * @param cell  Cell to render.
     * @param index Zero-based cell index.
     * @return Rendered cell markup.
     */
    std::string render_cell(
        const NoteCell &cell,
        std::size_t index) const;

    /**
     * @brief Renders a single output.
     *
     * @param output Output to render.
     * @return Rendered output markup.
     */
    std::string render_output(const NoteOutput &output) const;

    /**
     * @brief Renders cell outputs.
     *
     * @param cell Cell containing outputs.
     * @return Rendered output markup.
     */
    std::string render_outputs(const NoteCell &cell) const;

    /**
     * @brief Exporter options.
     */
    HtmlExporterOptions options_;
  };

  /**
   * @brief Escapes text for safe HTML output.
   *
   * @param value Raw text.
   * @return Escaped HTML text.
   */
  std::string html_escape(const std::string &value);

  /**
   * @brief Renders a small markdown subset to HTML.
   *
   * This helper supports headings and paragraphs. It is intentionally small
   * and dependency-free for the first Vix Note export format.
   *
   * @param markdown Markdown source.
   * @return Rendered HTML.
   */
  std::string render_note_markdown(const std::string &markdown);

  /**
   * @brief Renders a note document to HTML using default options.
   *
   * @param document Document to render.
   * @return Rendered HTML.
   */
  std::string export_note_html(const NoteDocument &document);

  /**
   * @brief Exports a note document to an HTML file using default options.
   *
   * @param document Document to export.
   * @param path     Target HTML file path.
   * @return Export result.
   */
  NoteResult export_note_html_file(
      const NoteDocument &document,
      const std::filesystem::path &path);
}

#endif // VIX_NOTE_EXPORT_HTML_EXPORTER_HPP
