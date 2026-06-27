/**
 *
 *  @file cpp_cell_runner_test.cpp
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
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/runtime/CppCellRunner.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace
{
  std::filesystem::path make_test_root()
  {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("vix-note-cpp-cell-runner-test-" + std::to_string(now));
  }

  void write_file(const std::filesystem::path &path, const std::string &content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }

  std::filesystem::path make_fake_vix_command(const std::filesystem::path &root)
  {
#ifdef _WIN32
    const std::filesystem::path command = root / "fake-vix.bat";

    write_file(
        command,
        "@echo off\n"
        "if \"%3\"==\"--fail\" (\n"
        "  echo simulated failure\n"
        "  exit /b 7\n"
        ")\n"
        "if \"%3\"==\"--compiler-fail\" (\n"
        "  echo main.cpp:3:10: error: expected ';' 1>&2\n"
        "  exit /b 1\n"
        ")\n"
        "if \"%3\"==\"--runtime-fail\" (\n"
        "  echo segmentation fault 1>&2\n"
        "  exit /b 139\n"
        ")\n"
        "if \"%3\"==\"--stderr-ok\" (\n"
        "  echo warning: simulated warning 1>&2\n"
        ")\n"
        "echo fake vix run\n"
        "echo mode:%1\n"
        "type \"%2\"\n"
        "exit /b 0\n");

    return command;
#else
    const std::filesystem::path command = root / "fake-vix";

    write_file(
        command,
        "#!/bin/sh\n"
        "if [ \"$3\" = \"--fail\" ]; then\n"
        "  echo simulated failure\n"
        "  exit 7\n"
        "fi\n"
        "if [ \"$3\" = \"--compiler-fail\" ]; then\n"
        "  echo \"main.cpp:3:10: error: expected ';'\" >&2\n"
        "  exit 1\n"
        "fi\n"
        "if [ \"$3\" = \"--runtime-fail\" ]; then\n"
        "  echo \"segmentation fault\" >&2\n"
        "  exit 139\n"
        "fi\n"
        "if [ \"$3\" = \"--stderr-ok\" ]; then\n"
        "  echo \"warning: simulated warning\" >&2\n"
        "fi\n"
        "echo fake vix run\n"
        "echo mode:$1\n"
        "cat \"$2\"\n"
        "exit 0\n");

    chmod(command.string().c_str(), 0755);

    return command;
#endif
  }

  std::size_t count_cpp_files(const std::filesystem::path &dir)
  {
    if (!std::filesystem::exists(dir))
    {
      return 0;
    }

    std::size_t count = 0;

    for (const auto &entry : std::filesystem::directory_iterator(dir))
    {
      if (entry.path().extension() == ".cpp")
      {
        ++count;
      }
    }

    return count;
  }

  bool has_output_kind(
      const vix::note::NoteResult &result,
      vix::note::NoteOutputKind kind)
  {
    for (const vix::note::NoteOutput &output : result.outputs())
    {
      if (output.kind == kind)
      {
        return true;
      }
    }

    return false;
  }

  bool output_contains(
      const vix::note::NoteResult &result,
      vix::note::NoteOutputKind kind,
      const std::string &needle)
  {
    for (const vix::note::NoteOutput &output : result.outputs())
    {
      if (output.kind == kind &&
          output.content.find(needle) != std::string::npos)
      {
        return true;
      }
    }

    return false;
  }
}

int main()
{
  const std::filesystem::path root = make_test_root();

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  const std::filesystem::path fakeVix =
      make_fake_vix_command(root);

  {
    vix::note::CppCellRunner runner;

    assert(runner.options().vixCommand == "vix");
    assert(runner.options().workingDirectory.empty());
    assert(runner.options().temporaryDirectory.empty());
    assert(runner.options().runArgs.empty());
    assert(!runner.options().keepTemporaryFile);
    assert(!runner.options().debugMode);
    assert(runner.options().separateStreams);
    assert(!runner.options().includeRawLog);
    assert(runner.options().enableErrorHints);
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-empty";

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("");

    assert(result.failed());
    assert(result.exit_code() == 1);
    assert(result.message() == "empty C++ cell");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
    assert(result.outputs()[0].content == "empty C++ cell");
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-success";
    options.debugMode = true;
    options.includeRawLog = true;

    vix::note::CppCellRunner runner(options);

    const std::string source =
        "#include <iostream>\n"
        "\n"
        "int main()\n"
        "{\n"
        "  std::cout << \"hello from cell\" << std::endl;\n"
        "  return 0;\n"
        "}\n";

    vix::note::NoteResult result =
        runner.run_source(source);

    assert(result.ok());
    assert(result.exit_code() == 0);
    assert(result.message() == "C++ cell executed");
    assert(result.has_outputs());

    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));

    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "fake vix run"));
    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "mode:run"));
    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "hello from cell"));

    assert(count_cpp_files(options.temporaryDirectory) == 0);
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-success-no-raw";
    options.includeRawLog = false;

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.ok());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(!has_output_kind(result, vix::note::NoteOutputKind::RawLog));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-keep";
    options.keepTemporaryFile = true;

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.ok());
    assert(count_cpp_files(options.temporaryDirectory) == 1);
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-failure";
    options.debugMode = true;
    options.includeRawLog = true;
    options.runArgs.push_back("--fail");

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.failed());
    assert(result.exit_code() == 7);
    assert(result.message() == "C++ cell failed");
    assert(result.has_outputs());

    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(has_output_kind(result, vix::note::NoteOutputKind::Error));
    assert(!has_output_kind(result, vix::note::NoteOutputKind::RuntimeError));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));

    assert(output_contains(result, vix::note::NoteOutputKind::Error, "simulated failure"));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-compiler-failure";
    options.debugMode = true;
    options.includeRawLog = true;
    options.runArgs.push_back("--compiler-fail");

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0 }\n");

    assert(result.failed());
    assert(result.exit_code() == 1);
    assert(result.message() == "C++ cell failed");

    assert(has_output_kind(result, vix::note::NoteOutputKind::CompilerError));
    assert(has_output_kind(result, vix::note::NoteOutputKind::Hint));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));

    assert(output_contains(result, vix::note::NoteOutputKind::CompilerError, "expected ';'"));
    assert(output_contains(result, vix::note::NoteOutputKind::Hint, "semicolon"));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-runtime-failure";
    options.debugMode = true;
    options.includeRawLog = true;
    options.runArgs.push_back("--runtime-fail");

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 139; }\n");

    assert(result.failed());
    assert(result.exit_code() == 139);
    assert(result.message() == "C++ cell failed");

    assert(has_output_kind(result, vix::note::NoteOutputKind::RuntimeError));
    assert(has_output_kind(result, vix::note::NoteOutputKind::Hint));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));

    assert(output_contains(result, vix::note::NoteOutputKind::RuntimeError, "segmentation fault"));
    assert(output_contains(result, vix::note::NoteOutputKind::Hint, "invalid memory access"));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-debug";
    options.debugMode = true;

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.ok());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Debug));

    assert(output_contains(result, vix::note::NoteOutputKind::Debug, "source_path="));
    assert(output_contains(result, vix::note::NoteOutputKind::Debug, "duration_ms="));
    assert(output_contains(result, vix::note::NoteOutputKind::Debug, "exit_code=0"));

    assert(count_cpp_files(options.temporaryDirectory) == 0);
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-debug-keep";
    options.debugMode = true;
    options.keepTemporaryFile = true;

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.ok());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Debug));
    assert(count_cpp_files(options.temporaryDirectory) == 1);
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-stderr-ok";
    options.runArgs.push_back("--stderr-ok");

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.ok());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stderr));
    assert(output_contains(result, vix::note::NoteOutputKind::Stderr, "simulated warning"));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-merged";
    options.debugMode = true;
    options.includeRawLog = true;
    options.separateStreams = false;

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.ok());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));
    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "fake vix run"));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-no-hints";
    options.enableErrorHints = false;
    options.runArgs.push_back("--compiler-fail");

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0 }\n");

    assert(result.failed());
    assert(has_output_kind(result, vix::note::NoteOutputKind::CompilerError));
    assert(!has_output_kind(result, vix::note::NoteOutputKind::Hint));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-cell";

    vix::note::CppCellRunner runner(options);

    vix::note::NoteCell cell =
        vix::note::NoteCell::cpp(
            "#include <iostream>\n"
            "int main()\n"
            "{\n"
            "  std::cout << \"cell ok\" << std::endl;\n"
            "  return 0;\n"
            "}\n");

    vix::note::NoteResult result =
        runner.run_cell(cell);

    assert(result.ok());
    assert(result.has_outputs());
    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "cell ok"));
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-non-cpp";

    vix::note::CppCellRunner runner(options);

    vix::note::NoteCell cell =
        vix::note::NoteCell::reply("println(\"hello\")");

    vix::note::NoteResult result =
        runner.run_cell(cell);

    assert(result.failed());
    assert(result.exit_code() == 1);
    assert(result.message() == "cell is not a C++ cell");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
    assert(result.outputs()[0].content == "cell is not a C++ cell");
  }

  {
    vix::note::CppCellRunnerOptions options;
    options.vixCommand = fakeVix.string();
    options.temporaryDirectory = root / "tmp-options";
    options.keepTemporaryFile = true;
    options.debugMode = true;
    options.separateStreams = false;
    options.includeRawLog = false;
    options.enableErrorHints = false;

    vix::note::CppCellRunner runner;
    runner.set_options(options);

    assert(runner.options().vixCommand == fakeVix.string());
    assert(runner.options().temporaryDirectory == options.temporaryDirectory);
    assert(runner.options().keepTemporaryFile);
    assert(runner.options().debugMode);
    assert(!runner.options().separateStreams);
    assert(!runner.options().includeRawLog);
    assert(!runner.options().enableErrorHints);
  }

  {
    vix::note::NoteCell cell =
        vix::note::NoteCell::markdown("not cpp");

    vix::note::NoteResult result =
        vix::note::run_cpp_cell(cell);

    assert(result.failed());
    assert(result.message() == "cell is not a C++ cell");
  }

  std::filesystem::remove_all(root);

  return 0;
}
