# Vix Note

Visual executable notes for learning C++ and Vix.cpp faster.

Vix Note is a small, native notebook foundation for Vix.cpp. It lets a document contain explanations, C++ cells, Reply cells, HTML cells, execution outputs, project context, local UI assets, and exportable lessons.

Vix Note is not another terminal experience.

`vix repl` already covers the terminal-first workflow. Vix Note is UI-first: it exists to make C++ learning more visual, more readable, and easier to follow step by step.

## Quick start

Open a note in the local browser UI:

```bash
vix note examples/cpp_basics.vixnote
```

Expected output:

```txt
✔ Vix Note started.
Open this URL in your browser:
  • http://127.0.0.1:5179/

Project context:
  • examples

➜ Press Ctrl+C to stop the note server.
```

Then open:

```txt
http://127.0.0.1:5179/
```

Use a custom port:

```bash
vix note examples/cpp_basics.vixnote --port 5180
```

Use a custom host and port:

```bash
vix note examples/cpp_basics.vixnote --host 127.0.0.1 --port 5179
```

Export a note to HTML:

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html
```

Export without saved outputs:

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html --no-outputs
```

## Why Vix Note exists

Learning C++ only through terminal output can be difficult for beginners.

A terminal is good for fast execution, but it does not naturally keep together:

- the explanation;
- the code;
- the output;
- the error;
- the project context;
- the lesson structure;
- the exported result.

Vix Note keeps these parts in one visual document.

A learner can read an explanation, run the C++ cell below it, see the result near the code, fix errors, save the document, and export the lesson as HTML.

## What Vix Note provides

Vix Note v1.0.0 provides:

- `.vixnote` document format;
- markdown-compatible parser;
- markdown cells;
- C++ cells;
- Reply cells;
- HTML cells;
- stable cell metadata;
- document loading and saving;
- C++ cell execution through `vix run`;
- Reply cell execution through the embedded Reply runtime;
- runtime session state;
- execution records;
- note kernel;
- project-aware note execution;
- project root detection;
- include path support;
- embedded UI assets;
- installed UI asset loading;
- custom UI asset directory support;
- local route resolver;
- local HTTP server;
- browser-ready UI;
- static HTML exporter;
- modern learning examples;
- tests for the core module parts;
- Vix CLI integration through `vix note`.

## What Vix Note is not

Vix Note is not a replacement for `vix repl`.

Use `vix repl` when you want an interactive terminal workflow.

Use Vix Note when you want a visual learning workspace where explanations, code cells, errors, and outputs stay together.

Vix Note is also not trying to become a heavy notebook platform. The first stable release stays small, native, and focused on the Vix.cpp tooling direction.

## CLI usage

```bash
vix note <file.vixnote> [options]
vix note export <file.vixnote> --out <file.html> [options]
```

### Open a note

```bash
vix note examples/cpp_basics.vixnote
```

### Open on another port

```bash
vix note examples/cpp_basics.vixnote --port 5180
```

### Open with host and port

```bash
vix note examples/cpp_basics.vixnote --host 127.0.0.1 --port 5179
```

### Export to HTML

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html
```

### Export without outputs

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html --no-outputs
```

### Export with outputs

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html --with-outputs
```

## CLI options

```txt
vix note <file.vixnote> [options]

Options:
  --host <host>       Host used by the local server. Default: 127.0.0.1
  --host=<host>       Same as --host <host>
  --port <port>       Port used by the local server. Default: 5179
  --port=<port>       Same as --port <port>
  -h, --help          Show help
```

```txt
vix note export <file.vixnote> --out <file.html> [options]

Options:
  --out <file.html>   Output HTML file
  --out=<file.html>   Same as --out <file.html>
  --no-outputs        Export without cell outputs
  --with-outputs      Export with cell outputs
  -h, --help          Show help
```

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
x = 1 + 2 * 3
println("x =", x)
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

## Stable cell metadata

Vix Note preserves stable cell ids using markdown-compatible metadata comments:

````md
<!-- vixnote:cell id="intro" kind="markdown" -->

# Introduction

<!-- vixnote:cell id="hello-cpp" kind="cpp" title="Hello C++" -->

```cpp
#include <iostream>

int main()
{
  std::cout << "Hello" << std::endl;
  return 0;
}
```
````

These comments keep `.vixnote` files readable while allowing the UI and runtime to track cells across save/load cycles.

## Basic C++ API usage

```cpp
#include <vix/note/note.hpp>

#include <iostream>

int main()
{
  vix::note::NoteDocument doc("Learning C++");

  doc.add_markdown("# Learning C++");

  doc.add_cpp(
      "#include <iostream>\n"
      "\n"
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
      "\n"
      "int main()\n"
      "{\n"
      "  std::cout << 42 << std::endl;\n"
      "  return 0;\n"
      "}\n"
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
      store.load_or_throw("examples/cpp_basics.vixnote");

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

This keeps Vix Note aligned with normal Vix execution instead of creating a separate compiler pipeline.

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteCell cell =
      vix::note::NoteCell::cpp(
          "#include <iostream>\n"
          "\n"
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

## Running Reply cells

Reply cells are executed through the embedded Vix Reply runtime.

They are useful for small expressions inside a visual lesson.

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteCell cell =
      vix::note::NoteCell::reply(
          "x = 1 + 2 * 3\n"
          "println(\"x =\", x)\n");

  vix::note::NoteResult result =
      vix::note::run_reply_cell(cell);

  return result.ok() ? 0 : 1;
}
```

Reply cells support small interactive helpers such as:

```reply
println("Hello")
print("Vix ")
println("Note")

calc 1 + 2 * 3
= (10 + 5) * 2

name = "Vix Note"
version = 1
ready = true

println("name =", name)
type(version)

scores = [10, 20, 30]
println(scores[0])
println(len(scores))

user = {"name":"Gaspard","language":"C++"}
println(user.name)

cwd()
pid()
Vix.cwd()
Vix.pid()
Vix.args()
```

Reply cells should stay small. For full interactive terminal work, use `vix repl`.

## Kernel

`NoteKernel` coordinates execution.

It owns a `NoteSession`, runs executable cells, applies outputs, updates execution counts, and records execution history.

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

## Project-aware notes

Vix Note can detect project context from a note path.

It can detect:

- project root;
- `vix.app`;
- `.vix/manifest.vix`;
- `.vix/deps`;
- project include directories;
- working directory;
- project name.

```cpp
#include <vix/note/note.hpp>

#include <iostream>

int main()
{
  vix::note::ProjectContext context =
      vix::note::detect_project_context("examples/cpp_basics.vixnote");

  if (context.enabled)
  {
    std::cout << "Project: " << context.projectName << std::endl;
    std::cout << "Root: " << context.projectRoot << std::endl;
  }

  return 0;
}
```

Project context is automatically used by the `vix note` command when opening a `.vixnote` file from inside a project.

## Local UI

Vix Note includes a small local UI layer.

The UI is served through:

- `NoteAssets`;
- `NoteRoutes`;
- `NoteServer`;
- `vix note`.

The command:

```bash
vix note examples/cpp_basics.vixnote
```

loads the document, detects project context, starts a local note server, and prints the URL to open in the browser.

Main API routes include:

```txt
GET     /api/document
POST    /api/cells
PUT     /api/cells/<id>
DELETE  /api/cells/<id>
POST    /api/cells/<id>/run
POST    /api/cells/<id>/move
POST    /api/run-all
POST    /api/document/save
```

Static UI routes include:

```txt
/
/index.html
/assets/note.css
/assets/note.js
```

## Starting a local server from C++

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteStore store;

  vix::note::NoteDocument doc =
      store.load_or_throw("examples/cpp_basics.vixnote");

  vix::note::NoteServerOptions options;
  options.host = "127.0.0.1";
  options.port = 5179;
  options.openBrowser = false;

  vix::note::NoteServer server(doc, options);

  vix::note::NoteResult started =
      server.start();

  if (!started.ok())
  {
    return 1;
  }

  return server.wait().ok() ? 0 : 1;
}
```

For normal use, prefer the CLI:

```bash
vix note examples/cpp_basics.vixnote
```

## UI asset loading

Vix Note supports three UI asset modes:

- embedded assets;
- installed assets;
- custom asset directory.

The expected asset directory layout is:

```txt
index.html
assets/note.css
assets/note.js
```

The default installed asset directory is configured at build/install time.

A custom asset directory can be provided through route options:

```cpp
#include <vix/note/note.hpp>

int main()
{
  vix::note::NoteRoutesOptions options;
  options.assetDirectory = "custom-ui";
  options.loadInstalledAssets = true;
  options.keepEmbeddedAssetFallback = true;

  vix::note::NoteRoutes routes(options);

  return routes.assets().contains("/") ? 0 : 1;
}
```

The environment variable `VIX_NOTE_ASSETS_DIR` can also be used to point to a UI asset directory.

Embedded assets remain available as fallback.

## HTML export

Vix Note can export a document as standalone HTML.

From the CLI:

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html
```

From C++:

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

Exporter options allow control over:

- standalone HTML;
- outputs;
- cell titles;
- execution counts;
- document metadata;
- table of contents;
- output labels;
- print-friendly CSS;
- custom CSS.

## Build

Standalone build:

```bash
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

With Vix:

```bash
vix build
vix tests
```

## CMake options

```txt
VIX_NOTE_BUILD_TESTS       Build Vix Note tests
VIX_NOTE_BUILD_EXAMPLES    Build Vix Note examples
VIX_NOTE_ENABLE_LTO        Enable LTO in Release builds
VIX_NOTE_INSTALL           Enable standalone install/package rules
```

## Install

Standalone install:

```bash
cmake --install build-ninja
```

Vix Note installs:

```txt
include/
library archive
CMake package files
UI assets
```

Installed UI assets are placed under:

```txt
${CMAKE_INSTALL_DATADIR}/vix/note/assets
```

## Repository layout

```txt
include/vix/note/
  core/
  export/
  note.hpp
  parser/
  project/
  runtime/
  storage/
  Version.hpp
  web/

src/
  core/
  export/
  parser/
  project/
  runtime/
  storage/
  web/

assets/
  index.html
  assets/note.css
  assets/note.js

examples/
  cpp_basics.vixnote
  cpp_classes.vixnote
  cpp_conditionals.vixnote
  cpp_functions.vixnote
  cpp_loops.vixnote
  cpp_standard_library.vixnote
  cpp_structs.vixnote
  cpp_variables.vixnote
  cpp_vectors.vixnote
  html_cells.vixnote
  reply_basics.vixnote
  reply_calculator.vixnote
  reply_json_values.vixnote
  reply_quick_cells.vixnote
  reply_runtime_helpers.vixnote
  reply_variables.vixnote

tests/
  cpp_cell_runner_test.cpp
  html_exporter_test.cpp
  note_assets_test.cpp
  note_cell_test.cpp
  note_document_test.cpp
  note_error_test.cpp
  note_kernel_test.cpp
  note_parser_test.cpp
  note_result_test.cpp
  note_routes_test.cpp
  note_server_test.cpp
  note_session_test.cpp
  note_store_test.cpp
```

## Examples

### C++ learning notes

```txt
examples/cpp_basics.vixnote
examples/cpp_variables.vixnote
examples/cpp_functions.vixnote
examples/cpp_conditionals.vixnote
examples/cpp_loops.vixnote
examples/cpp_vectors.vixnote
examples/cpp_structs.vixnote
examples/cpp_classes.vixnote
examples/cpp_standard_library.vixnote
```

### Reply learning notes

```txt
examples/reply_quick_cells.vixnote
examples/reply_basics.vixnote
examples/reply_calculator.vixnote
examples/reply_variables.vixnote
examples/reply_json_values.vixnote
examples/reply_runtime_helpers.vixnote
```

### UI / HTML notes

```txt
examples/html_cells.vixnote
```

Open one:

```bash
vix note examples/cpp_basics.vixnote
```

Open a Reply example:

```bash
vix note examples/reply_calculator.vixnote
```

Export one:

```bash
vix note export examples/cpp_basics.vixnote --out cpp_basics.html
```

## Design rules

Vix Note follows these rules:

```txt
Prefer simple features.
Avoid duplicating vix repl.
Keep the UI useful, not decorative.
Keep execution aligned with vix run.
Keep files readable as markdown.
Keep exports clean.
Keep the module easy to test.
```

## Status

Vix Note v1.0.0 is the first stable release of the module.

It provides the stable foundation for visual executable notes in Vix.cpp:

- readable `.vixnote` documents;
- editable cells;
- C++ execution;
- Reply execution;
- local UI through `vix note`;
- save/load;
- HTML export;
- project-aware execution;
- packaged UI assets;
- stable public C++ API.

## License

MIT License.

Copyright 2026, Gaspard Kirira.
