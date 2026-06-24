/**
 *
 *  @file NoteKernel.hpp
 *  @author Gaspard Kirira
 *
 *  @brief High-level execution kernel for Vix Note documents.
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

#ifndef VIX_NOTE_RUNTIME_NOTE_KERNEL_HPP
#define VIX_NOTE_RUNTIME_NOTE_KERNEL_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/runtime/CppCellRunner.hpp>
#include <vix/note/runtime/NoteSession.hpp>
#include <vix/note/runtime/ReplyCellRunner.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vix::note
{
  /**
   * @brief Options controlling the Vix Note execution kernel.
   */
  struct NoteKernelOptions
  {
    /**
     * @brief Session behavior used when applying cell results.
     */
    NoteSessionOptions sessionOptions;

    /**
     * @brief C++ runner options used for C++ cells.
     */
    CppCellRunnerOptions cppOptions;

    /**
     * @brief Reply runner options used for Reply cells.
     */
    ReplyCellRunnerOptions replyOptions;

    /**
     * @brief Stops run_all() after the first failed executable cell.
     */
    bool stopOnFirstFailure = false;

    /**
     * @brief Includes markdown and HTML cells as skipped results during run_all().
     *
     * When false, non-executable cells are only counted as visited and do not
     * produce NoteResult entries.
     */
    bool includeNonExecutableAsSkipped = false;
  };

  /**
   * @brief Result returned by multi-cell kernel execution.
   */
  struct NoteKernelRunResult
  {
    /**
     * @brief True when all executed cells completed without failure.
     */
    bool ok = true;

    /**
     * @brief True when execution stopped early.
     */
    bool stopped = false;

    /**
     * @brief Number of cells visited by the run.
     */
    std::size_t visited = 0;

    /**
     * @brief Number of executable cells run or applied.
     */
    std::size_t executed = 0;

    /**
     * @brief Number of skipped cells included in the result list.
     */
    std::size_t skipped = 0;

    /**
     * @brief Number of failed executable cells.
     */
    std::size_t failed = 0;

    /**
     * @brief Results produced while running cells.
     */
    std::vector<NoteResult> results;

    /**
     * @brief Checks whether at least one cell failed.
     *
     * @return True when one result is a failure.
     */
    bool has_failures() const noexcept;

    /**
     * @brief Checks whether at least one cell was skipped.
     *
     * @return True when one result is skipped.
     */
    bool has_skipped() const noexcept;

    /**
     * @brief Checks whether the run produced any results.
     *
     * @return True when the result list is not empty.
     */
    bool has_results() const noexcept;
  };

  /**
   * @brief High-level runtime kernel for Vix Note.
   *
   * NoteKernel owns a NoteSession and coordinates cell execution. It delegates
   * C++ cells to CppCellRunner and updates the session with produced outputs,
   * execution counts, and execution records.
   */
  class NoteKernel
  {
  public:
    /**
     * @brief Creates an empty kernel with default options.
     */
    NoteKernel();

    /**
     * @brief Creates a kernel for an existing document.
     *
     * @param document Document to execute.
     */
    explicit NoteKernel(NoteDocument document);

    /**
     * @brief Creates a kernel with custom options.
     *
     * @param options Kernel options.
     */
    explicit NoteKernel(NoteKernelOptions options);

    /**
     * @brief Creates a kernel for an existing document with custom options.
     *
     * @param document Document to execute.
     * @param options  Kernel options.
     */
    NoteKernel(NoteDocument document, NoteKernelOptions options);

    /**
     * @brief Returns the current kernel options.
     *
     * @return Kernel options.
     */
    const NoteKernelOptions &options() const noexcept;

    /**
     * @brief Replaces the current kernel options.
     *
     * @param options New kernel options.
     */
    void set_options(NoteKernelOptions options);

    /**
     * @brief Returns the runtime session.
     *
     * @return Read-only session.
     */
    const NoteSession &session() const noexcept;

    /**
     * @brief Returns the mutable runtime session.
     *
     * @return Mutable session.
     */
    NoteSession &session() noexcept;

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
     * @brief Replaces the kernel document and clears execution records.
     *
     * @param document New document.
     */
    void set_document(NoteDocument document);

    /**
     * @brief Returns the number of cells in the current document.
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
     * @brief Checks whether a cell can be executed.
     *
     * @param id Cell id.
     * @return True when the cell exists and is executable.
     */
    bool can_execute_cell(const std::string &id) const noexcept;

    /**
     * @brief Finds the index of a cell by id.
     *
     * @param id Cell id.
     * @return Cell index, or std::nullopt when not found.
     */
    std::optional<std::size_t> cell_index(const std::string &id) const noexcept;

    /**
     * @brief Runs a single cell by index.
     *
     * Markdown and HTML cells are returned as skipped because they are rendered
     * by the UI and not executed by the kernel.
     *
     * @param index Cell index.
     * @return Cell execution result.
     */
    NoteResult run_cell(std::size_t index);

    /**
     * @brief Runs a single cell by id.
     *
     * @param id Cell id.
     * @return Cell execution result.
     */
    NoteResult run_cell(const std::string &id);

    /**
     * @brief Runs all cells in document order.
     *
     * @return Multi-cell execution result.
     */
    NoteKernelRunResult run_all();

    /**
     * @brief Runs only executable cells in document order.
     *
     * @return Multi-cell execution result.
     */
    NoteKernelRunResult run_executable_cells();

    /**
     * @brief Clears all outputs from the current document.
     */
    void clear_outputs();

    /**
     * @brief Resets execution counts and clears execution records.
     */
    void reset_execution();

    /**
     * @brief Clears outputs, execution counts, and execution records.
     */
    void reset();

  private:
    /**
     * @brief Runs a Reply cell.
     *
     * @param cell Cell to run.
     * @return Execution result.
     */
    NoteResult run_reply_cell(const NoteCell &cell);

    /**
     * @brief Runs a C++ cell.
     *
     * @param cell Cell to run.
     * @return Execution result.
     */
    NoteResult run_cpp_cell_internal(const NoteCell &cell);

    /**
     * @brief Applies current options to internal runtime components.
     */
    void sync_options();

    /**
     * @brief Kernel options.
     */
    NoteKernelOptions options_;

    /**
     * @brief Runtime session.
     */
    NoteSession session_;

    /**
     * @brief C++ cell runner.
     */
    CppCellRunner cppRunner_;

    /**
     * @brief Reply cell runner.
     */
    ReplyCellRunner replyRunner_;
  };

  /**
   * @brief Runs all cells in a document using default kernel options.
   *
   * @param document Document to execute.
   * @return Multi-cell execution result.
   */
  NoteKernelRunResult run_note(NoteDocument document);

  /**
   * @brief Runs one cell in a document using default kernel options.
   *
   * @param document Document to execute.
   * @param index    Cell index.
   * @return Cell execution result.
   */
  NoteResult run_note_cell(NoteDocument document, std::size_t index);

  /**
   * @brief Runs one cell in a document using default kernel options.
   *
   * @param document Document to execute.
   * @param id       Cell id.
   * @return Cell execution result.
   */
  NoteResult run_note_cell(NoteDocument document, const std::string &id);
}

#endif // VIX_NOTE_RUNTIME_NOTE_KERNEL_HPP
