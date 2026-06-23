# Changelog

All notable changes to Vix Note will be documented in this file.

The format follows a simple versioned changelog style.

## v0.1.0

### Added

- Added the initial Vix Note module foundation.
- Added the public umbrella header:
  - `include/vix/note/note.hpp`

- Added module version metadata:
  - `include/vix/note/Version.hpp`

- Added core note error support:
  - `NoteError`
  - `NoteErrorCode`

- Added core result and output models:
  - `NoteResult`
  - `NoteResultStatus`
  - `NoteOutput`
  - `NoteOutputKind`

- Added the note cell model:
  - markdown cells
  - Reply cells
  - C++ cells
  - HTML cells
  - cell ids
  - cell titles
  - execution counts
  - cell outputs

- Added the note document model:
  - document id
  - document title
  - document path
  - ordered cell storage
  - execution counter
  - cell insertion
  - cell removal
  - cell lookup by index or id

- Added the markdown-compatible `.vixnote` parser.
- Added parser options for:
  - automatic cell id assignment
  - title inference from the first markdown heading

- Added `.vixnote` storage support:
  - load from disk
  - save to disk
  - atomic write option
  - parent directory creation
  - serialization back to markdown-compatible text

- Added C++ cell execution through `vix run`.
- Added runtime session support:
  - mutable document state
  - execution records
  - output application
  - execution count tracking

- Added the high-level note kernel:
  - run one cell
  - run all cells
  - run executable cells only
  - stop on first failure option
  - non-executable skipped-cell option

- Added embedded web asset support:
  - default index HTML
  - default CSS
  - default JavaScript
  - asset lookup
  - asset content type detection

- Added local UI route resolver:
  - static asset routes
  - `/api/document`
  - `/api/cells/<index>/run`
  - `/api/run-all`

- Added local server facade:
  - server lifecycle state
  - start
  - stop
  - restart
  - local URL generation
  - route forwarding

- Added static HTML exporter:
  - standalone HTML export
  - fragment export
  - markdown rendering
  - code cell rendering
  - HTML cell rendering
  - output rendering
  - custom CSS option

- Added browser UI assets:
  - `assets/index.html`
  - `assets/css/note.css`
  - `assets/js/note.js`

- Added example note documents:
  - `examples/hello.vixnote`
  - `examples/learning_cpp.vixnote`
  - `examples/vix_ui_note.vixnote`

- Added CMake build configuration.
- Added CMake test configuration.
- Added CMake example file copying configuration.
- Added README documentation.
- Added roadmap documentation.

### Design

- Established Vix Note as a UI-first notebook foundation.
- Clarified that Vix Note is not a replacement for `vix repl`.
- Kept C++ cell execution aligned with normal Vix CLI behavior by delegating to `vix run`.
- Kept the first server layer backend-independent through `NoteRoutes` and `NoteServer`.
- Kept `.vixnote` files readable as markdown-compatible documents.

### Current Limitations

- Reply cells are recognized but not fully executed yet.
- `NoteServer` is currently a lifecycle and routing facade, not a concrete socket-binding HTTP server.
- The browser UI is static and prepared for later API integration.
- Markdown rendering in the HTML exporter supports only a small dependency-free subset.
- C++ cell execution currently depends on the `vix` command being available.

### Planned Next

- Connect `NoteServer` to the real Vix HTTP server layer.
- Add a CLI command such as `vix note <file.vixnote>`.
- Load real `.vixnote` documents into the browser UI.
- Connect browser actions to note API routes.
- Add real Reply cell execution through Vix Reply.
- Improve compiler and runtime error display.
- Improve HTML export rendering.
