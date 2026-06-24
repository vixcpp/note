/**
 *
 *  @file ReplyCellRunner.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Reply cell execution helper for Vix Note.
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

#ifndef VIX_NOTE_RUNTIME_REPLY_CELL_RUNNER_HPP
#define VIX_NOTE_RUNTIME_REPLY_CELL_RUNNER_HPP

#include <vix/note/core/NoteCell.hpp>
#include <vix/note/core/NoteResult.hpp>

#include <vix/reply/core/ReplyRuntime.hpp>

#include <string>
#include <utility>
#include <vector>

namespace vix::note
{
  /**
   * @brief Options used when running a Reply note cell.
   *
   * ReplyCellRunner executes small Reply cells through the embeddable
   * Vix Reply runtime. It does not start the interactive terminal REPL.
   */
  struct ReplyCellRunnerOptions
  {
    /**
     * @brief Runtime options passed to the embedded Reply runtime.
     */
    vix::reply::ReplyRuntimeOptions runtimeOptions;

    /**
     * @brief Arguments exposed through Vix.args().
     */
    std::vector<std::string> args;

    /**
     * @brief Enables debug metadata outputs.
     */
    bool debugMode = false;
  };

  /**
   * @brief Executes Reply cells through the embedded Reply runtime.
   */
  class ReplyCellRunner
  {
  public:
    /**
     * @brief Creates a runner with default options.
     */
    ReplyCellRunner();

    /**
     * @brief Creates a runner with custom options.
     *
     * @param options Runner options.
     */
    explicit ReplyCellRunner(ReplyCellRunnerOptions options);

    /**
     * @brief Returns the current runner options.
     *
     * @return Runner options.
     */
    const ReplyCellRunnerOptions &options() const noexcept;

    /**
     * @brief Replaces the current runner options.
     *
     * @param options New runner options.
     */
    void set_options(ReplyCellRunnerOptions options);

    /**
     * @brief Runs raw Reply source code.
     *
     * @param source Reply source code.
     * @return Execution result.
     */
    NoteResult run_source(const std::string &source);

    /**
     * @brief Runs a Reply note cell.
     *
     * @param cell Cell to execute.
     * @return Execution result.
     */
    NoteResult run_cell(const NoteCell &cell);

    /**
     * @brief Clears the embedded Reply runtime state.
     */
    void clear();

  private:
    /**
     * @brief Rebuilds the embedded runtime from current options.
     */
    void reset_runtime();

    /**
     * @brief Runner options.
     */
    ReplyCellRunnerOptions options_;

    /**
     * @brief Embedded Reply runtime.
     */
    vix::reply::ReplyRuntime runtime_;
  };

  /**
   * @brief Runs raw Reply source code using default runner options.
   *
   * @param source Reply source code.
   * @return Execution result.
   */
  NoteResult run_reply_source(const std::string &source);

  /**
   * @brief Runs a Reply cell using default runner options.
   *
   * @param cell Cell to execute.
   * @return Execution result.
   */
  NoteResult run_reply_cell(const NoteCell &cell);
}

#endif // VIX_NOTE_RUNTIME_REPLY_CELL_RUNNER_HPP
