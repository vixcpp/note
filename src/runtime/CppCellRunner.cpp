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
#include <cstdint>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace vix::note
{
  namespace
  {
    struct ProcessOutput
    {
      int exitCode = 0;
      bool timedOut = false;
      bool outputLimitExceeded = false;
      std::string failureMessage;
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

    std::string sanitize_file_stem(std::string value)
    {
      if (value.empty())
      {
        return "cell";
      }

      for (char &c : value)
      {
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '_';

        if (!ok)
        {
          c = '-';
        }
      }

      return value;
    }

    std::uint64_t fnv1a_hash(const std::string &value)
    {
      std::uint64_t hash = 14695981039346656037ULL;

      for (char raw_c : value)
      {
        const auto c = static_cast<unsigned char>(raw_c);

        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
      }

      return hash;
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

    bool env_flag_enabled(const char *name)
    {
      const char *value = std::getenv(name);

      if (value == nullptr || *value == '\0')
      {
        return false;
      }

      const std::string text = lower_copy(value);

      return text != "0" &&
             text != "false" &&
             text != "off" &&
             text != "no";
    }

    std::string strip_ansi_codes(const std::string &text)
    {
      std::string out;
      out.reserve(text.size());

      for (std::size_t i = 0; i < text.size(); ++i)
      {
        const unsigned char c =
            static_cast<unsigned char>(text[i]);

        if (c != 0x1B)
        {
          out.push_back(static_cast<char>(c));
          continue;
        }

        if (i + 1 >= text.size())
        {
          break;
        }

        if (text[i + 1] == '[')
        {
          i += 2;

          while (i < text.size())
          {
            const unsigned char code =
                static_cast<unsigned char>(text[i]);

            if (code >= 0x40 && code <= 0x7E)
            {
              break;
            }

            ++i;
          }

          continue;
        }

        ++i;
      }

      return out;
    }

    std::filesystem::path make_temp_cpp_file(
        const CppCellRunnerOptions &options,
        const std::string &source,
        const std::string &stableName,
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

      if (!stableName.empty())
      {
        return root / (sanitize_file_stem(stableName) + ".cpp");
      }

      const std::uint64_t hash =
          fnv1a_hash(source);

      return root / ("cell-" + std::to_string(hash) + ".cpp");
    }

    CppCellRunnerOptions effective_options(CppCellRunnerOptions options)
    {
      const ProjectContext &context = options.projectContext;

      const bool projectMode =
          options.enableProjectContext ||
          env_flag_enabled("VIX_NOTE_PROJECT_CONTEXT") ||
          env_flag_enabled("VIX_NOTE_REGISTRY_DEPS");

      /*
       * Fast notebook mode.
       *
       * By default, do not attach every C++ cell to the full project/registry
       * context. This keeps normal learning cells fast and avoids paying the
       * first-run registry/build cost when the user only wants to test small C++
       * snippets.
       */
      if (!context.enabled || !projectMode)
      {
        if (options.workingDirectory.empty())
        {
          std::error_code ec;

          const std::filesystem::path root =
              options.temporaryDirectory.empty()
                  ? std::filesystem::temp_directory_path(ec) / "vix-note"
                  : options.temporaryDirectory;

          if (!ec)
          {
            options.workingDirectory = root;
          }
        }

        return options;
      }

      /*
       * Project/registry mode.
       *
       * Opt-in mode for cells that include registry libraries or project-local
       * headers/libs. Slower on the first run, but required for linked packages.
       */
      if (options.workingDirectory.empty())
      {
        options.workingDirectory = context.effective_working_directory();
      }

      if (options.temporaryDirectory.empty() &&
          !context.projectRoot.empty())
      {
        options.temporaryDirectory =
            context.projectRoot / ".vix" / "note" / "tmp";
      }

      return options;
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

    bool write_text_file_if_changed(
        const std::filesystem::path &path,
        const std::string &content,
        std::string &err)
    {
      err.clear();

      std::string existing;

      if (read_text_file(path, existing) && existing == content)
      {
        return true;
      }

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

    bool path_contains_parent_reference(const std::filesystem::path &path)
    {
      for (const auto &part : path)
      {
        if (part == "..")
        {
          return true;
        }
      }

      return false;
    }

    std::string vix_run_file_argument(
        const CppCellRunnerOptions &options,
        const std::filesystem::path &file)
    {
      if (options.workingDirectory.empty())
      {
        return file.string();
      }

      std::error_code ec;

      const std::filesystem::path relative =
          std::filesystem::relative(
              file,
              options.workingDirectory,
              ec);

      if (!ec &&
          !relative.empty() &&
          !path_contains_parent_reference(relative))
      {
        return relative.generic_string();
      }

      return file.string();
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

      cmd << "set VIX_COLOR=never && ";
#else
      if (!options.workingDirectory.empty())
      {
        cmd << "cd "
            << shell_quote(options.workingDirectory.string())
            << " && ";
      }

      cmd << "VIX_COLOR=never ";
#endif

      const std::string runFile =
          vix_run_file_argument(options, file);

      cmd << shell_quote(options.vixCommand)
          << " run "
          << shell_quote(runFile);

      if (!options.runArgs.empty())
      {
        for (const auto &arg : options.runArgs)
        {
          cmd << " " << shell_quote(arg);
        }
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

    bool append_limited(
        std::string &out,
        const char *data,
        std::size_t size,
        std::size_t maxBytes)
    {
      if (size == 0)
      {
        return true;
      }

      if (maxBytes == 0)
      {
        out.append(data, size);
        return true;
      }

      if (out.size() >= maxBytes)
      {
        return false;
      }

      const std::size_t remaining =
          maxBytes - out.size();

      const std::size_t toCopy =
          size < remaining ? size : remaining;

      out.append(data, toCopy);

      return toCopy == size;
    }

    std::uintmax_t safe_file_size_no_error(const std::filesystem::path &path)
    {
      std::error_code ec;

      const std::uintmax_t size =
          std::filesystem::file_size(path, ec);

      if (ec)
      {
        return 0;
      }

      return size;
    }

    bool output_files_exceed_limit(
        const std::filesystem::path *stdoutFile,
        const std::filesystem::path *stderrFile,
        std::size_t maxBytes)
    {
      if (maxBytes == 0)
      {
        return false;
      }

      std::uintmax_t total = 0;

      if (stdoutFile != nullptr)
      {
        total += safe_file_size_no_error(*stdoutFile);
      }

      if (stderrFile != nullptr)
      {
        total += safe_file_size_no_error(*stderrFile);
      }

      return total > static_cast<std::uintmax_t>(maxBytes);
    }

    bool read_text_file_limited(
        const std::filesystem::path &path,
        std::string &out,
        std::size_t maxBytes,
        bool &truncated)
    {
      out.clear();
      truncated = false;

      std::ifstream in(path, std::ios::binary);

      if (!in.is_open())
      {
        return false;
      }

      std::array<char, 4096> buffer{};

      while (in)
      {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

        const std::streamsize count = in.gcount();

        if (count <= 0)
        {
          break;
        }

        const bool ok =
            append_limited(
                out,
                buffer.data(),
                static_cast<std::size_t>(count),
                maxBytes);

        if (!ok)
        {
          truncated = true;
          break;
        }
      }

      return true;
    }

#ifdef _WIN32
    bool append_pipe_output(
        HANDLE pipe,
        std::string &out,
        std::size_t maxBytes)
    {
      bool withinLimit = true;

      while (true)
      {
        DWORD available = 0;

        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
        {
          break;
        }

        if (available == 0)
        {
          break;
        }

        std::array<char, 4096> buffer{};
        DWORD read = 0;

        const DWORD toRead =
            available < static_cast<DWORD>(buffer.size())
                ? available
                : static_cast<DWORD>(buffer.size());

        if (!ReadFile(pipe, buffer.data(), toRead, &read, nullptr) ||
            read == 0)
        {
          break;
        }

        if (!append_limited(
                out,
                buffer.data(),
                static_cast<std::size_t>(read),
                maxBytes))
        {
          withinLimit = false;
          break;
        }
      }

      return withinLimit;
    }

    ProcessOutput run_command_controlled(
        const std::string &command,
        std::chrono::milliseconds timeout,
        std::size_t maxOutputBytes,
        const std::filesystem::path *stdoutFile = nullptr,
        const std::filesystem::path *stderrFile = nullptr)
    {
      ProcessOutput output;

      SECURITY_ATTRIBUTES security{};
      security.nLength = sizeof(security);
      security.bInheritHandle = TRUE;
      security.lpSecurityDescriptor = nullptr;

      HANDLE readPipe = nullptr;
      HANDLE writePipe = nullptr;

      if (!CreatePipe(&readPipe, &writePipe, &security, 0))
      {
        output.exitCode = 1;
        output.failureMessage = "cannot create process pipe";
        output.stderrText = output.failureMessage;
        output.mergedText = output.failureMessage;
        return output;
      }

      SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

      STARTUPINFOA startup{};
      startup.cb = sizeof(startup);
      startup.dwFlags = STARTF_USESTDHANDLES;
      startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
      startup.hStdOutput = writePipe;
      startup.hStdError = writePipe;

      PROCESS_INFORMATION process{};

      std::string commandLine = "cmd.exe /C " + command;

      const BOOL created =
          CreateProcessA(
              nullptr,
              commandLine.data(),
              nullptr,
              nullptr,
              TRUE,
              CREATE_NO_WINDOW,
              nullptr,
              nullptr,
              &startup,
              &process);

      CloseHandle(writePipe);

      if (!created)
      {
        CloseHandle(readPipe);

        output.exitCode = 1;
        output.failureMessage = "cannot start C++ cell process";
        output.stderrText = output.failureMessage;
        output.mergedText = output.failureMessage;
        return output;
      }

      const auto start = std::chrono::steady_clock::now();
      const bool hasTimeout = timeout.count() > 0;

      while (true)
      {
        if (!append_pipe_output(readPipe, output.mergedText, maxOutputBytes) ||
            output_files_exceed_limit(stdoutFile, stderrFile, maxOutputBytes))
        {
          output.outputLimitExceeded = true;
          output.exitCode = 125;
          output.failureMessage = "C++ cell produced too much output";

          TerminateProcess(process.hProcess, 125);
          WaitForSingleObject(process.hProcess, 1000);
          break;
        }

        const DWORD waitResult =
            WaitForSingleObject(process.hProcess, 25);

        if (waitResult == WAIT_OBJECT_0)
        {
          break;
        }

        if (hasTimeout)
        {
          const auto elapsed =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start);

          if (elapsed >= timeout)
          {
            output.timedOut = true;
            output.exitCode = 124;
            output.failureMessage =
                "C++ cell timed out after " +
                std::to_string(timeout.count()) +
                " ms";

            TerminateProcess(process.hProcess, 124);
            WaitForSingleObject(process.hProcess, 1000);
            break;
          }
        }
      }

      if (!append_pipe_output(readPipe, output.mergedText, maxOutputBytes) &&
          !output.timedOut &&
          !output.outputLimitExceeded)
      {
        output.outputLimitExceeded = true;
        output.exitCode = 125;
        output.failureMessage = "C++ cell produced too much output";
      }

      if (!output.timedOut && !output.outputLimitExceeded)
      {
        DWORD exitCode = 1;

        if (GetExitCodeProcess(process.hProcess, &exitCode))
        {
          output.exitCode = static_cast<int>(exitCode);
        }
        else
        {
          output.exitCode = 1;
        }
      }

      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
      CloseHandle(readPipe);

      return output;
    }
#else
    bool append_fd_output(
        int fd,
        std::string &out,
        std::size_t maxBytes)
    {
      bool withinLimit = true;

      std::array<char, 4096> buffer{};

      while (true)
      {
        const ssize_t n =
            ::read(fd, buffer.data(), buffer.size());

        if (n > 0)
        {
          if (!append_limited(
                  out,
                  buffer.data(),
                  static_cast<std::size_t>(n),
                  maxBytes))
          {
            withinLimit = false;
            break;
          }

          continue;
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
          break;
        }

        break;
      }

      return withinLimit;
    }

    void terminate_process_group(pid_t pid)
    {
      if (pid <= 0)
      {
        return;
      }

      ::kill(-pid, SIGTERM);

      for (int i = 0; i < 10; ++i)
      {
        int status = 0;
        const pid_t done = ::waitpid(pid, &status, WNOHANG);

        if (done == pid)
        {
          return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }

      ::kill(-pid, SIGKILL);
    }

    ProcessOutput run_command_controlled(
        const std::string &command,
        std::chrono::milliseconds timeout,
        std::size_t maxOutputBytes,
        const std::filesystem::path *stdoutFile = nullptr,
        const std::filesystem::path *stderrFile = nullptr)
    {
      ProcessOutput output;

      int pipeFd[2];

      if (::pipe(pipeFd) != 0)
      {
        output.exitCode = 1;
        output.failureMessage = "cannot create process pipe";
        output.stderrText = output.failureMessage;
        output.mergedText = output.failureMessage;
        return output;
      }

      const pid_t pid = ::fork();

      if (pid == -1)
      {
        ::close(pipeFd[0]);
        ::close(pipeFd[1]);

        output.exitCode = 1;
        output.failureMessage = "cannot start C++ cell process";
        output.stderrText = output.failureMessage;
        output.mergedText = output.failureMessage;
        return output;
      }

      if (pid == 0)
      {
        ::setpgid(0, 0);

        ::close(pipeFd[0]);

        ::dup2(pipeFd[1], STDOUT_FILENO);
        ::dup2(pipeFd[1], STDERR_FILENO);

        ::close(pipeFd[1]);

        ::execl(
            "/bin/sh",
            "sh",
            "-c",
            command.c_str(),
            static_cast<char *>(nullptr));

        _exit(127);
      }

      ::setpgid(pid, pid);

      ::close(pipeFd[1]);

      const int flags = ::fcntl(pipeFd[0], F_GETFL, 0);

      if (flags != -1)
      {
        ::fcntl(pipeFd[0], F_SETFL, flags | O_NONBLOCK);
      }

      const auto start = std::chrono::steady_clock::now();
      const bool hasTimeout = timeout.count() > 0;

      int status = 0;

      while (true)
      {
        if (!append_fd_output(pipeFd[0], output.mergedText, maxOutputBytes) ||
            output_files_exceed_limit(stdoutFile, stderrFile, maxOutputBytes))
        {
          output.outputLimitExceeded = true;
          output.exitCode = 125;
          output.failureMessage = "C++ cell produced too much output";

          terminate_process_group(pid);
          break;
        }

        const pid_t done =
            ::waitpid(pid, &status, WNOHANG);

        if (done == pid)
        {
          output.exitCode = normalize_exit_code(status);
          break;
        }

        if (done == -1)
        {
          output.exitCode = 1;
          break;
        }

        if (hasTimeout)
        {
          const auto elapsed =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start);

          if (elapsed >= timeout)
          {
            output.timedOut = true;
            output.exitCode = 124;
            output.failureMessage =
                "C++ cell timed out after " +
                std::to_string(timeout.count()) +
                " ms";

            terminate_process_group(pid);
            break;
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }

      if (!append_fd_output(pipeFd[0], output.mergedText, maxOutputBytes) &&
          !output.timedOut &&
          !output.outputLimitExceeded)
      {
        output.outputLimitExceeded = true;
        output.exitCode = 125;
        output.failureMessage = "C++ cell produced too much output";
      }

      ::close(pipeFd[0]);

      return output;
    }
#endif

    ProcessOutput run_command_capture_merged(
        const std::string &command,
        std::chrono::milliseconds timeout,
        std::size_t maxOutputBytes)
    {
      ProcessOutput output =
          run_command_controlled(
              command,
              timeout,
              maxOutputBytes);

      if (!output.failureMessage.empty() && output.mergedText.empty())
      {
        output.mergedText = output.failureMessage;
      }

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
        const std::filesystem::path &stderrFile,
        std::chrono::milliseconds timeout,
        std::size_t maxOutputBytes)
    {
      ProcessOutput output =
          run_command_controlled(
              command,
              timeout,
              maxOutputBytes,
              &stdoutFile,
              &stderrFile);

      bool stdoutTruncated = false;
      bool stderrTruncated = false;

      (void)read_text_file_limited(
          stdoutFile,
          output.stdoutText,
          maxOutputBytes,
          stdoutTruncated);

      const std::size_t remaining =
          maxOutputBytes > output.stdoutText.size()
              ? maxOutputBytes - output.stdoutText.size()
              : 0;

      (void)read_text_file_limited(
          stderrFile,
          output.stderrText,
          remaining,
          stderrTruncated);

      if (stdoutTruncated || stderrTruncated)
      {
        output.outputLimitExceeded = true;

        if (output.failureMessage.empty())
        {
          output.failureMessage = "C++ cell produced too much output";
        }

        if (output.exitCode == 0)
        {
          output.exitCode = 125;
        }
      }

      if (!output.failureMessage.empty() && output.stderrText.empty())
      {
        output.stderrText = output.failureMessage;
      }

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

      if (contains_text(lower, "runtime error:") ||
          contains_text(lower, "runtime log:") ||
          contains_text(lower, "segmentation fault") ||
          contains_text(lower, "aborted") ||
          contains_text(lower, "floating point exception") ||
          contains_text(lower, "core dumped"))
      {
        return false;
      }

      return contains_text(lower, "fatal error:") ||
             contains_text(lower, "undefined reference") ||
             contains_text(lower, "not declared in this scope") ||
             contains_text(lower, "no such file or directory") ||
             contains_text(lower, ": error:") ||
             contains_text(lower, " error:");
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
      if (!process.stdoutText.empty())
      {
        result.add_stdout(process.stdoutText);
      }

      std::string errorText;

      if (!process.stderrText.empty())
      {
        errorText = process.stderrText;
      }
      else if (process.stdoutText.empty())
      {
        errorText = process.mergedText;
      }

      if (errorText.empty())
      {
        result.add_error(
            "C++ cell exited with code " +
            std::to_string(process.exitCode));

        return;
      }

      if (looks_like_runtime_error(errorText, process.exitCode))
      {
        result.add_runtime_error(errorText);
      }
      else if (looks_like_compiler_error(errorText))
      {
        result.add_compiler_error(errorText);
      }
      else
      {
        result.add_error(errorText);
      }

      if (options.enableErrorHints)
      {
        for (const std::string &hint : make_cpp_error_hints(errorText))
        {
          result.add_hint(hint);
        }
      }

      if (options.debugMode && options.includeRawLog && !process.mergedText.empty())
      {
        result.add_raw_log(process.mergedText);
      }
    }

    void add_success_outputs(
        NoteResult &result,
        const CppCellRunnerOptions &options,
        const ProcessOutput &process)
    {
      bool hasVisibleOutput = false;

      if (!process.stdoutText.empty())
      {
        result.add_stdout(process.stdoutText);
        hasVisibleOutput = true;
      }

      if (!process.stderrText.empty())
      {
        result.add_stderr(process.stderrText);
        hasVisibleOutput = true;
      }

      if (!hasVisibleOutput && options.showEmptySuccessOutput)
      {
        result.add_text("Program finished successfully.\nExit code: 0\nNo output.");
      }

      if (options.debugMode && options.includeRawLog && !process.mergedText.empty())
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
        make_temp_cpp_file(
            runOptions,
            source,
            {},
            err);

    if (file.empty())
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    if (!write_text_file_if_changed(file, source, err))
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
              stderrFile,
              runOptions.executionTimeout,
              runOptions.maxCapturedOutputBytes);
    }
    else
    {
      const std::string command =
          make_merged_run_command(runOptions, file);

      process =
          run_command_capture_merged(
              command,
              runOptions.executionTimeout,
              runOptions.maxCapturedOutputBytes);
    }

    process.stdoutText = strip_ansi_codes(process.stdoutText);
    process.stderrText = strip_ansi_codes(process.stderrText);
    process.mergedText = strip_ansi_codes(process.mergedText);

    const auto end =
        std::chrono::steady_clock::now();

    const long long durationMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (process.timedOut)
    {
      NoteResult result =
          NoteResult::failure(
              process.failureMessage.empty()
                  ? "C++ cell timed out"
                  : process.failureMessage,
              process.exitCode == 0 ? 124 : process.exitCode);

      if (!process.stdoutText.empty())
      {
        result.add_stdout(process.stdoutText);
      }

      if (!process.stderrText.empty())
      {
        result.add_stderr(process.stderrText);
      }

      result.add_error(
          process.failureMessage.empty()
              ? "C++ cell timed out"
              : process.failureMessage);

      add_debug_outputs(
          result,
          runOptions,
          file,
          durationMs,
          process.exitCode);

      if (!runOptions.keepTemporaryFile)
      {
        std::error_code ec;

        if (!runOptions.projectContext.enabled)
        {
          std::filesystem::remove(file, ec);
        }

        std::filesystem::remove(stdoutFile, ec);
        std::filesystem::remove(stderrFile, ec);
      }

      return result;
    }

    if (process.outputLimitExceeded)
    {
      const std::string message =
          process.failureMessage.empty()
              ? "C++ cell produced too much output"
              : process.failureMessage;

      NoteResult result =
          NoteResult::failure(
              message,
              process.exitCode == 0 ? 125 : process.exitCode);

      if (!process.stdoutText.empty())
      {
        result.add_stdout(process.stdoutText);
      }

      if (!process.stderrText.empty())
      {
        result.add_stderr(process.stderrText);
      }

      result.add_error(message);

      add_debug_outputs(
          result,
          runOptions,
          file,
          durationMs,
          process.exitCode);

      if (!runOptions.keepTemporaryFile)
      {
        std::error_code ec;

        if (!runOptions.projectContext.enabled)
        {
          std::filesystem::remove(file, ec);
        }

        std::filesystem::remove(stdoutFile, ec);
        std::filesystem::remove(stderrFile, ec);
      }

      return result;
    }

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

      if (!runOptions.projectContext.enabled)
      {
        std::filesystem::remove(file, ec);
      }

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
