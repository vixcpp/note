/**
 *
 *  @file NoteCell.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Cell model used by Vix Note documents.
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

#ifndef VIX_NOTE_CORE_NOTE_CELL_HPP
#define VIX_NOTE_CORE_NOTE_CELL_HPP

#include <vix/note/core/NoteResult.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace vix::note
{
  /**
   * @brief Type of content stored in a note cell.
   *
   * A Vix Note document is made of cells. Some cells are only rendered by the
   * UI, while others can be executed by the note runtime.
   */
  enum class NoteCellKind
  {
    /**
     * @brief Unknown or unsupported cell kind.
     */
    Unknown,

    /**
     * @brief Markdown documentation cell.
     */
    Markdown,

    /**
     * @brief Reply cell evaluated by the Vix Reply kernel.
     */
    Reply,

    /**
     * @brief C++ cell compiled and executed through the Vix runtime.
     */
    Cpp,

    /**
     * @brief Raw HTML cell rendered by the UI.
     */
    Html
  };

  /**
   * @brief Represents one editable and executable note cell.
   *
   * NoteCell stores the cell identity, type, source content, optional title,
   * execution count, and outputs produced by the last execution.
   */
  class NoteCell
  {
  public:
    /**
     * @brief Creates an empty unknown cell.
     */
    NoteCell();

    /**
     * @brief Creates a cell with a kind and source content.
     *
     * @param kind   Cell kind.
     * @param source Cell source content.
     */
    NoteCell(NoteCellKind kind, std::string source);

    /**
     * @brief Creates a cell with an id, kind, and source content.
     *
     * @param id     Stable cell id.
     * @param kind   Cell kind.
     * @param source Cell source content.
     */
    NoteCell(std::string id, NoteCellKind kind, std::string source);

    /**
     * @brief Creates a markdown cell.
     *
     * @param source Markdown content.
     * @return Created cell.
     */
    static NoteCell markdown(std::string source);

    /**
     * @brief Creates a Reply cell.
     *
     * @param source Reply code.
     * @return Created cell.
     */
    static NoteCell reply(std::string source);

    /**
     * @brief Creates a C++ cell.
     *
     * @param source C++ source code.
     * @return Created cell.
     */
    static NoteCell cpp(std::string source);

    /**
     * @brief Creates an HTML cell.
     *
     * @param source HTML content.
     * @return Created cell.
     */
    static NoteCell html(std::string source);

    /**
     * @brief Returns the stable cell id.
     *
     * @return Cell id.
     */
    const std::string &id() const noexcept;

    /**
     * @brief Returns the cell kind.
     *
     * @return Cell kind.
     */
    NoteCellKind kind() const noexcept;

    /**
     * @brief Returns the cell source content.
     *
     * @return Cell source.
     */
    const std::string &source() const noexcept;

    /**
     * @brief Returns the optional cell title.
     *
     * @return Cell title.
     */
    const std::string &title() const noexcept;

    /**
     * @brief Returns the last execution count.
     *
     * A value of 0 means the cell has not been executed yet.
     *
     * @return Execution count.
     */
    int execution_count() const noexcept;

    /**
     * @brief Returns the outputs produced by the cell.
     *
     * @return Read-only output list.
     */
    const std::vector<NoteOutput> &outputs() const noexcept;

    /**
     * @brief Changes the cell id.
     *
     * @param id New cell id.
     */
    void set_id(std::string id);

    /**
     * @brief Changes the cell kind.
     *
     * @param kind New cell kind.
     */
    void set_kind(NoteCellKind kind) noexcept;

    /**
     * @brief Changes the cell source content.
     *
     * @param source New source content.
     */
    void set_source(std::string source);

    /**
     * @brief Changes the optional cell title.
     *
     * @param title New title.
     */
    void set_title(std::string title);

    /**
     * @brief Sets the execution count.
     *
     * @param count Execution count. Values below 0 are normalized to 0.
     */
    void set_execution_count(int count) noexcept;

    /**
     * @brief Marks the cell as executed.
     *
     * @param count Execution count assigned to the cell.
     */
    void mark_executed(int count) noexcept;

    /**
     * @brief Clears the execution count.
     */
    void reset_execution() noexcept;

    /**
     * @brief Checks whether the cell source is empty.
     *
     * @return True when the source content is empty.
     */
    bool empty() const noexcept;

    /**
     * @brief Checks whether the cell can be executed.
     *
     * @return True for Reply and C++ cells.
     */
    bool executable() const noexcept;

    /**
     * @brief Checks whether the cell has outputs.
     *
     * @return True when at least one output is stored.
     */
    bool has_outputs() const noexcept;

    /**
     * @brief Adds one output to the cell.
     *
     * @param output Output to append.
     * @return Reference to this cell.
     */
    NoteCell &add_output(NoteOutput output);

    /**
     * @brief Replaces all outputs with a new output list.
     *
     * @param outputs New outputs.
     */
    void set_outputs(std::vector<NoteOutput> outputs);

    /**
     * @brief Removes all outputs from the cell.
     */
    void clear_outputs();

  private:
    /**
     * @brief Stable cell id.
     */
    std::string id_;

    /**
     * @brief Cell kind.
     */
    NoteCellKind kind_{NoteCellKind::Unknown};

    /**
     * @brief Cell source content.
     */
    std::string source_;

    /**
     * @brief Optional UI title.
     */
    std::string title_;

    /**
     * @brief Last execution count.
     */
    int executionCount_{0};

    /**
     * @brief Outputs produced by the cell.
     */
    std::vector<NoteOutput> outputs_;
  };

  /**
   * @brief Converts a NoteCellKind to a stable string name.
   *
   * @param kind Cell kind to convert.
   * @return String representation.
   */
  std::string_view to_string(NoteCellKind kind) noexcept;

  /**
   * @brief Parses a cell kind from a stable string name.
   *
   * @param value String value to parse.
   * @return Parsed cell kind, or Unknown when unsupported.
   */
  NoteCellKind note_cell_kind_from_string(std::string_view value) noexcept;

  /**
   * @brief Checks whether a cell kind can be executed.
   *
   * @param kind Cell kind.
   * @return True for Reply and C++ cells.
   */
  bool is_executable(NoteCellKind kind) noexcept;
}

#endif // VIX_NOTE_CORE_NOTE_CELL_HPP
