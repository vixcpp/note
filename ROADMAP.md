# Vix Note Roadmap

Vix Note is a UI-first notebook foundation for learning C++ and Vix.cpp faster.

The goal is not to create another terminal experience.
`vix repl` already covers the terminal-first workflow.

Vix Note exists for a different reason: to keep explanations, code cells, outputs, errors, and exported lessons together in one visual workspace.

## Direction

Vix Note should stay small, native, and focused.

The first direction is:

```txt
learn C++ faster
write explanations
run small C++ cells
see outputs near the code
export the lesson
open the note in a local UI
```

It should not become a heavy notebook platform too early.

Before adding a feature, we should ask:

```txt
Does this make learning C++ clearer?
Does this avoid repeating what vix repl already does?
Does this keep the module simple?
Does this fit the Vix.cpp tooling direction?
```

## v0.1.0 — Foundation

Status: in progress.

Main goal: create the core module architecture.

Included:

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

The first version proves the module shape without forcing a concrete HTTP backend too early.

## v0.2.0 — Real local UI connection

Main goal: connect the server facade to a real local server.

Planned:

- serve `assets/index.html`;
- serve CSS and JavaScript assets;
- expose `/api/document`;
- expose `/api/cells/<index>/run`;
- expose `/api/run-all`;
- load a `.vixnote` file from disk;
- update the browser UI from API responses;
- show cell outputs visually;
- show execution status;
- show basic errors.

Expected command direction:

```bash
vix note examples/hello.vixnote
```

The command should open a local browser page or print the local URL.

## v0.3.0 — Better document editing

Main goal: make Vix Note usable as a lightweight editor.

Planned:

- edit markdown cells;
- edit C++ cells;
- add new cells;
- remove cells;
- reorder cells;
- save back to `.vixnote`;
- detect unsaved changes;
- improve keyboard shortcuts;
- improve mobile layout;
- preserve cell ids.

Possible shortcuts:

```txt
Ctrl + Enter     run current cell
Shift + Enter    run current cell and move next
Ctrl + S         save note
A                add cell above
B                add cell below
D D              delete cell
```

## v0.4.0 — Better C++ learning experience

Main goal: make outputs and errors easier to understand.

Planned:

- display stdout separately;
- display stderr separately;
- display compiler errors clearly;
- display runtime errors clearly;
- show execution time;
- show exit code;
- show temporary source path when debug mode is enabled;
- add beginner-friendly error hints when possible;
- keep raw logs accessible.

This version should make C++ errors less intimidating for learners.

## v0.5.0 — Reply cell integration

Main goal: make Reply cells useful inside notes.

Planned:

- execute `reply` cells through the Vix Reply runtime;
- keep simple expressions fast;
- support `println`;
- support calculator-like cells;
- support basic Vix runtime helpers;
- store Reply outputs in the document session;
- make Reply cells useful for quick explanations.

Important:

Reply cells should not duplicate the full terminal REPL.
They should be small interactive cells inside a visual note.

## v0.6.0 — HTML export improvements

Main goal: make exported lessons clean enough to share.

Planned:

- better markdown rendering;
- code block syntax classes;
- output rendering;
- error rendering;
- table of contents;
- document metadata;
- export without outputs;
- export with outputs;
- custom export theme;
- printable layout.

Possible command:

```bash
vix note export examples/hello.vixnote --out hello.html
```

## v0.7.0 — Project-aware notes

Main goal: let notes work inside real Vix projects.

Planned:

- detect project root;
- run cells with project context;
- support include paths;
- support `.vix` manifest context;
- support `.vix/deps`;
- support project environment variables;
- support working directory selection;
- show project name in the UI.

This version should make Vix Note useful for tutorials inside real repositories.

## v0.8.0 — Asset and UI packaging

Main goal: make the UI easier to package and install.

Planned:

- install built-in assets;
- load assets from installed data directory;
- fallback to embedded assets;
- allow custom UI asset directory;
- improve CSS structure;
- prepare WebView shell usage;
- prepare desktop app shell integration.

This can later connect with `vix::ui`.

## v0.9.0 — Notebook API polish

Main goal: stabilize the public API.

Planned:

- review naming;
- review error messages;
- review ownership model;
- review serialization;
- review kernel/session boundaries;
- review server/route boundaries;
- add more examples;
- add API documentation;
- improve Doxygen comments;
- reduce anything that feels unnecessary.

This version should focus on clarity before `1.0.0`.

## v1.0.0 — Stable first release

Main goal: provide a stable first Vix Note release.

Expected capabilities:

- open `.vixnote` files;
- edit notes visually;
- run C++ cells;
- run Reply cells;
- show outputs;
- save documents;
- export HTML;
- work inside Vix projects;
- provide a stable public C++ API;
- integrate with the Vix CLI.

Possible commands:

```bash
vix note examples/hello.vixnote
vix note new lesson.vixnote
vix note export lesson.vixnote --out lesson.html
```

## Long-term ideas

These ideas are not priorities for the first versions.

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
- integration with Vix UI.

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
