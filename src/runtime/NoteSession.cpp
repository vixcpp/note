/**
 *
 *  @file NoteSession.cpp
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

#include <vix/note/runtime/NoteSession.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vix::note
{
  NoteSession::NoteSession() = default;

  NoteSession::NoteSession(NoteDocument document)
      : document_(std::move(document))
  {
  }

  NoteSession::NoteSession(NoteDocument document, NoteSessionOptions options)
      : document_(std::move(document)),
        options_(options)
  {
  }

  const NoteSessionOptions &NoteSession::options() const noexcept
  {
    return options_;
  }

  void NoteSession::set_options(NoteSessionOptions options) noexcept
  {
    options_ = options;
  }

  const NoteDocument &NoteSession::document() const noexcept
  {
    return document_;
  }

  NoteDocument &NoteSession::document() noexcept
  {
    return document_;
  }

  void NoteSession::set_document(NoteDocument document)
  {
    document_ = std::move(document);
    records_.clear();
  }

  bool NoteSession::empty() const noexcept
  {
    return document_.empty();
  }

  std::size_t NoteSession::cell_count() const noexcept
  {
    return document_.cell_count();
  }

  bool NoteSession::has_cell(std::size_t index) const noexcept
  {
    return document_.cell_at(index) != nullptr;
  }

  bool NoteSession::can_execute_cell(std::size_t index) const noexcept
  {
    const NoteCell *cell = document_.cell_at(index);

    if (cell == nullptr)
    {
      return false;
    }

    return cell->executable();
  }

  NoteCell *NoteSession::cell_at(std::size_t index) noexcept
  {
    return document_.cell_at(index);
  }

  const NoteCell *NoteSession::cell_at(std::size_t index) const noexcept
  {
    return document_.cell_at(index);
  }

  NoteCell *NoteSession::find_cell(const std::string &id) noexcept
  {
    return document_.find_cell(id);
  }

  const NoteCell *NoteSession::find_cell(const std::string &id) const noexcept
  {
    return document_.find_cell(id);
  }

  std::optional<std::size_t> NoteSession::cell_index(const std::string &id) const noexcept
  {
    return document_.cell_index(id);
  }

  NoteResult NoteSession::apply_result(std::size_t index, const NoteResult &result)
  {
    NoteCell *cell = document_.cell_at(index);

    if (cell == nullptr)
    {
      return NoteResult::failure("cell index out of range", 1)
          .add_error("cell index out of range");
    }

    if (!cell->executable() && !options_.allowDynamicCellResults)
    {
      return NoteResult::failure("cell is not executable", 1)
          .add_error("cell is not executable");
    }

    if (options_.clearOutputsBeforeRun)
    {
      cell->clear_outputs();
    }

    for (const NoteOutput &output : result.outputs())
    {
      cell->add_output(output);
    }

    const int executionCount =
        document_.next_execution_count();

    cell->mark_executed(executionCount);

    NoteSessionRecord record;
    record.cellIndex = index;
    record.cellId = cell->id();
    record.executionCount = executionCount;
    record.result = result;

    records_.push_back(std::move(record));

    if (result.failed())
    {
      return NoteResult::failure("cell result applied with failure", result.exit_code());
    }

    if (result.was_skipped())
    {
      return NoteResult::skipped("cell result applied as skipped");
    }

    return NoteResult::success("cell result applied");
  }

  NoteResult NoteSession::apply_result(const std::string &id, const NoteResult &result)
  {
    const std::optional<std::size_t> index =
        document_.cell_index(id);

    if (!index)
    {
      return NoteResult::failure("cell not found: " + id, 1)
          .add_error("cell not found: " + id);
    }

    return apply_result(*index, result);
  }

  void NoteSession::clear_outputs()
  {
    document_.clear_outputs();
  }

  void NoteSession::reset_execution()
  {
    document_.reset_execution_count();

    for (NoteCell &cell : document_.cells())
    {
      cell.reset_execution();
    }
  }

  void NoteSession::clear_records()
  {
    records_.clear();
  }

  const std::vector<NoteSessionRecord> &NoteSession::records() const noexcept
  {
    return records_;
  }

  bool NoteSession::has_records() const noexcept
  {
    return !records_.empty();
  }
}
