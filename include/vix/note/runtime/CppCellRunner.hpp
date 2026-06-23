/**
 *
 *  @file CppCellRunner.hpp
 *  @author Gaspard Kirira
 *
 *  @brief C++ cell execution helper for Vix Note.
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

#ifndef VIX_NOTE_RUNTIME_CPP_CELL_RUNNER_HPP
#define VIX_NOTE_RUNTIME_CPP_CELL_RUNNER_HPP

#include <vix/note/core/NoteCell.hpp>
#include <vix/note/core/NoteResult.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace vix::note
{
  /**
   * @brief Options used when running a C++ note cell.
   *
   * CppCellRunner delegates actual C++ execution to the Vix CLI by writing
   * the cell source to a temporary `.cpp` file and running `vix run <file>`.
   */
  struct CppCellRunnerOptions
  {
    /**
     * @brief Command used to invoke the Vix CLI.
     */
    std::string vixCommand = "vix";

    /**
     * @brief Optional working directory used when running the cell.
     *
     * When empty, the current process directory is used.
     */
    std::filesystem::path workingDirectory;

    /**
     * @brief Optional temporary root directory.
     *
     * When empty, the system temporary directory is used.
     */
    std::filesystem::path temporaryDirectory;

    /**
     * @brief Extra arguments passed to `vix run` after the generated file path.
     */
    std::vector<std::string> runArgs;

    /**
     * @brief Keeps the generated temporary C++ file after execution.
     */
    bool keepTemporaryFile = false;
  };

  /**
   * @brief Executes C++ cells by delegating to `vix run`.
   *
   * The runner is intentionally small. It does not implement a new C++
   * compiler pipeline. It reuses Vix CLI behavior so note execution stays
   * aligned with normal Vix script execution.
   */
  class CppCellRunner
  {
  public:
    /**
     * @brief Creates a runner with default options.
     */
    CppCellRunner();

    /**
     * @brief Creates a runner with custom options.
     *
     * @param options Runner options.
     */
    explicit CppCellRunner(CppCellRunnerOptions options);

    /**
     * @brief Returns the current runner options.
     *
     * @return Runner options.
     */
    const CppCellRunnerOptions &options() const noexcept;

    /**
     * @brief Replaces the current runner options.
     *
     * @param options New runner options.
     */
    void set_options(CppCellRunnerOptions options) noexcept;

    /**
     * @brief Runs raw C++ source code.
     *
     * @param source C++ source code.
     * @return Execution result.
     */
    NoteResult run_source(const std::string &source) const;

    /**
     * @brief Runs a C++ note cell.
     *
     * @param cell Cell to execute.
     * @return Execution result.
     */
    NoteResult run_cell(const NoteCell &cell) const;

  private:
    /**
     * @brief Runner options.
     */
    CppCellRunnerOptions options_;
  };

  /**
   * @brief Runs raw C++ source code using default runner options.
   *
   * @param source C++ source code.
   * @return Execution result.
   */
  NoteResult run_cpp_source(const std::string &source);

  /**
   * @brief Runs a C++ cell using default runner options.
   *
   * @param cell Cell to execute.
   * @return Execution result.
   */
  NoteResult run_cpp_cell(const NoteCell &cell);
}

#endif // VIX_NOTE_RUNTIME_CPP_CELL_RUNNER_HPP
