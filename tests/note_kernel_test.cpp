/**
 *
 *  @file note_kernel_test.cpp
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
#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/runtime/NoteKernel.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
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
           ("vix-note-kernel-test-" + std::to_string(now));
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
        "  echo simulated kernel failure\n"
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
        "  echo simulated kernel failure\n"
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

  vix::note::NoteKernelOptions make_kernel_options(
      const std::filesystem::path &fakeVix,
      const std::filesystem::path &tempDir)
  {
    vix::note::NoteKernelOptions options;
    options.cppOptions.vixCommand = fakeVix.string();
    options.cppOptions.temporaryDirectory = tempDir;
    return options;
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
    vix::note::NoteKernel kernel;

    assert(kernel.cell_count() == 0);
    assert(!kernel.has_cell(0));
    assert(!kernel.can_execute_cell(0));
    assert(kernel.document().empty());
    assert(kernel.session().empty());

    assert(!kernel.options().stopOnFirstFailure);
    assert(!kernel.options().includeNonExecutableAsSkipped);
    assert(!kernel.can_execute_cell("missing"));
  }

  {
    vix::note::NoteDocument doc("Learning C++");
    doc.add_markdown("# Learning C++");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteKernel kernel(doc);

    assert(kernel.cell_count() == 2);
    assert(kernel.document().title() == "Learning C++");

    assert(kernel.has_cell(0));
    assert(kernel.has_cell(1));
    assert(!kernel.has_cell(2));

    assert(!kernel.can_execute_cell(0));
    assert(kernel.can_execute_cell(1));
  }

  {
    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-options");

    options.stopOnFirstFailure = true;
    options.includeNonExecutableAsSkipped = true;
    options.sessionOptions.clearOutputsBeforeRun = false;

    vix::note::NoteKernel kernel(options);

    assert(kernel.options().stopOnFirstFailure);
    assert(kernel.options().includeNonExecutableAsSkipped);
    assert(!kernel.session().options().clearOutputsBeforeRun);
    assert(kernel.session().options().stopOnFirstFailure);
  }

  {
    vix::note::NoteKernel kernel;

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-set-options");

    options.stopOnFirstFailure = true;
    options.sessionOptions.clearOutputsBeforeRun = false;

    kernel.set_options(options);

    assert(kernel.options().stopOnFirstFailure);
    assert(!kernel.session().options().clearOutputsBeforeRun);
    assert(kernel.session().options().stopOnFirstFailure);
  }

  {
    vix::note::NoteKernel kernel;

    vix::note::NoteDocument doc("New Document");
    doc.add_cpp("int main() { return 0; }");

    kernel.set_document(doc);

    assert(kernel.cell_count() == 1);
    assert(kernel.document().title() == "New Document");
    assert(kernel.can_execute_cell(0));
    assert(!kernel.session().has_records());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "intro",
            vix::note::NoteCellKind::Markdown,
            "# Intro"));

    doc.add_cell(
        vix::note::NoteCell(
            "run",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteKernel kernel(doc);

    std::optional<std::size_t> intro =
        kernel.cell_index("intro");

    std::optional<std::size_t> run =
        kernel.cell_index("run");

    std::optional<std::size_t> missing =
        kernel.cell_index("missing");

    assert(intro.has_value());
    assert(run.has_value());
    assert(!missing.has_value());

    assert(*intro == 0);
    assert(*run == 1);
    assert(!kernel.can_execute_cell("intro"));
    assert(kernel.can_execute_cell("run"));
    assert(!kernel.can_execute_cell("missing"));
  }

  {
    vix::note::NoteDocument doc;
    doc.add_markdown("# Markdown");

    vix::note::NoteKernel kernel(doc);

    vix::note::NoteResult result =
        kernel.run_cell(0);

    assert(result.was_skipped());
    assert(result.message() == "cell is not executable");

    assert(!kernel.session().has_records());
    assert(kernel.document().cells()[0].execution_count() == 0);
    assert(!kernel.document().cells()[0].has_outputs());
  }

  {
    vix::note::NoteKernel kernel;

    vix::note::NoteResult result =
        kernel.run_cell(99);

    assert(result.failed());
    assert(result.message() == "cell index out of range");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "reply",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteKernel kernel(doc);

    vix::note::NoteResult result =
        kernel.run_cell("reply");

    assert(result.ok());
    assert(result.message() == "Reply cell executed");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Stdout);
    assert(result.outputs()[0].content.find("hello") != std::string::npos);

    const vix::note::NoteCell &cell =
        kernel.document().cells()[0];

    assert(cell.execution_count() == 1);
    assert(cell.has_outputs());

    assert(kernel.session().has_records());
    assert(kernel.session().records().size() == 1);
    assert(kernel.session().records()[0].cellId == "reply");
    assert(kernel.session().records()[0].executionCount == 1);
    assert(kernel.session().records()[0].result.ok());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_reply("println(\"hello\")");

    vix::note::NoteKernel kernel(doc);

    vix::note::NoteResult result =
        kernel.run_cell("missing");

    assert(result.failed());
    assert(result.message() == "cell not found: missing");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp",
            vix::note::NoteCellKind::Cpp,
            "#include <iostream>\n"
            "int main()\n"
            "{\n"
            "  std::cout << \"kernel cpp ok\" << std::endl;\n"
            "  return 0;\n"
            "}\n"));

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-cpp-success");
    options.cppOptions.debugMode = true;
    options.cppOptions.includeRawLog = true;

    vix::note::NoteKernel kernel(doc, options);

    vix::note::NoteResult result =
        kernel.run_cell("cpp");

    assert(result.ok());
    assert(result.message() == "C++ cell executed");
    assert(result.has_outputs());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));

    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "fake vix run"));
    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "kernel cpp ok"));

    const vix::note::NoteCell &cell =
        kernel.document().cells()[0];

    assert(cell.execution_count() == 1);
    assert(cell.has_outputs());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(output_contains(result, vix::note::NoteOutputKind::Stdout, "kernel cpp ok"));

    assert(kernel.session().records().size() == 1);
    assert(kernel.session().records()[0].result.ok());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp",
            vix::note::NoteCellKind::Cpp,
            "int main() { return 0; }\n"));

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-cpp-failure");
    options.cppOptions.debugMode = true;
    options.cppOptions.includeRawLog = true;

    options.cppOptions.runArgs.push_back("--fail");

    vix::note::NoteKernel kernel(doc, options);

    vix::note::NoteResult result =
        kernel.run_cell(0);

    assert(result.failed());
    assert(result.exit_code() == 7);
    assert(result.message() == "C++ cell failed");
    assert(result.has_outputs());
    assert(has_output_kind(result, vix::note::NoteOutputKind::Stdout));
    assert(has_output_kind(result, vix::note::NoteOutputKind::Error));
    assert(!has_output_kind(result, vix::note::NoteOutputKind::RuntimeError));
    assert(has_output_kind(result, vix::note::NoteOutputKind::RawLog));

    assert(output_contains(result, vix::note::NoteOutputKind::Error, "simulated kernel failure"));

    const vix::note::NoteCell &cell =
        kernel.document().cells()[0];

    assert(cell.execution_count() == 1);
    assert(cell.has_outputs());
    assert(output_contains(result, vix::note::NoteOutputKind::Error, "simulated kernel failure"));

    assert(kernel.session().records().size() == 1);
    assert(kernel.session().records()[0].result.failed());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Intro");
    doc.add_reply("println(\"reply\")");
    doc.add_cpp(
        "#include <iostream>\n"
        "int main()\n"
        "{\n"
        "  std::cout << \"cpp one\" << std::endl;\n"
        "  return 0;\n"
        "}\n");
    doc.add_html("<p>render only</p>");

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-run-all");

    vix::note::NoteKernel kernel(doc, options);

    vix::note::NoteKernelRunResult result =
        kernel.run_all();

    assert(result.ok);
    assert(!result.stopped);
    assert(result.visited == 4);
    assert(result.executed == 2);
    assert(result.skipped == 0);
    assert(result.failed == 0);
    assert(result.results.size() == 2);
    assert(result.has_results());
    assert(!result.has_failures());
    assert(!result.has_skipped());

    assert(result.results[0].ok());
    assert(result.results[1].ok());

    assert(kernel.session().records().size() == 2);
    assert(kernel.document().cells()[1].execution_count() == 1);
    assert(kernel.document().cells()[2].execution_count() == 2);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Intro");
    doc.add_reply("println(\"reply\")");
    doc.add_html("<p>render only</p>");

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-run-all-skipped");

    options.includeNonExecutableAsSkipped = true;

    vix::note::NoteKernel kernel(doc, options);

    vix::note::NoteKernelRunResult result =
        kernel.run_all();

    assert(result.ok);
    assert(!result.stopped);
    assert(result.visited == 3);
    assert(result.executed == 1);
    assert(result.skipped == 2);
    assert(result.failed == 0);
    assert(result.has_skipped());
    assert(!result.has_failures());
    assert(result.results.size() == 3);

    assert(result.results[0].was_skipped());
    assert(result.results[1].ok());
    assert(result.results[2].was_skipped());

    assert(kernel.session().records().size() == 1);
    assert(kernel.document().cells()[1].execution_count() == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cpp("int main() { return 0; }\n");
    doc.add_cpp("int main() { return 0; }\n");

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-stop-first-failure");

    options.stopOnFirstFailure = true;
    options.cppOptions.runArgs.push_back("--fail");

    vix::note::NoteKernel kernel(doc, options);

    vix::note::NoteKernelRunResult result =
        kernel.run_all();

    assert(!result.ok);
    assert(result.stopped);
    assert(result.visited == 1);
    assert(result.executed == 1);
    assert(result.skipped == 0);
    assert(result.failed == 1);
    assert(result.results.size() == 1);
    assert(result.has_failures());

    assert(kernel.session().records().size() == 1);
    assert(kernel.document().cells()[0].execution_count() == 1);
    assert(kernel.document().cells()[1].execution_count() == 0);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Intro");
    doc.add_cpp(
        "#include <iostream>\n"
        "int main()\n"
        "{\n"
        "  std::cout << \"only executable\" << std::endl;\n"
        "  return 0;\n"
        "}\n");
    doc.add_html("<p>render only</p>");

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-run-executable");

    vix::note::NoteKernel kernel(doc, options);

    vix::note::NoteKernelRunResult result =
        kernel.run_executable_cells();

    assert(result.ok);
    assert(!result.stopped);
    assert(result.visited == 1);
    assert(result.executed == 1);
    assert(result.skipped == 0);
    assert(result.failed == 0);
    assert(!result.has_skipped());
    assert(!result.has_failures());
    assert(result.results.size() == 1);
    assert(result.results[0].ok());

    assert(kernel.session().records().size() == 1);
    assert(kernel.document().cells()[0].execution_count() == 0);
    assert(kernel.document().cells()[1].execution_count() == 1);
    assert(kernel.document().cells()[2].execution_count() == 0);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_reply("println(\"hello\")")
        .add_output(vix::note::NoteOutput::stdout_text("old\n"));

    doc.add_cpp("int main() { return 0; }")
        .add_output(vix::note::NoteOutput::stdout_text("old cpp\n"));

    vix::note::NoteKernel kernel(doc);

    assert(kernel.document().cells()[0].has_outputs());
    assert(kernel.document().cells()[1].has_outputs());

    kernel.clear_outputs();

    assert(!kernel.document().cells()[0].has_outputs());
    assert(!kernel.document().cells()[1].has_outputs());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_reply("println(\"hello\")");

    vix::note::NoteKernel kernel(doc);

    (void)kernel.run_cell(0);

    assert(kernel.document().execution_count() == 1);
    assert(kernel.document().cells()[0].execution_count() == 1);
    assert(kernel.session().has_records());

    kernel.reset_execution();

    assert(kernel.document().execution_count() == 0);
    assert(kernel.document().cells()[0].execution_count() == 0);
    assert(!kernel.session().has_records());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_reply("println(\"hello\")")
        .add_output(vix::note::NoteOutput::stdout_text("hello\n"));

    vix::note::NoteKernel kernel(doc);

    (void)kernel.run_cell(0);

    assert(kernel.document().cells()[0].execution_count() == 1);
    assert(kernel.session().has_records());

    kernel.reset();

    assert(kernel.document().execution_count() == 0);
    assert(kernel.document().cells()[0].execution_count() == 0);
    assert(!kernel.document().cells()[0].has_outputs());
    assert(!kernel.session().has_records());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_markdown("# Free Function");

    vix::note::NoteKernelRunResult result =
        vix::note::run_note(doc);

    assert(result.ok);
    assert(!result.stopped);
    assert(result.visited == 1);
    assert(result.executed == 0);
    assert(!result.has_results());
  }

  {
    vix::note::NoteDocument doc;
    doc.add_markdown("# Free Cell");

    vix::note::NoteResult result =
        vix::note::run_note_cell(doc, 0);

    assert(result.was_skipped());
    assert(result.message() == "cell is not executable");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "free-id",
            vix::note::NoteCellKind::Markdown,
            "# Free Cell By Id"));

    vix::note::NoteResult result =
        vix::note::run_note_cell(doc, "free-id");

    assert(result.was_skipped());
    assert(result.message() == "cell is not executable");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "missing-test",
            vix::note::NoteCellKind::Cpp,
            "int main() { return 0; }\n"));

    vix::note::NoteResult result =
        vix::note::run_note_cell(doc, "missing");

    assert(result.failed());
    assert(result.message() == "cell not found: missing");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  std::filesystem::remove_all(root);

  return 0;
}
