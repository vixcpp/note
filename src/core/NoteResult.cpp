/**
 *
 *  @file NoteResult.cpp
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

#include <vix/note/core/NoteResult.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace vix::note
{
  NoteOutput NoteOutput::text(std::string content)
  {
    return {NoteOutputKind::Text, std::move(content)};
  }

  NoteOutput NoteOutput::stdout_text(std::string content)
  {
    return {NoteOutputKind::Stdout, std::move(content)};
  }

  NoteOutput NoteOutput::stderr_text(std::string content)
  {
    return {NoteOutputKind::Stderr, std::move(content)};
  }

  NoteOutput NoteOutput::html(std::string content)
  {
    return {NoteOutputKind::Html, std::move(content)};
  }

  NoteOutput NoteOutput::error(std::string content)
  {
    return {NoteOutputKind::Error, std::move(content)};
  }

  NoteOutput NoteOutput::compiler_error(std::string content)
  {
    return {NoteOutputKind::CompilerError, std::move(content)};
  }

  NoteOutput NoteOutput::runtime_error(std::string content)
  {
    return {NoteOutputKind::RuntimeError, std::move(content)};
  }

  NoteOutput NoteOutput::debug(std::string content)
  {
    return {NoteOutputKind::Debug, std::move(content)};
  }

  NoteOutput NoteOutput::hint(std::string content)
  {
    return {NoteOutputKind::Hint, std::move(content)};
  }

  NoteOutput NoteOutput::raw_log(std::string content)
  {
    return {NoteOutputKind::RawLog, std::move(content)};
  }

  bool NoteOutput::empty() const noexcept
  {
    return content.empty();
  }

  NoteResult NoteResult::success(std::string message)
  {
    return NoteResult(
        NoteResultStatus::Success,
        std::move(message),
        0);
  }

  NoteResult NoteResult::failure(std::string message, int exitCode)
  {
    return NoteResult(
        NoteResultStatus::Failure,
        std::move(message),
        exitCode);
  }

  NoteResult NoteResult::skipped(std::string message)
  {
    return NoteResult(
        NoteResultStatus::Skipped,
        std::move(message),
        0);
  }

  NoteResult::NoteResult(
      NoteResultStatus status,
      std::string message,
      int exitCode)
      : status_(status),
        message_(std::move(message)),
        exitCode_(exitCode)
  {
  }

  NoteResultStatus NoteResult::status() const noexcept
  {
    return status_;
  }

  int NoteResult::exit_code() const noexcept
  {
    return exitCode_;
  }

  const std::string &NoteResult::message() const noexcept
  {
    return message_;
  }

  const std::vector<NoteOutput> &NoteResult::outputs() const noexcept
  {
    return outputs_;
  }

  bool NoteResult::ok() const noexcept
  {
    return status_ == NoteResultStatus::Success;
  }

  bool NoteResult::failed() const noexcept
  {
    return status_ == NoteResultStatus::Failure;
  }

  bool NoteResult::was_skipped() const noexcept
  {
    return status_ == NoteResultStatus::Skipped;
  }

  bool NoteResult::has_outputs() const noexcept
  {
    return !outputs_.empty();
  }

  NoteResult &NoteResult::add_output(NoteOutput output)
  {
    outputs_.push_back(std::move(output));
    return *this;
  }

  NoteResult &NoteResult::add_text(std::string content)
  {
    return add_output(NoteOutput::text(std::move(content)));
  }

  NoteResult &NoteResult::add_stdout(std::string content)
  {
    return add_output(NoteOutput::stdout_text(std::move(content)));
  }

  NoteResult &NoteResult::add_stderr(std::string content)
  {
    return add_output(NoteOutput::stderr_text(std::move(content)));
  }

  NoteResult &NoteResult::add_html(std::string content)
  {
    return add_output(NoteOutput::html(std::move(content)));
  }

  NoteResult &NoteResult::add_error(std::string content)
  {
    return add_output(NoteOutput::error(std::move(content)));
  }

  NoteResult &NoteResult::add_compiler_error(std::string content)
  {
    return add_output(NoteOutput::compiler_error(std::move(content)));
  }

  NoteResult &NoteResult::add_runtime_error(std::string content)
  {
    return add_output(NoteOutput::runtime_error(std::move(content)));
  }

  NoteResult &NoteResult::add_debug(std::string content)
  {
    return add_output(NoteOutput::debug(std::move(content)));
  }

  NoteResult &NoteResult::add_hint(std::string content)
  {
    return add_output(NoteOutput::hint(std::move(content)));
  }

  NoteResult &NoteResult::add_raw_log(std::string content)
  {
    return add_output(NoteOutput::raw_log(std::move(content)));
  }

  std::string_view to_string(NoteResultStatus status) noexcept
  {
    switch (status)
    {
    case NoteResultStatus::Success:
      return "success";

    case NoteResultStatus::Failure:
      return "failure";

    case NoteResultStatus::Skipped:
      return "skipped";
    }

    return "failure";
  }

  std::string_view to_string(NoteOutputKind kind) noexcept
  {
    switch (kind)
    {
    case NoteOutputKind::Text:
      return "text";

    case NoteOutputKind::Stdout:
      return "stdout";

    case NoteOutputKind::Stderr:
      return "stderr";

    case NoteOutputKind::Html:
      return "html";

    case NoteOutputKind::Error:
      return "error";

    case NoteOutputKind::CompilerError:
      return "compiler_error";

    case NoteOutputKind::RuntimeError:
      return "runtime_error";

    case NoteOutputKind::Debug:
      return "debug";

    case NoteOutputKind::Hint:
      return "hint";

    case NoteOutputKind::RawLog:
      return "raw_log";
    }

    return "text";
  }
}
