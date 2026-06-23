/**
 *
 *  @file NoteDocument.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Document model used by Vix Note notebooks.
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

#ifndef VIX_NOTE_CORE_NOTE_DOCUMENT_HPP
#define VIX_NOTE_CORE_NOTE_DOCUMENT_HPP

#include <vix/note/core/NoteCell.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vix::note
{
  /**
   * @brief Represents one Vix Note document.
   *
   * A NoteDocument is a lightweight notebook model. It stores document
   * metadata, an ordered list of cells, and a document-level execution counter
   * used to assign execution numbers to executable cells.
   */
  class NoteDocument
  {
  public:
    /**
     * @brief Creates an empty note document.
     */
    NoteDocument();

    /**
     * @brief Creates an empty note document with a title.
     *
     * @param title Document title.
     */
    explicit NoteDocument(std::string title);

    /**
     * @brief Creates an empty note document.
     *
     * @param title Optional document title.
     * @return Created document.
     */
    static NoteDocument create(std::string title = {});

    /**
     * @brief Returns the stable document id.
     *
     * @return Document id.
     */
    const std::string &id() const noexcept;

    /**
     * @brief Returns the document title.
     *
     * @return Document title.
     */
    const std::string &title() const noexcept;

    /**
     * @brief Returns the source path associated with this document.
     *
     * The path is optional and is usually filled by the storage layer.
     *
     * @return Document path string.
     */
    const std::string &path() const noexcept;

    /**
     * @brief Returns all document cells.
     *
     * @return Read-only cell list.
     */
    const std::vector<NoteCell> &cells() const noexcept;

    /**
     * @brief Returns the mutable list of document cells.
     *
     * @return Mutable cell list.
     */
    std::vector<NoteCell> &cells() noexcept;

    /**
     * @brief Returns the number of cells in the document.
     *
     * @return Cell count.
     */
    std::size_t cell_count() const noexcept;

    /**
     * @brief Returns the current document execution counter.
     *
     * @return Execution counter.
     */
    int execution_count() const noexcept;

    /**
     * @brief Checks whether the document has no cells.
     *
     * @return True when no cells are stored.
     */
    bool empty() const noexcept;

    /**
     * @brief Checks whether the document has a non-empty title.
     *
     * @return True when the title is not empty.
     */
    bool has_title() const noexcept;

    /**
     * @brief Checks whether the document contains executable cells.
     *
     * @return True when at least one cell can be executed.
     */
    bool has_executable_cells() const noexcept;

    /**
     * @brief Returns the number of executable cells.
     *
     * @return Number of Reply and C++ cells.
     */
    std::size_t executable_cell_count() const noexcept;

    /**
     * @brief Changes the document id.
     *
     * @param id New document id.
     */
    void set_id(std::string id);

    /**
     * @brief Changes the document title.
     *
     * @param title New document title.
     */
    void set_title(std::string title);

    /**
     * @brief Changes the source path associated with this document.
     *
     * @param path New document path.
     */
    void set_path(std::string path);

    /**
     * @brief Sets the document execution counter.
     *
     * @param count New execution counter. Values below 0 are normalized to 0.
     */
    void set_execution_count(int count) noexcept;

    /**
     * @brief Returns the next execution count and increments the counter.
     *
     * @return Next execution count.
     */
    int next_execution_count() noexcept;

    /**
     * @brief Resets the document execution counter.
     */
    void reset_execution_count() noexcept;

    /**
     * @brief Adds a cell to the end of the document.
     *
     * @param cell Cell to append.
     * @return Reference to the appended cell.
     */
    NoteCell &add_cell(NoteCell cell);

    /**
     * @brief Adds a markdown cell to the document.
     *
     * @param source Markdown content.
     * @return Reference to the appended cell.
     */
    NoteCell &add_markdown(std::string source);

    /**
     * @brief Adds a Reply cell to the document.
     *
     * @param source Reply code.
     * @return Reference to the appended cell.
     */
    NoteCell &add_reply(std::string source);

    /**
     * @brief Adds a C++ cell to the document.
     *
     * @param source C++ source code.
     * @return Reference to the appended cell.
     */
    NoteCell &add_cpp(std::string source);

    /**
     * @brief Adds an HTML cell to the document.
     *
     * @param source HTML content.
     * @return Reference to the appended cell.
     */
    NoteCell &add_html(std::string source);

    /**
     * @brief Inserts a cell at a specific index.
     *
     * @param index Target index.
     * @param cell  Cell to insert.
     * @return True on success, false when the index is out of range.
     */
    bool insert_cell(std::size_t index, NoteCell cell);

    /**
     * @brief Removes a cell by index.
     *
     * @param index Cell index.
     * @return True on success, false when the index is out of range.
     */
    bool remove_cell(std::size_t index);

    /**
     * @brief Removes a cell by id.
     *
     * @param id Cell id.
     * @return True when a matching cell was removed.
     */
    bool remove_cell_by_id(const std::string &id);

    /**
     * @brief Removes all cells from the document.
     */
    void clear_cells();

    /**
     * @brief Clears all cell outputs.
     */
    void clear_outputs();

    /**
     * @brief Returns a mutable cell by index.
     *
     * @param index Cell index.
     * @return Pointer to the cell, or nullptr when out of range.
     */
    NoteCell *cell_at(std::size_t index) noexcept;

    /**
     * @brief Returns a read-only cell by index.
     *
     * @param index Cell index.
     * @return Pointer to the cell, or nullptr when out of range.
     */
    const NoteCell *cell_at(std::size_t index) const noexcept;

    /**
     * @brief Finds a mutable cell by id.
     *
     * @param id Cell id.
     * @return Pointer to the cell, or nullptr when not found.
     */
    NoteCell *find_cell(const std::string &id) noexcept;

    /**
     * @brief Finds a read-only cell by id.
     *
     * @param id Cell id.
     * @return Pointer to the cell, or nullptr when not found.
     */
    const NoteCell *find_cell(const std::string &id) const noexcept;

    /**
     * @brief Finds the index of a cell by id.
     *
     * @param id Cell id.
     * @return Cell index, or std::nullopt when not found.
     */
    std::optional<std::size_t> cell_index(const std::string &id) const noexcept;

  private:
    /**
     * @brief Stable document id.
     */
    std::string id_;

    /**
     * @brief Human-readable document title.
     */
    std::string title_;

    /**
     * @brief Optional source path.
     */
    std::string path_;

    /**
     * @brief Ordered list of document cells.
     */
    std::vector<NoteCell> cells_;

    /**
     * @brief Document execution counter.
     */
    int executionCount_{0};
  };
}

#endif // VIX_NOTE_CORE_NOTE_DOCUMENT_HPP
