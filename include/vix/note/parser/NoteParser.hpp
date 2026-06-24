/**
 *
 *  @file NoteParser.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Parser for Vix Note executable notebook documents.
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

#ifndef VIX_NOTE_PARSER_NOTE_PARSER_HPP
#define VIX_NOTE_PARSER_NOTE_PARSER_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteError.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace vix::note
{
  /**
   * @brief Options controlling how a Vix Note document is parsed.
   */
  struct NoteParseOptions
  {
    /**
     * @brief Automatically assign ids to cells that do not have one.
     *
     * This keeps old markdown-compatible notes usable even when they do not
     * contain Vix Note metadata comments.
     */
    bool assignCellIds = true;

    /**
     * @brief Read Vix Note cell metadata comments when present.
     *
     * Supported metadata comment format:
     * `<!-- vixnote:cell id="cell-1" kind="markdown" -->`
     *
     * Metadata comments are used to preserve stable cell ids across save/load
     * cycles. They are consumed by the parser and are not stored as markdown
     * cell source content.
     */
    bool readCellMetadata = true;

    /**
     * @brief Infer the document title from the first markdown heading.
     */
    bool inferTitle = true;
  };

  /**
   * @brief Diagnostic emitted while parsing a Vix Note document.
   *
   * Diagnostics are non-fatal messages. Fatal parser failures are returned
   * through NoteParseResult::error or thrown as NoteError by parse_or_throw().
   */
  struct NoteParseDiagnostic
  {
    /**
     * @brief One-based line number where the diagnostic was produced.
     */
    std::size_t line = 0;

    /**
     * @brief Human-readable diagnostic message.
     */
    std::string message;
  };

  /**
   * @brief Result returned by the Vix Note parser.
   */
  struct NoteParseResult
  {
    /**
     * @brief True when parsing completed successfully.
     */
    bool ok = false;

    /**
     * @brief Parsed note document.
     */
    NoteDocument document;

    /**
     * @brief Fatal parser error message when parsing fails.
     */
    std::string error;

    /**
     * @brief Non-fatal parser diagnostics.
     */
    std::vector<NoteParseDiagnostic> diagnostics;

    /**
     * @brief Checks whether the result contains diagnostics.
     *
     * @return True when diagnostics are present.
     */
    bool has_diagnostics() const noexcept;
  };

  /**
   * @brief Parses the lightweight Vix Note document format.
   *
   * Vix Note uses a markdown-compatible format where normal text is stored as
   * markdown cells and fenced blocks become executable or renderable cells.
   *
   * Supported fenced cell languages:
   * - `reply` or `repl`
   * - `cpp` or `c++`
   * - `html`
   *
   * Supported metadata comment:
   * - `<!-- vixnote:cell id="cell-1" kind="markdown" -->`
   *
   * Metadata comments allow the storage layer to preserve stable cell ids while
   * keeping `.vixnote` files readable as markdown.
   *
   * Any unknown fenced language is preserved as markdown.
   */
  class NoteParser
  {
  public:
    /**
     * @brief Creates a parser with default options.
     */
    NoteParser();

    /**
     * @brief Creates a parser with custom options.
     *
     * @param options Parser options.
     */
    explicit NoteParser(NoteParseOptions options);

    /**
     * @brief Returns the current parser options.
     *
     * @return Parser options.
     */
    const NoteParseOptions &options() const noexcept;

    /**
     * @brief Replaces the current parser options.
     *
     * @param options New parser options.
     */
    void set_options(NoteParseOptions options) noexcept;

    /**
     * @brief Parses a Vix Note document from text.
     *
     * @param source Document source text.
     * @return Parse result.
     */
    NoteParseResult parse(std::string_view source) const;

    /**
     * @brief Parses a Vix Note document or throws on failure.
     *
     * @param source Document source text.
     * @return Parsed document.
     *
     * @throws NoteError when parsing fails.
     */
    NoteDocument parse_or_throw(std::string_view source) const;

  private:
    /**
     * @brief Parser options.
     */
    NoteParseOptions options_;
  };

  /**
   * @brief Parses a Vix Note document using default options.
   *
   * @param source Document source text.
   * @return Parse result.
   */
  NoteParseResult parse_note(std::string_view source);

  /**
   * @brief Parses a Vix Note document using default options or throws.
   *
   * @param source Document source text.
   * @return Parsed document.
   *
   * @throws NoteError when parsing fails.
   */
  NoteDocument parse_note_or_throw(std::string_view source);
}

#endif // VIX_NOTE_PARSER_NOTE_PARSER_HPP
