/**
 *
 *  @file note_store_test.cpp
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
#include <vix/note/core/NoteError.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/storage/NoteStore.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
  std::filesystem::path make_test_root()
  {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("vix-note-store-test-" + std::to_string(now));
  }

  std::string read_file(const std::filesystem::path &path)
  {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }

  void write_file(const std::filesystem::path &path, const std::string &content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }
}

int main()
{
  const std::filesystem::path root = make_test_root();

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  {
    vix::note::NoteStore store;

    vix::note::NoteDocument doc("Learning C++");
    doc.add_markdown("# Learning C++");
    doc.add_reply("x = 1 + 2\nprintln(x)");
    doc.add_cpp("int main() { return 0; }");
    doc.add_html("<section>Hello</section>");

    const std::string serialized =
        store.serialize(doc);

    const std::string expected =
        "# Learning C++\n"
        "\n"
        "```reply\n"
        "x = 1 + 2\n"
        "println(x)\n"
        "```\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n"
        "\n"
        "```html\n"
        "<section>Hello</section>\n"
        "```\n";

    assert(serialized == expected);
  }

  {
    vix::note::NoteStore store;

    vix::note::NoteDocument doc("Saved Note");
    doc.add_markdown("# Saved Note");
    doc.add_reply("println(\"hello\")");

    const std::filesystem::path file =
        root / "notes" / "saved.vixnote";

    vix::note::NoteResult result =
        store.save(doc, file);

    assert(result.ok());
    assert(result.message() == "note saved");
    assert(result.has_outputs());
    assert(result.outputs().size() == 1);
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Text);
    assert(result.outputs()[0].content == file.string());

    assert(std::filesystem::exists(file));

    const std::string content = read_file(file);

    const std::string expected =
        "# Saved Note\n"
        "\n"
        "```reply\n"
        "println(\"hello\")\n"
        "```\n";

    assert(content == expected);
  }

  {
    vix::note::NoteStore store;

    const std::filesystem::path file =
        root / "notes" / "load.vixnote";

    write_file(
        file,
        "# Loaded Note\n"
        "\n"
        "Some explanation.\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n");

    vix::note::NoteLoadResult result =
        store.load(file);

    assert(result.ok);
    assert(!result.has_error());
    assert(result.error.empty());

    assert(result.document.path() == file.string());
    assert(result.document.title() == "Loaded Note");
    assert(result.document.cell_count() == 2);

    assert(result.document.cells()[0].kind() == vix::note::NoteCellKind::Markdown);
    assert(result.document.cells()[1].kind() == vix::note::NoteCellKind::Cpp);
    assert(result.document.cells()[1].source() == "int main() { return 0; }");
  }

  {
    vix::note::NoteStore store;

    const std::filesystem::path file =
        root / "notes" / "throw-load.vixnote";

    write_file(
        file,
        "# Throw Load\n"
        "\n"
        "```reply\n"
        "println(\"ok\")\n"
        "```\n");

    vix::note::NoteDocument doc =
        store.load_or_throw(file);

    assert(doc.path() == file.string());
    assert(doc.title() == "Throw Load");
    assert(doc.cell_count() == 2);
    assert(doc.cells()[1].kind() == vix::note::NoteCellKind::Reply);
  }

  {
    vix::note::NoteStore store;

    vix::note::NoteLoadResult result =
        store.load(root / "missing.vixnote");

    assert(!result.ok);
    assert(result.has_error());
    assert(!result.error.empty());
  }

  {
    vix::note::NoteStore store;

    bool thrown = false;

    try
    {
      (void)store.load_or_throw(root / "missing-throw.vixnote");
    }
    catch (const vix::note::NoteError &error)
    {
      thrown = true;

      assert(error.code() == vix::note::NoteErrorCode::Read);
      assert(std::string(error.what()).find("cannot open note file") != std::string::npos);
    }

    assert(thrown);
  }

  {
    vix::note::NoteStore store;

    const std::filesystem::path file =
        root / "notes" / "broken.vixnote";

    write_file(
        file,
        "# Broken\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n");

    vix::note::NoteLoadResult result =
        store.load(file);

    assert(!result.ok);
    assert(result.has_error());
    assert(result.error == "unterminated fenced cell starting at line 3");
  }

  {
    vix::note::NoteStore store;

    vix::note::NoteDocument doc;
    doc.set_path((root / "notes" / "path-save.vixnote").string());
    doc.add_markdown("# Path Save");

    vix::note::NoteResult result =
        store.save(doc);

    assert(result.ok());
    assert(std::filesystem::exists(doc.path()));

    vix::note::NoteDocument loaded =
        store.load_or_throw(doc.path());

    assert(loaded.title() == "Path Save");
    assert(loaded.cell_count() == 1);
  }

  {
    vix::note::NoteStore store;

    vix::note::NoteDocument doc;
    doc.add_markdown("# No Path");

    vix::note::NoteResult result =
        store.save(doc);

    assert(result.failed());
    assert(result.message() == "note document has no path");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteStore store;

    vix::note::NoteDocument doc;
    doc.add_markdown("# Empty Path");

    vix::note::NoteResult result =
        store.save(doc, {});

    assert(result.failed());
    assert(result.message() == "empty note path");
    assert(result.has_outputs());
  }

  {
    vix::note::NoteStoreOptions options;
    options.atomicWrite = false;
    options.createParentDirectories = true;

    vix::note::NoteStore store(options);

    assert(!store.options().atomicWrite);
    assert(store.options().createParentDirectories);

    vix::note::NoteDocument doc;
    doc.add_markdown("# Direct Write");

    const std::filesystem::path file =
        root / "direct" / "direct-write.vixnote";

    vix::note::NoteResult result =
        store.save(doc, file);

    assert(result.ok());
    assert(std::filesystem::exists(file));
    assert(read_file(file) == "# Direct Write\n");
  }

  {
    vix::note::NoteStoreOptions options;
    options.parseOptions.inferTitle = false;

    vix::note::NoteStore store(options);

    const std::filesystem::path file =
        root / "notes" / "no-title.vixnote";

    write_file(
        file,
        "# Title Should Not Be Inferred\n"
        "\n"
        "Content.\n");

    vix::note::NoteLoadResult result =
        store.load(file);

    assert(result.ok);
    assert(!result.document.has_title());
    assert(result.document.title().empty());
    assert(result.document.cell_count() == 1);
  }

  {
    vix::note::NoteDocument doc;
    doc.add_markdown("# Free Function Save");
    doc.add_cpp("int main() { return 0; }");

    const std::filesystem::path file =
        root / "free" / "free-save.vixnote";

    vix::note::NoteResult saveResult =
        vix::note::save_note(doc, file);

    assert(saveResult.ok());

    vix::note::NoteLoadResult loadResult =
        vix::note::load_note(file);

    assert(loadResult.ok);
    assert(loadResult.document.title() == "Free Function Save");
    assert(loadResult.document.cell_count() == 2);

    const std::string serialized =
        vix::note::serialize_note(loadResult.document);

    assert(serialized.find("```cpp") != std::string::npos);
  }

  {
    const std::filesystem::path file =
        root / "free" / "load-or-throw.vixnote";

    write_file(
        file,
        "# Free Load\n"
        "\n"
        "```reply\n"
        "println(\"loaded\")\n"
        "```\n");

    vix::note::NoteDocument doc =
        vix::note::load_note_or_throw(file);

    assert(doc.title() == "Free Load");
    assert(doc.cell_count() == 2);
    assert(doc.cells()[1].kind() == vix::note::NoteCellKind::Reply);
  }

  std::filesystem::remove_all(root);

  return 0;
}
