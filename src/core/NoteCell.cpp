/**
 *
 *  @file NoteCell.cpp
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

#include <vix/note/core/NoteCell.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vix::note
{
  NoteCell::NoteCell() = default;

  NoteCell::NoteCell(NoteCellKind kind, std::string source)
      : kind_(kind),
        typeId_(type_id_from_builtin_kind(kind)),
        source_(std::move(source))
  {
  }

  NoteCell::NoteCell(std::string id, NoteCellKind kind, std::string source)
      : id_(std::move(id)),
        kind_(kind),
        typeId_(type_id_from_builtin_kind(kind)),
        source_(std::move(source))
  {
  }

  NoteCell::NoteCell(std::string id, std::string typeId, std::string source)
      : id_(std::move(id)),
        typeId_(normalize_cell_type_id(typeId)),
        source_(std::move(source))
  {
    kind_ = builtin_kind_from_type_id(typeId_);
  }

  NoteCell NoteCell::markdown(std::string source)
  {
    return NoteCell(NoteCellKind::Markdown, std::move(source));
  }

  NoteCell NoteCell::reply(std::string source)
  {
    return NoteCell(NoteCellKind::Reply, std::move(source));
  }

  NoteCell NoteCell::cpp(std::string source)
  {
    return NoteCell(NoteCellKind::Cpp, std::move(source));
  }

  NoteCell NoteCell::html(std::string source)
  {
    return NoteCell(NoteCellKind::Html, std::move(source));
  }

  const std::string &NoteCell::id() const noexcept
  {
    return id_;
  }

  NoteCellKind NoteCell::kind() const noexcept
  {
    return kind_;
  }

  const std::string &NoteCell::type_id() const noexcept
  {
    return typeId_;
  }

  const std::string &NoteCell::source() const noexcept
  {
    return source_;
  }

  const std::string &NoteCell::title() const noexcept
  {
    return title_;
  }

  int NoteCell::execution_count() const noexcept
  {
    return executionCount_;
  }

  const std::vector<NoteOutput> &NoteCell::outputs() const noexcept
  {
    return outputs_;
  }

  void NoteCell::set_id(std::string id)
  {
    id_ = std::move(id);
  }

  void NoteCell::set_kind(NoteCellKind kind) noexcept
  {
    kind_ = kind;
    typeId_ = type_id_from_builtin_kind(kind);
  }

  void NoteCell::set_type_id(std::string typeId)
  {
    typeId_ = normalize_cell_type_id(typeId);
    kind_ = builtin_kind_from_type_id(typeId_);
  }

  void NoteCell::set_source(std::string source)
  {
    source_ = std::move(source);
  }

  void NoteCell::set_title(std::string title)
  {
    title_ = std::move(title);
  }

  void NoteCell::set_execution_count(int count) noexcept
  {
    executionCount_ = std::max(0, count);
  }

  void NoteCell::mark_executed(int count) noexcept
  {
    set_execution_count(count);
  }

  void NoteCell::reset_execution() noexcept
  {
    executionCount_ = 0;
  }

  bool NoteCell::empty() const noexcept
  {
    return source_.empty();
  }

  bool NoteCell::executable() const noexcept
  {
    return is_executable(kind_);
  }

  bool NoteCell::has_outputs() const noexcept
  {
    return !outputs_.empty();
  }

  NoteCell &NoteCell::add_output(NoteOutput output)
  {
    outputs_.push_back(std::move(output));
    return *this;
  }

  void NoteCell::set_outputs(std::vector<NoteOutput> outputs)
  {
    outputs_ = std::move(outputs);
  }

  void NoteCell::clear_outputs()
  {
    outputs_.clear();
  }

  std::string_view to_string(NoteCellKind kind) noexcept
  {
    switch (kind)
    {
    case NoteCellKind::Unknown:
      return "unknown";

    case NoteCellKind::Markdown:
      return "markdown";

    case NoteCellKind::Reply:
      return "reply";

    case NoteCellKind::Cpp:
      return "cpp";

    case NoteCellKind::Html:
      return "html";
    }

    return "unknown";
  }

  NoteCellKind note_cell_kind_from_string(std::string_view value) noexcept
  {
    if (value == "markdown" || value == "md")
    {
      return NoteCellKind::Markdown;
    }

    if (value == "reply" || value == "repl")
    {
      return NoteCellKind::Reply;
    }

    if (value == "cpp" || value == "c++")
    {
      return NoteCellKind::Cpp;
    }

    if (value == "html")
    {
      return NoteCellKind::Html;
    }

    return NoteCellKind::Unknown;
  }


  std::string normalize_cell_type_id(std::string_view value)
  {
    std::string out(value);
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.front())))
      out.erase(out.begin());
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back())))
      out.pop_back();
    for (char &c : out)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (out == "md")
      return "markdown";
    if (out == "repl")
      return "reply";
    if (out == "c++")
      return "cpp";
    return out.empty() ? std::string("unknown") : out;
  }

  std::string type_id_from_builtin_kind(NoteCellKind kind)
  {
    return std::string(to_string(kind));
  }

  NoteCellKind builtin_kind_from_type_id(std::string_view value) noexcept
  {
    return note_cell_kind_from_string(value);
  }

  bool is_builtin_cell_type(std::string_view value) noexcept
  {
    const NoteCellKind kind = note_cell_kind_from_string(value);
    return kind == NoteCellKind::Markdown ||
           kind == NoteCellKind::Reply ||
           kind == NoteCellKind::Cpp ||
           kind == NoteCellKind::Html;
  }

  bool is_executable(NoteCellKind kind) noexcept
  {
    return kind == NoteCellKind::Reply ||
           kind == NoteCellKind::Cpp;
  }
}
