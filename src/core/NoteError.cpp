/**
 *
 *  @file NoteError.cpp
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

#include <vix/note/core/NoteError.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace vix::note
{
  NoteError::NoteError(std::string message)
      : std::runtime_error(std::move(message)),
        code_(NoteErrorCode::Unknown)
  {
  }

  NoteError::NoteError(NoteErrorCode code, std::string message)
      : std::runtime_error(std::move(message)),
        code_(code)
  {
  }

  NoteErrorCode NoteError::code() const noexcept
  {
    return code_;
  }

  std::string_view to_string(NoteErrorCode code) noexcept
  {
    switch (code)
    {
    case NoteErrorCode::Unknown:
      return "unknown";

    case NoteErrorCode::Parse:
      return "parse";

    case NoteErrorCode::Read:
      return "read";

    case NoteErrorCode::Write:
      return "write";

    case NoteErrorCode::Runtime:
      return "runtime";

    case NoteErrorCode::Unsupported:
      return "unsupported";

    case NoteErrorCode::Invalid:
      return "invalid";
    }

    return "unknown";
  }
}
