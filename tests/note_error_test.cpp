/**
 *
 *  @file note_error_test.cpp
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

#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>

int main()
{
  {
    vix::note::NoteError error("unknown failure");

    assert(error.code() == vix::note::NoteErrorCode::Unknown);
    assert(std::string(error.what()) == "unknown failure");
  }

  {
    vix::note::NoteError error(
        vix::note::NoteErrorCode::Parse,
        "invalid note syntax");

    assert(error.code() == vix::note::NoteErrorCode::Parse);
    assert(std::string(error.what()) == "invalid note syntax");
  }

  {
    const std::runtime_error &base =
        vix::note::NoteError(
            vix::note::NoteErrorCode::Runtime,
            "cell execution failed");

    assert(std::string(base.what()) == "cell execution failed");
  }

  {
    assert(vix::note::to_string(vix::note::NoteErrorCode::Unknown) == "unknown");
    assert(vix::note::to_string(vix::note::NoteErrorCode::Parse) == "parse");
    assert(vix::note::to_string(vix::note::NoteErrorCode::Read) == "read");
    assert(vix::note::to_string(vix::note::NoteErrorCode::Write) == "write");
    assert(vix::note::to_string(vix::note::NoteErrorCode::Runtime) == "runtime");
    assert(vix::note::to_string(vix::note::NoteErrorCode::Unsupported) == "unsupported");
    assert(vix::note::to_string(vix::note::NoteErrorCode::Invalid) == "invalid");
  }

  return 0;
}
