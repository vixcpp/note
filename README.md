# Vix Note

Visual executable notes for learning C++ and Vix.cpp faster.

Vix Note is a small notebook foundation for Vix.cpp.
It lets a document contain explanations, C++ cells, Reply cells, HTML cells, execution outputs, and exportable lessons.

The goal is not to create another terminal.

`vix repl` already gives Vix a terminal-first interactive experience.
Vix Note is different: it is UI-first. It exists to make C++ learning more visual, more readable, and easier to follow step by step.

## Why Vix Note exists

Learning C++ only inside a terminal can be hard for beginners.

A terminal is good for fast execution, but it does not naturally keep together:

- the explanation;
- the code;
- the output;
- the error;
- the lesson history;
- the exported result.

Vix Note keeps these parts in one document.

A learner can read an explanation, run the C++ cell below it, see the result, fix errors, and export the lesson as HTML.

## What Vix Note provides

Current foundation:

- `.vixnote` document format;
- markdown-compatible parser;
- markdown cells;
- C++ cells;
- Reply cells;
- HTML cells;
- document storage;
- C++ cell runner through `vix run`;
- runtime session state;
- execution records;
- note kernel;
- embedded UI assets;
- local route resolver;
- local server facade;
- HTML exporter;
- tests for each core part.

## What Vix Note is not

Vix Note is not a replacement for `vix repl`.

Use `vix repl` when you want a terminal experience.

Use Vix Note when you want a visual learning workspace where explanations, code cells, and outputs stay together.

Vix Note is also not trying to become a heavy notebook platform first.
The first direction is small, native, and Vix-focused.

## Document format

A `.vixnote` file is markdown-compatible.

Normal markdown becomes a markdown cell.

A fenced `cpp` block becomes a C++ cell:

````md
# Hello C++

This is a C++ cell.

```cpp
#include <iostream>

int main()
{
  std::cout << "Hello from Vix Note" << std::endl;
  return 0;
}
```
````

A fenced `reply` or `repl` block becomes a Reply cell:

````md
```reply
println("hello")
```
````

A fenced `html` block becomes an HTML cell:

````md
```html
<section>
  <h2>Hello</h2>
  <p>Rendered by the note UI.</p>
</section>
```
````

Unknown fenced languages are preserved as markdown.

## Basic C++ usage

```cpp
#include <vix/note/note.hpp>

#include <iostream>

int main()
{
  vix::note::NoteDocument doc("Learning C++");

  doc.add_markdown("# Learning C++");
  doc.add_cpp(
      "#include <iostream>\n"
      "int main()\n"
      "{\n"
      "  std::cout << \"Hello from Vix Note\" << std::endl;\n"
      "  return 0;\n"
      "}\n");

  std::cout << "Cells: " << doc.cell_count() << std::endl;

  return 0;
}
```

## Parsing a note

````cpp
#include <vix/note/note.hpp>

#include <iostream>
#include <string>

int main()
{
  const std::string source =
      "# Variables\n"
      "\n"
      "A variable stores a value.\n"
      "\n"
      "```cpp\n"
      "#include <iostream>\n"
      "int main() { std::cout << 42 << std::endl; }\n"
      "```\n";

  vix::note::NoteParseResult result =
      vix::note::parse_note(source);

  if (!result.ok)
  {
    std::cerr << result.error << std::endl;
    return 1;
  }

  std::cout << result.document.title() << std::endl;
  std::cout << result.document.cell_count() << " cells" << std::endl;

  return 0;
}
````

## Loading and saving

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteStore store;

  vix::note::NoteDocument doc =
      store.load_or_throw("examples/hello.vixnote");

  doc.add_markdown("New explanation.");

  store.save_or_throw(doc);

  return 0;
}
```

## Running C++ cells

C++ cells are executed by delegating to the Vix CLI.

Vix Note writes the cell source to a temporary `.cpp` file and runs:

```txt
vix run <generated-file.cpp>
```

This keeps Vix Note aligned with the normal Vix script execution model instead of creating a separate compiler pipeline.

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteCell cell =
      vix::note::NoteCell::cpp(
          "#include <iostream>\n"
          "int main()\n"
          "{\n"
          "  std::cout << \"Hello\" << std::endl;\n"
          "  return 0;\n"
          "}\n");

  vix::note::NoteResult result =
      vix::note::run_cpp_cell(cell);

  return result.exit_code();
}
```

## Kernel

`NoteKernel` coordinates execution.

It owns a `NoteSession`, runs executable cells, applies outputs, and tracks execution counts.

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteDocument doc;

  doc.add_markdown("# Lesson");
  doc.add_cpp("int main() { return 0; }");

  vix::note::NoteKernel kernel(doc);

  vix::note::NoteKernelRunResult result =
      kernel.run_all();

  return result.ok ? 0 : 1;
}
```

## Local UI direction

Vix Note has a UI-first direction.

The current server layer is intentionally a facade:

- `NoteAssets` stores embedded UI assets;
- `NoteRoutes` maps UI and API paths;
- `NoteServer` owns routes and lifecycle state.

The first version does not force a concrete HTTP backend.
Later, this can be connected to the real Vix HTTP server layer.

Expected direction:

```txt
Vix Note document
        ↓
NoteParser / NoteStore
        ↓
NoteKernel
        ↓
NoteRoutes
        ↓
Vix HTTP server
        ↓
Browser or WebView UI
```

## HTML export

Vix Note can export a document as standalone HTML.

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteDocument doc("Exported Lesson");

  doc.add_markdown("# Exported Lesson");
  doc.add_cpp("int main() { return 0; }");

  vix::note::NoteResult result =
      vix::note::export_note_html_file(doc, "lesson.html");

  return result.ok() ? 0 : 1;
}
```

## Build

Standalone:

```bash
vix build -- -DVIX_NOTE_BUILD_TESTS=ON
vix tests
```

Inside the Vix umbrella build, tests and examples are disabled by default.

## Options

```txt
VIX_NOTE_BUILD_TESTS       Build Vix Note tests
VIX_NOTE_BUILD_EXAMPLES    Build Vix Note examples
VIX_NOTE_ENABLE_LTO        Enable LTO in Release builds
VIX_NOTE_INSTALL           Enable standalone install/package rules
```

## Repository layout

```txt
include/vix/note/
  core/
  parser/
  storage/
  runtime/
  web/
  export/
  note.hpp
  Version.hpp

src/
  core/
  parser/
  storage/
  runtime/
  web/
  export/

assets/
  index.html
  css/note.css
  js/note.js

examples/
  hello.vixnote
  learning_cpp.vixnote
  vix_ui_note.vixnote

tests/
```

## Examples

Available example notes:

```txt
examples/hello.vixnote
examples/learning_cpp.vixnote
examples/vix_ui_note.vixnote
```

## Current status

Vix Note is in early development.

The current version focuses on the foundation:

- document model;
- parser;
- storage;
- runtime session;
- kernel;
- local UI route/server abstraction;
- HTML export.

The next step is to connect the UI server facade to the real Vix server layer and expose a simple command such as:

```bash
vix note examples/hello.vixnote
```

## License

MIT License.

Copyright 2026, Gaspard Kirira.
