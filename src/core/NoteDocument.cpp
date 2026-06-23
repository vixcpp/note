/**
 *
 *  @file NoteDocument.cpp
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

#include <vix/note/core/NoteDocument.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vix::note
{
  NoteDocument::NoteDocument() = default;

  NoteDocument::NoteDocument(std::string title)
      : title_(std::move(title))
  {
  }

  NoteDocument NoteDocument::create(std::string title)
  {
    return NoteDocument(std::move(title));
  }

  const std::string &NoteDocument::id() const noexcept
  {
    return id_;
  }

  const std::string &NoteDocument::title() const noexcept
  {
    return title_;
  }

  const std::string &NoteDocument::path() const noexcept
  {
    return path_;
  }

  const std::vector<NoteCell> &NoteDocument::cells() const noexcept
  {
    return cells_;
  }

  std::vector<NoteCell> &NoteDocument::cells() noexcept
  {
    return cells_;
  }

  std::size_t NoteDocument::cell_count() const noexcept
  {
    return cells_.size();
  }

  int NoteDocument::execution_count() const noexcept
  {
    return executionCount_;
  }

  bool NoteDocument::empty() const noexcept
  {
    return cells_.empty();
  }

  bool NoteDocument::has_title() const noexcept
  {
    return !title_.empty();
  }

  bool NoteDocument::has_executable_cells() const noexcept
  {
    return executable_cell_count() > 0;
  }

  std::size_t NoteDocument::executable_cell_count() const noexcept
  {
    return static_cast<std::size_t>(
        std::count_if(
            cells_.begin(),
            cells_.end(),
            [](const NoteCell &cell)
            {
              return cell.executable();
            }));
  }

  void NoteDocument::set_id(std::string id)
  {
    id_ = std::move(id);
  }

  void NoteDocument::set_title(std::string title)
  {
    title_ = std::move(title);
  }

  void NoteDocument::set_path(std::string path)
  {
    path_ = std::move(path);
  }

  void NoteDocument::set_execution_count(int count) noexcept
  {
    executionCount_ = std::max(0, count);
  }

  int NoteDocument::next_execution_count() noexcept
  {
    ++executionCount_;
    return executionCount_;
  }

  void NoteDocument::reset_execution_count() noexcept
  {
    executionCount_ = 0;
  }

  NoteCell &NoteDocument::add_cell(NoteCell cell)
  {
    cells_.push_back(std::move(cell));
    return cells_.back();
  }

  NoteCell &NoteDocument::add_markdown(std::string source)
  {
    return add_cell(NoteCell::markdown(std::move(source)));
  }

  NoteCell &NoteDocument::add_reply(std::string source)
  {
    return add_cell(NoteCell::reply(std::move(source)));
  }

  NoteCell &NoteDocument::add_cpp(std::string source)
  {
    return add_cell(NoteCell::cpp(std::move(source)));
  }

  NoteCell &NoteDocument::add_html(std::string source)
  {
    return add_cell(NoteCell::html(std::move(source)));
  }

  bool NoteDocument::insert_cell(std::size_t index, NoteCell cell)
  {
    if (index > cells_.size())
    {
      return false;
    }

    cells_.insert(
        cells_.begin() + static_cast<std::vector<NoteCell>::difference_type>(index),
        std::move(cell));

    return true;
  }

  bool NoteDocument::remove_cell(std::size_t index)
  {
    if (index >= cells_.size())
    {
      return false;
    }

    cells_.erase(
        cells_.begin() + static_cast<std::vector<NoteCell>::difference_type>(index));

    return true;
  }

  bool NoteDocument::remove_cell_by_id(const std::string &id)
  {
    const auto it =
        std::find_if(
            cells_.begin(),
            cells_.end(),
            [&](const NoteCell &cell)
            {
              return cell.id() == id;
            });

    if (it == cells_.end())
    {
      return false;
    }

    cells_.erase(it);
    return true;
  }

  void NoteDocument::clear_cells()
  {
    cells_.clear();
  }

  void NoteDocument::clear_outputs()
  {
    for (auto &cell : cells_)
    {
      cell.clear_outputs();
    }
  }

  NoteCell *NoteDocument::cell_at(std::size_t index) noexcept
  {
    if (index >= cells_.size())
    {
      return nullptr;
    }

    return &cells_[index];
  }

  const NoteCell *NoteDocument::cell_at(std::size_t index) const noexcept
  {
    if (index >= cells_.size())
    {
      return nullptr;
    }

    return &cells_[index];
  }

  NoteCell *NoteDocument::find_cell(const std::string &id) noexcept
  {
    const auto it =
        std::find_if(
            cells_.begin(),
            cells_.end(),
            [&](const NoteCell &cell)
            {
              return cell.id() == id;
            });

    if (it == cells_.end())
    {
      return nullptr;
    }

    return &(*it);
  }

  const NoteCell *NoteDocument::find_cell(const std::string &id) const noexcept
  {
    const auto it =
        std::find_if(
            cells_.begin(),
            cells_.end(),
            [&](const NoteCell &cell)
            {
              return cell.id() == id;
            });

    if (it == cells_.end())
    {
      return nullptr;
    }

    return &(*it);
  }

  std::optional<std::size_t> NoteDocument::cell_index(const std::string &id) const noexcept
  {
    for (std::size_t i = 0; i < cells_.size(); ++i)
    {
      if (cells_[i].id() == id)
      {
        return i;
      }
    }

    return std::nullopt;
  }
}
