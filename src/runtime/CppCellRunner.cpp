/**
 *
 *  @file CppCellRunner.cpp
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

#include <vix/note/runtime/CppCellRunner.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace vix::note
{
  namespace
  {
    std::string shell_quote(const std::string &value)
    {
#ifdef _WIN32
      std::string out = "\"";

      for (char c : value)
      {
        if (c == '"')
        {
          out += "\\\"";
        }
        else
        {
          out.push_back(c);
        }
      }

      out += "\"";
      return out;
#else
      std::string out = "'";

      for (char c : value)
      {
        if (c == '\'')
        {
          out += "'\\''";
        }
        else
        {
          out.push_back(c);
        }
      }

      out += "'";
      return out;
#endif
    }

    int normalize_exit_code(int rawCode)
    {
#ifdef _WIN32
      return rawCode;
#else
      if (rawCode == -1)
      {
        return 1;
      }

      if (WIFEXITED(rawCode))
      {
        return WEXITSTATUS(rawCode);
      }

      if (WIFSIGNALED(rawCode))
      {
        return 128 + WTERMSIG(rawCode);
      }

      return rawCode;
#endif
    }

    std::filesystem::path make_temp_cpp_file(
        const CppCellRunnerOptions &options,
        std::string &err)
    {
      err.clear();

      std::error_code ec;

      std::filesystem::path root =
          options.temporaryDirectory.empty()
              ? std::filesystem::temp_directory_path(ec) / "vix-note"
              : options.temporaryDirectory;

      if (ec)
      {
        err = "cannot resolve temporary directory: " + ec.message();
        return {};
      }

      std::filesystem::create_directories(root, ec);

      if (ec)
      {
        err = "cannot create temporary directory: " + root.string() + ": " + ec.message();
        return {};
      }

      const auto stamp =
          std::chrono::steady_clock::now().time_since_epoch().count();

      return root / ("cell-" + std::to_string(stamp) + ".cpp");
    }

    bool write_text_file(
        const std::filesystem::path &path,
        const std::string &content,
        std::string &err)
    {
      err.clear();

      std::ofstream out(path, std::ios::binary | std::ios::trunc);

      if (!out.is_open())
      {
        err = "cannot write C++ cell file: " + path.string();
        return false;
      }

      out << content;

      if (!out.good())
      {
        err = "cannot write C++ cell file: " + path.string();
        return false;
      }

      return true;
    }

    std::string make_run_command(
        const CppCellRunnerOptions &options,
        const std::filesystem::path &file)
    {
      std::ostringstream cmd;

#ifdef _WIN32
      if (!options.workingDirectory.empty())
      {
        cmd << "cd /D "
            << shell_quote(options.workingDirectory.string())
            << " && ";
      }
#else
      if (!options.workingDirectory.empty())
      {
        cmd << "cd "
            << shell_quote(options.workingDirectory.string())
            << " && ";
      }
#endif

      cmd << shell_quote(options.vixCommand)
          << " run "
          << shell_quote(file.string());

      for (const auto &arg : options.runArgs)
      {
        cmd << " " << shell_quote(arg);
      }

      cmd << " 2>&1";

      return cmd.str();
    }

    NoteResult run_command_capture(const std::string &command)
    {
#ifdef _WIN32
      FILE *pipe = _popen(command.c_str(), "r");
#else
      FILE *pipe = popen(command.c_str(), "r");
#endif

      if (pipe == nullptr)
      {
        return NoteResult::failure("cannot start C++ cell process", 1)
            .add_error("cannot start C++ cell process");
      }

      std::string output;
      std::array<char, 4096> buffer{};

      while (true)
      {
        const std::size_t n =
            std::fread(buffer.data(), 1, buffer.size(), pipe);

        if (n > 0)
        {
          output.append(buffer.data(), n);
        }

        if (n < buffer.size())
        {
          if (std::feof(pipe) != 0)
          {
            break;
          }

          if (std::ferror(pipe) != 0)
          {
            break;
          }
        }
      }

#ifdef _WIN32
      const int rawCode = _pclose(pipe);
#else
      const int rawCode = pclose(pipe);
#endif

      const int exitCode = normalize_exit_code(rawCode);

      if (exitCode != 0)
      {
        NoteResult result =
            NoteResult::failure("C++ cell failed", exitCode);

        if (!output.empty())
        {
          result.add_error(output);
        }

        return result;
      }

      NoteResult result =
          NoteResult::success("C++ cell executed");

      if (!output.empty())
      {
        result.add_stdout(output);
      }

      return result;
    }
  }

  CppCellRunner::CppCellRunner() = default;

  CppCellRunner::CppCellRunner(CppCellRunnerOptions options)
      : options_(std::move(options))
  {
  }

  const CppCellRunnerOptions &CppCellRunner::options() const noexcept
  {
    return options_;
  }

  void CppCellRunner::set_options(CppCellRunnerOptions options) noexcept
  {
    options_ = std::move(options);
  }

  NoteResult CppCellRunner::run_source(const std::string &source) const
  {
    if (source.empty())
    {
      return NoteResult::failure("empty C++ cell", 1)
          .add_error("empty C++ cell");
    }

    std::string err;
    const std::filesystem::path file =
        make_temp_cpp_file(options_, err);

    if (file.empty())
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    if (!write_text_file(file, source, err))
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    const std::string command =
        make_run_command(options_, file);

    NoteResult result =
        run_command_capture(command);

    if (!options_.keepTemporaryFile)
    {
      std::error_code ec;
      std::filesystem::remove(file, ec);
    }

    return result;
  }

  NoteResult CppCellRunner::run_cell(const NoteCell &cell) const
  {
    if (cell.kind() != NoteCellKind::Cpp)
    {
      return NoteResult::failure("cell is not a C++ cell", 1)
          .add_error("cell is not a C++ cell");
    }

    return run_source(cell.source());
  }

  NoteResult run_cpp_source(const std::string &source)
  {
    return CppCellRunner().run_source(source);
  }

  NoteResult run_cpp_cell(const NoteCell &cell)
  {
    return CppCellRunner().run_cell(cell);
  }
}
