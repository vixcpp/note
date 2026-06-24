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
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace vix::note
{
  namespace
  {
    struct ProcessOutput
    {
      int exitCode = 0;
      std::string stdoutText;
      std::string stderrText;
      std::string mergedText;
    };

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

    bool contains_text(
        const std::string &value,
        const std::string &needle)
    {
      return value.find(needle) != std::string::npos;
    }

    std::string lower_copy(std::string value)
    {
      for (char &c : value)
      {
        if (c >= 'A' && c <= 'Z')
        {
          c = static_cast<char>(c - 'A' + 'a');
        }
      }

      return value;
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

    CppCellRunnerOptions effective_options(CppCellRunnerOptions options)
    {
      const ProjectContext &context = options.projectContext;

      if (!context.enabled)
      {
        return options;
      }

      if (options.workingDirectory.empty())
      {
        options.workingDirectory = context.effective_working_directory();
      }

      for (const auto &includePath : context.includePaths)
      {
        if (includePath.empty())
        {
          continue;
        }

        options.runArgs.push_back("-I" + includePath.string());
      }

      return options;
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

    bool read_text_file(
        const std::filesystem::path &path,
        std::string &out)
    {
      out.clear();

      std::ifstream in(path, std::ios::binary);

      if (!in.is_open())
      {
        return false;
      }

      std::ostringstream buffer;
      buffer << in.rdbuf();

      if (in.bad())
      {
        return false;
      }

      out = buffer.str();
      return true;
    }

    std::string make_base_run_command(
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

      return cmd.str();
    }

    std::string make_merged_run_command(
        const CppCellRunnerOptions &options,
        const std::filesystem::path &file)
    {
      return make_base_run_command(options, file) + " 2>&1";
    }

    std::string make_separated_run_command(
        const CppCellRunnerOptions &options,
        const std::filesystem::path &file,
        const std::filesystem::path &stdoutFile,
        const std::filesystem::path &stderrFile)
    {
      std::ostringstream cmd;

      cmd << make_base_run_command(options, file)
          << " > "
          << shell_quote(stdoutFile.string())
          << " 2> "
          << shell_quote(stderrFile.string());

      return cmd.str();
    }

    int run_command_exit_code(const std::string &command)
    {
#ifdef _WIN32
      FILE *pipe = _popen(command.c_str(), "r");
#else
      FILE *pipe = popen(command.c_str(), "r");
#endif

      if (pipe == nullptr)
      {
        return 1;
      }

      std::array<char, 4096> buffer{};

      while (true)
      {
        const std::size_t n =
            std::fread(buffer.data(), 1, buffer.size(), pipe);

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

      return normalize_exit_code(rawCode);
    }

    ProcessOutput run_command_capture_merged(const std::string &command)
    {
      ProcessOutput output;

#ifdef _WIN32
      FILE *pipe = _popen(command.c_str(), "r");
#else
      FILE *pipe = popen(command.c_str(), "r");
#endif

      if (pipe == nullptr)
      {
        output.exitCode = 1;
        output.mergedText = "cannot start C++ cell process";
        return output;
      }

      std::array<char, 4096> buffer{};

      while (true)
      {
        const std::size_t n =
            std::fread(buffer.data(), 1, buffer.size(), pipe);

        if (n > 0)
        {
          output.mergedText.append(buffer.data(), n);
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

      output.exitCode = normalize_exit_code(rawCode);

      if (output.exitCode == 0)
      {
        output.stdoutText = output.mergedText;
      }
      else
      {
        output.stderrText = output.mergedText;
      }

      return output;
    }

    ProcessOutput run_command_capture_separated(
        const std::string &command,
        const std::filesystem::path &stdoutFile,
        const std::filesystem::path &stderrFile)
    {
      ProcessOutput output;
      output.exitCode = run_command_exit_code(command);

      (void)read_text_file(stdoutFile, output.stdoutText);
      (void)read_text_file(stderrFile, output.stderrText);

      output.mergedText = output.stdoutText;

      if (!output.mergedText.empty() && !output.stderrText.empty())
      {
        output.mergedText += '\n';
      }

      output.mergedText += output.stderrText;

      return output;
    }

    bool looks_like_compiler_error(const std::string &text)
    {
      const std::string lower = lower_copy(text);

      return contains_text(lower, "error:") ||
             contains_text(lower, "fatal error:") ||
             contains_text(lower, "undefined reference") ||
             contains_text(lower, "not declared in this scope") ||
             contains_text(lower, "expected") ||
             contains_text(lower, "no such file or directory");
    }

    bool looks_like_runtime_error(
        const std::string &text,
        int exitCode)
    {
      const std::string lower = lower_copy(text);

      return exitCode >= 128 ||
             contains_text(lower, "segmentation fault") ||
             contains_text(lower, "floating point exception") ||
             contains_text(lower, "abort") ||
             contains_text(lower, "core dumped") ||
             contains_text(lower, "runtime error");
    }

    std::vector<std::string> make_cpp_error_hints(const std::string &text)
    {
      const std::string lower = lower_copy(text);

      std::vector<std::string> hints;

      if (contains_text(lower, "expected ';'") ||
          contains_text(lower, "expected ‘;’") ||
          contains_text(lower, "expected ;"))
      {
        hints.push_back("Check if the previous C++ statement is missing a semicolon.");
      }

      if (contains_text(lower, "not declared in this scope"))
      {
        hints.push_back("Check if the variable or function name is spelled correctly and declared before it is used.");
      }

      if (contains_text(lower, "no such file or directory") ||
          contains_text(lower, "fatal error:"))
      {
        hints.push_back("Check if the included header exists and if the include path is configured correctly.");
      }

      if (contains_text(lower, "undefined reference"))
      {
        hints.push_back("Check if the function is implemented and if the required library is linked.");
      }

      if (contains_text(lower, "segmentation fault"))
      {
        hints.push_back("A segmentation fault usually means invalid memory access, such as a bad pointer or out-of-range access.");
      }

      if (contains_text(lower, "floating point exception"))
      {
        hints.push_back("Check for division by zero or invalid arithmetic operations.");
      }

      return hints;
    }

    void add_debug_outputs(
        NoteResult &result,
        const CppCellRunnerOptions &options,
        const std::filesystem::path &sourceFile,
        long long durationMs,
        int exitCode)
    {
      if (!options.debugMode)
      {
        return;
      }

      result.add_debug("source_path=" + sourceFile.string());
      result.add_debug("duration_ms=" + std::to_string(durationMs));
      result.add_debug("exit_code=" + std::to_string(exitCode));

      if (options.projectContext.enabled)
      {
        result.add_debug("project_name=" + options.projectContext.projectName);
        result.add_debug("project_root=" + options.projectContext.projectRoot.string());
        result.add_debug("working_directory=" + options.workingDirectory.string());
      }
    }

    void add_failure_outputs(
        NoteResult &result,
        const CppCellRunnerOptions &options,
        const ProcessOutput &process)
    {
      const std::string errorText =
          !process.stderrText.empty()
              ? process.stderrText
              : process.mergedText;

      if (!process.stdoutText.empty())
      {
        result.add_stdout(process.stdoutText);
      }

      if (!process.stderrText.empty())
      {
        result.add_stderr(process.stderrText);
      }

      if (!errorText.empty())
      {
        result.add_error(errorText);
      }

      if (!errorText.empty() && looks_like_compiler_error(errorText))
      {
        result.add_compiler_error(errorText);
      }
      else if (!errorText.empty() &&
               looks_like_runtime_error(errorText, process.exitCode))
      {
        result.add_runtime_error(errorText);
      }
      else if (!errorText.empty())
      {
        result.add_runtime_error(errorText);
      }

      if (options.enableErrorHints)
      {
        for (const std::string &hint : make_cpp_error_hints(errorText))
        {
          result.add_hint(hint);
        }
      }

      if (options.includeRawLog && !process.mergedText.empty())
      {
        result.add_raw_log(process.mergedText);
      }
    }

    void add_success_outputs(
        NoteResult &result,
        const CppCellRunnerOptions &options,
        const ProcessOutput &process)
    {
      if (!process.stdoutText.empty())
      {
        result.add_stdout(process.stdoutText);
      }

      if (!process.stderrText.empty())
      {
        result.add_stderr(process.stderrText);
      }

      if (options.includeRawLog && !process.mergedText.empty())
      {
        result.add_raw_log(process.mergedText);
      }
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

    const CppCellRunnerOptions runOptions =
        effective_options(options_);

    std::string err;
    const std::filesystem::path file =
        make_temp_cpp_file(runOptions, err);

    if (file.empty())
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    if (!write_text_file(file, source, err))
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    const std::filesystem::path stdoutFile =
        std::filesystem::path(file.string() + ".stdout");

    const std::filesystem::path stderrFile =
        std::filesystem::path(file.string() + ".stderr");

    const auto start =
        std::chrono::steady_clock::now();

    ProcessOutput process;

    if (runOptions.separateStreams)
    {
      const std::string command =
          make_separated_run_command(
              runOptions,
              file,
              stdoutFile,
              stderrFile);

      process =
          run_command_capture_separated(
              command,
              stdoutFile,
              stderrFile);
    }
    else
    {
      const std::string command =
          make_merged_run_command(runOptions, file);

      process =
          run_command_capture_merged(command);
    }

    const auto end =
        std::chrono::steady_clock::now();

    const long long durationMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    NoteResult result =
        process.exitCode == 0
            ? NoteResult::success("C++ cell executed")
            : NoteResult::failure("C++ cell failed", process.exitCode);

    if (process.exitCode == 0)
    {
      add_success_outputs(result, runOptions, process);
    }
    else
    {
      add_failure_outputs(result, runOptions, process);
    }

    add_debug_outputs(
        result,
        runOptions,
        file,
        durationMs,
        process.exitCode);

    if (!runOptions.keepTemporaryFile)
    {
      std::error_code ec;
      std::filesystem::remove(file, ec);
      std::filesystem::remove(stdoutFile, ec);
      std::filesystem::remove(stderrFile, ec);
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
