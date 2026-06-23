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
    assert(result.outputs().size() == 1);
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Stdout);

    assert(result.outputs()[0].content.find("fake vix run") != std::string::npos);
    assert(result.outputs()[0].content.find("mode:run") != std::string::npos);
    assert(result.outputs()[0].content.find("hello from cell") != std::string::npos);

    assert(count_cpp_files(options.temporaryDirectory) == 0);
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
    options.runArgs.push_back("--fail");

    vix::note::CppCellRunner runner(options);

    vix::note::NoteResult result =
        runner.run_source("int main() { return 0; }\n");

    assert(result.failed());
    assert(result.exit_code() == 7);
    assert(result.message() == "C++ cell failed");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
    assert(result.outputs()[0].content.find("simulated failure") != std::string::npos);
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
    assert(result.outputs()[0].content.find("cell ok") != std::string::npos);
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

    vix::note::CppCellRunner runner;
    runner.set_options(options);

    assert(runner.options().vixCommand == fakeVix.string());
    assert(runner.options().temporaryDirectory == options.temporaryDirectory);
    assert(runner.options().keepTemporaryFile);
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
