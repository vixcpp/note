/**
 *
 *  @file ReplyCellRunner.cpp
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

#include <vix/note/runtime/ReplyCellRunner.hpp>

#include <string>
#include <utility>

namespace vix::note
{
  namespace
  {
    void add_reply_outputs(
        NoteResult &result,
        const ReplyCellRunnerOptions &options,
        const vix::reply::ReplyRunResult &replyResult)
    {
      if (!replyResult.stdoutText.empty())
      {
        result.add_stdout(replyResult.stdoutText);
      }

      if (!replyResult.stderrText.empty())
      {
        result.add_stderr(replyResult.stderrText);
      }

      if (!replyResult.errorText.empty())
      {
        result.add_error(replyResult.errorText);
      }

      if (options.debugMode)
      {
        result.add_debug("duration_ms=" + std::to_string(replyResult.elapsedMs));
        result.add_debug("exit_code=" + std::to_string(replyResult.exitCode));
      }
    }
  }

  ReplyCellRunner::ReplyCellRunner()
      : runtime_(options_.runtimeOptions)
  {
    runtime_.set_args(options_.args);
  }

  ReplyCellRunner::ReplyCellRunner(ReplyCellRunnerOptions options)
      : options_(std::move(options)),
        runtime_(options_.runtimeOptions)
  {
    runtime_.set_args(options_.args);
  }

  const ReplyCellRunnerOptions &ReplyCellRunner::options() const noexcept
  {
    return options_;
  }

  void ReplyCellRunner::set_options(ReplyCellRunnerOptions options)
  {
    options_ = std::move(options);
    reset_runtime();
  }

  NoteResult ReplyCellRunner::run_source(const std::string &source)
  {
    if (source.empty())
    {
      return NoteResult::failure("empty Reply cell", 1)
          .add_error("empty Reply cell");
    }

    vix::reply::ReplyRunResult replyResult =
        runtime_.run_cell(source);

    NoteResult result =
        replyResult.ok
            ? NoteResult::success("Reply cell executed")
            : NoteResult::failure("Reply cell failed", replyResult.exitCode == 0 ? 1 : replyResult.exitCode);

    add_reply_outputs(result, options_, replyResult);

    return result;
  }

  NoteResult ReplyCellRunner::run_cell(const NoteCell &cell)
  {
    if (cell.kind() != NoteCellKind::Reply)
    {
      return NoteResult::failure("cell is not a Reply cell", 1)
          .add_error("cell is not a Reply cell");
    }

    return run_source(cell.source());
  }

  void ReplyCellRunner::clear()
  {
    runtime_.clear();
  }

  void ReplyCellRunner::reset_runtime()
  {
    runtime_ = vix::reply::ReplyRuntime(options_.runtimeOptions);
    runtime_.set_args(options_.args);
  }

  NoteResult run_reply_source(const std::string &source)
  {
    return ReplyCellRunner().run_source(source);
  }

  NoteResult run_reply_cell(const NoteCell &cell)
  {
    return ReplyCellRunner().run_cell(cell);
  }
}
