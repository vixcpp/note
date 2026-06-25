# Vix Note Roadmap

Vix Note is a UI-first notebook foundation for learning C++ and Vix.cpp faster.

The goal is not to create another terminal experience. `vix repl` already covers the terminal-first workflow.

Vix Note exists for a different reason: to keep explanations, code cells, outputs, errors, project context, and exported lessons together in one visual workspace.

## Direction

Vix Note should stay small, native, and focused.

The stable direction is:

```txt
learn C++ faster
write explanations
run small C++ cells
run small Reply cells
see outputs near the code
save readable .vixnote files
open notes with vix note
export lessons as HTML
work inside Vix projects
package the UI cleanly
```

Before adding a feature, we should ask:

```txt
Does this make learning C++ clearer?
Does this avoid repeating what vix repl already does?
Does this keep the module simple?
Does this fit the Vix.cpp tooling direction?
```

## v0.1.0 — Foundation

Status: completed.

Main goal: create the core module architecture.

Completed:

- core error type;
- result and output model;
- note cell model;
- note document model;
- markdown-compatible parser;
- `.vixnote` storage;
- C++ cell runner through `vix run`;
- runtime session;
- note kernel;
- embedded web assets;
- route resolver;
- server facade;
- static HTML exporter;
- examples;
- tests.

The first version proved the module shape without forcing a concrete HTTP backend too early.

## v0.2.0 — Real local UI connection

Status: completed.

Main goal: connect the server facade to a real local UI workflow.

Completed:

- serve `assets/index.html`;
- serve CSS and JavaScript assets;
- expose `/api/document`;
- expose `/api/cells/<id>/run`;
- expose `/api/run-all`;
- load a `.vixnote` file from disk;
- update the browser UI from API responses;
- show cell outputs visually;
- show execution status;
- show basic errors;
- provide a local HTTP server.

## v0.3.0 — Better document editing

Status: completed.

Main goal: make Vix Note usable as a lightweight editor.

Completed:

- edit markdown cells;
- edit C++ cells;
- edit Reply cells;
- edit HTML cells;
- add new cells;
- remove cells;
- reorder cells;
- save back to `.vixnote`;
- detect unsaved changes in the UI;
- preserve cell ids through metadata comments;
- improve keyboard shortcuts;
- improve mobile layout.

Supported UI actions:

```txt
Add Markdown
Add C++
Add Reply
Save
Run cell
Run all
Move up
Move down
Delete
```

Supported shortcuts:

```txt
Ctrl + Enter     run current cell
Ctrl + S         save note
```

## v0.4.0 — Better C++ learning experience

Status: completed.

Main goal: make outputs and errors easier to understand.

Completed:

- display stdout separately;
- display stderr separately;
- display compiler errors clearly;
- display runtime errors clearly;
- show execution time in debug mode;
- show exit code in debug mode;
- show temporary source path when debug mode is enabled;
- add beginner-friendly error hints when possible;
- keep raw logs accessible;
- support empty-success output for cells with no output.

This version made C++ errors less intimidating for learners.

## v0.5.0 — Reply cell integration

Status: completed.

Main goal: make Reply cells useful inside notes.

Completed:

- execute Reply cells through the embedded Vix Reply runtime;
- support small expression cells;
- support `println`;
- support calculator-like cells;
- expose arguments through runner options;
- store Reply outputs in the document session;
- reset Reply runtime state when needed;
- keep Reply cells separate from the terminal REPL.

Important:

Reply cells should not duplicate the full terminal REPL. They are small interactive cells inside a visual note.

## v0.6.0 — HTML export improvements

Status: completed.

Main goal: make exported lessons clean enough to share.

Completed:

- standalone HTML export;
- fragment HTML export;
- better markdown rendering for headings and paragraphs;
- code block syntax classes;
- output rendering;
- error rendering;
- document metadata;
- table of contents;
- export with outputs;
- export without outputs;
- custom CSS support;
- printable layout;
- CLI export through `vix note export`.

Supported command:

```bash
vix note export examples/hello.vixnote --out hello.html
```

Optional output control:

```bash
vix note export examples/hello.vixnote --out hello.html --no-outputs
vix note export examples/hello.vixnote --out hello.html --with-outputs
```

## v0.7.0 — Project-aware notes

Status: completed.

Main goal: let notes work inside real Vix projects.

Completed:

- detect project root;
- detect `vix.app`;
- detect `.vix/manifest.vix`;
- detect `.vix/deps`;
- detect project include directory;
- support project working directory;
- support include paths;
- support project context in C++ cell execution;
- show project information through the document API;
- show project context in the `vix note` command output;
- keep project context separate from `NoteDocument`.

This version makes Vix Note useful for tutorials inside real repositories.

## v0.8.0 — Asset and UI packaging

Status: completed.

Main goal: make the UI easier to package and install.

Completed:

- install built-in assets;
- load assets from installed data directory;
- fallback to embedded assets;
- allow custom UI asset directory;
- support `VIX_NOTE_ASSET_DIR`;
- add asset search path resolver;
- add best-available asset loading;
- improve CSS structure;
- improve browser UI layout;
- prepare WebView shell usage;
- prepare desktop app shell integration;
- add tests for custom asset loading;
- add tests for embedded fallback;
- add tests for route-level custom assets.

This version prepares future integration with `vix::ui`.

## v0.9.0 — Notebook API polish

Status: completed.

Main goal: stabilize the public API before `1.0.0`.

Completed:

- review naming;
- review error messages;
- review ownership model;
- review serialization;
- review kernel/session boundaries;
- review server/route boundaries;
- add more examples;
- improve Doxygen comments;
- reduce unnecessary complexity;
- improve tests across the module;
- verify stable public include through `<vix/note/note.hpp>`;
- verify stable CLI usage through `vix note`.

This version focused on clarity before the first stable release.

## v1.0.0 — Stable first release

Status: completed.

Main goal: provide a stable first Vix Note release.

Completed capabilities:

- open `.vixnote` files;
- parse markdown-compatible note files;
- preserve stable cell metadata;
- edit notes visually;
- run C++ cells;
- run Reply cells;
- render HTML cells;
- show outputs;
- show errors;
- save documents;
- export HTML;
- work inside Vix projects;
- load embedded UI assets;
- load installed UI assets;
- load custom UI assets;
- serve a local browser UI;
- provide a stable public C++ API;
- integrate with the Vix CLI through `vix note`.

Stable commands:

```bash
vix note examples/hello.vixnote
vix note examples/cpp_basics.vixnote
vix note examples/cpp_basics.vixnote --port 5180
vix note examples/cpp_basics.vixnote --host 127.0.0.1 --port 5179
vix note export examples/cpp_basics.vixnote --out cpp_basics.html
vix note export examples/cpp_basics.vixnote --out cpp_basics.html --no-outputs
```

Expected local UI output:

```txt
Vix Note started.
Open this URL in your browser:
  http://127.0.0.1:5179/

Project context:
  examples

Press Ctrl+C to stop the note server.
```

## Stable module scope

Vix Note v1.0.0 is intentionally focused.

The stable scope is:

```txt
visual executable lessons
C++ cells
Reply cells
HTML cells
markdown-compatible notes
local UI
vix note command
save/load
project context
HTML export
packaged assets
```

The stable scope is not:

```txt
cloud notebooks
multi-user collaboration
heavy notebook runtime
remote execution
full Jupyter replacement
terminal REPL replacement
```

## Long-term ideas

These ideas are not priorities for the first stable release.

Possible future directions:

- WebView desktop shell;
- visual diagrams;
- inline charts;
- cell templates;
- learning tracks;
- package examples as tutorials;
- shareable static lessons;
- project documentation notebooks;
- C++ diagnostics assistant;
- replay integration for executed cells;
- timeline of previous runs;
- side-by-side explanation and code;
- integration with Vix UI;
- richer export themes;
- lesson publishing workflow.

## Design rules

Vix Note should follow these rules:

```txt
Prefer simple features.
Avoid duplicating vix repl.
Keep the UI useful, not decorative.
Keep the runtime aligned with vix run.
Keep files readable as markdown.
Keep exports clean.
Keep the module easy to test.
```

## Core philosophy

Vix Note exists because learning C++ should not feel hidden inside terminal output only.

A good note should show:

```txt
what the code means
what the code does
what the output says
what the error means
what to try next
```

That is the difference between a terminal REPL and a visual notebook.

## Release summary

Vix Note v1.0.0 completes the first stable notebook foundation for Vix.cpp.

It gives Vix a visual learning layer without replacing the terminal tools that already exist.

```txt
vix repl   -> terminal-first interaction
vix run    -> execution
vix note   -> visual learning workspace
```
