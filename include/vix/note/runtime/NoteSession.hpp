/**
 *
 *  @file NoteSession.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Runtime session state for Vix Note documents.
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

#ifndef VIX_NOTE_RUNTIME_NOTE_SESSION_HPP
#define VIX_NOTE_RUNTIME_NOTE_SESSION_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vix::note
{
  /**
   * @brief Options controlling runtime session behavior.
   */
  struct NoteSessionOptions
  {
    /**
     * @brief Clears previous outputs before applying a new result to a cell.
     */
    bool clearOutputsBeforeRun = true;

    /**
     * @brief Allows a higher-level kernel to apply results for dynamically
     * registered extension cell types.
     *
     * Direct NoteSession usage remains conservative and rejects results for
     * cells that are not executable according to their built-in kind.
     */
    bool allowDynamicCellResults = false;

    /**
     * @brief Stops multi-cell execution after the first failure.
     *
     * This option is stored at session level and is mainly consumed by higher
     * level runners such as NoteKernel.
     */
    bool stopOnFirstFailure = false;
  };

  /**
   * @brief Record describing one executed cell.
   */
  struct NoteSessionRecord
  {
    /**
     * @brief Zero-based cell index in the document.
     */
    std::size_t cellIndex = 0;

    /**
     * @brief Cell id at execution time.
     */
    std::string cellId;

    /**
     * @brief Execution count assigned to the cell.
     */
    int executionCount = 0;

    /**
     * @brief Result produced by the execution.
     */
    NoteResult result;
  };

  /**
   * @brief Holds the mutable runtime state of one note document.
   *
   * NoteSession owns a NoteDocument and tracks execution history. It does not
   * compile or run code by itself. Higher-level runtime components execute
   * cells, then call apply_result() to update the document state.
   */
  class NoteSession
  {
  public:
    /**
     * @brief Creates an empty session.
     */
    NoteSession();

    /**
     * @brief Creates a session for an existing document.
     *
     * @param document Document owned by the session.
     */
    explicit NoteSession(NoteDocument document);

    /**
     * @brief Creates a session with custom options.
     *
     * @param document Document owned by the session.
     * @param options  Session options.
     */
    NoteSession(NoteDocument document, NoteSessionOptions options);

    /**
     * @brief Returns the current session options.
     *
     * @return Session options.
     */
    const NoteSessionOptions &options() const noexcept;

    /**
     * @brief Replaces the current session options.
     *
     * @param options New options.
     */
    void set_options(NoteSessionOptions options) noexcept;

    /**
     * @brief Returns the session document.
     *
     * @return Read-only document.
     */
    const NoteDocument &document() const noexcept;

    /**
     * @brief Returns the mutable session document.
     *
     * @return Mutable document.
     */
    NoteDocument &document() noexcept;

    /**
     * @brief Replaces the session document and clears execution records.
     *
     * @param document New document.
     */
    void set_document(NoteDocument document);

    /**
     * @brief Checks whether the session document has no cells.
     *
     * @return True when no cells are stored.
     */
    bool empty() const noexcept;

    /**
     * @brief Returns the number of cells in the session document.
     *
     * @return Cell count.
     */
    std::size_t cell_count() const noexcept;

    /**
     * @brief Checks whether a cell index exists.
     *
     * @param index Cell index.
     * @return True when the index is valid.
     */
    bool has_cell(std::size_t index) const noexcept;

    /**
     * @brief Checks whether a cell can be executed.
     *
     * @param index Cell index.
     * @return True when the cell exists and is executable.
     */
    bool can_execute_cell(std::size_t index) const noexcept;

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

    /**
     * @brief Applies an execution result to a cell.
     *
     * The cell receives the result outputs and gets a new execution count when
     * it is executable. A session record is also stored.
     *
     * @param index  Cell index.
     * @param result Execution result.
     * @return Result describing whether the update succeeded.
     */
    NoteResult apply_result(std::size_t index, const NoteResult &result);

    /**
     * @brief Applies an execution result to a cell by id.
     *
     * @param id     Cell id.
     * @param result Execution result.
     * @return Result describing whether the update succeeded.
     */
    NoteResult apply_result(const std::string &id, const NoteResult &result);

    /**
     * @brief Clears all cell outputs in the document.
     */
    void clear_outputs();

    /**
     * @brief Resets all cell execution counts and document execution counter.
     */
    void reset_execution();

    /**
     * @brief Clears execution records.
     */
    void clear_records();

    /**
     * @brief Returns execution records.
     *
     * @return Read-only execution records.
     */
    const std::vector<NoteSessionRecord> &records() const noexcept;

    /**
     * @brief Checks whether the session has execution records.
     *
     * @return True when at least one record is stored.
     */
    bool has_records() const noexcept;

  private:
    /**
     * @brief Document owned by the session.
     */
    NoteDocument document_;

    /**
     * @brief Session options.
     */
    NoteSessionOptions options_;

    /**
     * @brief Execution records.
     */
    std::vector<NoteSessionRecord> records_;
  };
}

#endif // VIX_NOTE_RUNTIME_NOTE_SESSION_HPP
