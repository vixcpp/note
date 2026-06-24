# Changelog

All notable changes to Vix Note will be documented in this file.

The format follows a simple versioned changelog style.

## v0.4.0

### Added

- Added better C++ execution output modeling.

- Added separated output categories for:
  - `stdout`
  - `stderr`
  - `error`
  - `compiler_error`
  - `runtime_error`
  - `debug`
  - `hint`
  - `raw_log`

- Added execution timing support for C++ cells.

- Added support for exposing the C++ cell exit code.

- Added support for exposing the generated temporary source path when debug mode is enabled.

- Added raw execution logs so advanced users can inspect the full runtime output.

- Added beginner-friendly diagnostic hints for common C++ errors.

- Added clearer runtime failure reporting for failed C++ cells.

- Added better run summaries for multi-cell execution:
  - visited cells
  - executed cells
  - skipped cells
  - failed cells
  - stopped state

### Changed

- Improved `CppCellRunner` so C++ execution results are easier to understand in the UI.
- Improved `NoteKernel` run results to expose clearer execution summaries.
- Improved route JSON responses for cell execution and run-all operations.
- Improved browser output rendering for different output kinds.
- Improved local UI styling for success, warning, error, debug, hint, and raw log outputs.
- Updated Vix Note version metadata to `0.4.0`.

### Fixed

- Fixed C++ cell output rendering so successful output and raw logs can both be preserved.
- Fixed failed C++ cell reporting so runtime failures include structured error output.
- Fixed local UI JavaScript embedded asset startup by preserving the full IIFE wrapper.
- Fixed route tests for running cells by id.
- Fixed server tests for editing and deleting cells through route wrappers.

## v0.3.0

### Added

- Added lightweight document editing support through the local API.

- Added API route support for adding cells:
  - `POST /api/cells`

- Added API route support for updating cells:
  - `PUT /api/cells/<id>`

- Added API route support for deleting cells:
  - `DELETE /api/cells/<id>`

- Added API route support for moving cells:
  - `POST /api/cells/<id>/move`

- Added API route support for saving the document:
  - `POST /api/document/save`

- Added support for running cells by stable id:
  - `POST /api/cells/<id>/run`

- Added route wrappers for:
  - `put()`
  - `delete_request()`

- Added server wrappers for:
  - `put()`
  - `delete_request()`

- Added parser support for Vix Note cell metadata comments:
  - `<!-- vixnote:cell id="cell-1" kind="markdown" -->`

- Added storage support for preserving stable cell ids across save/load cycles.

- Added browser UI support for:
  - editing markdown cells
  - editing C++ cells
  - adding markdown cells
  - adding C++ cells
  - deleting cells
  - moving cells up and down
  - saving notes
  - marking unsaved changes

- Added keyboard shortcut support in the browser UI:
  - `Ctrl + Enter` to run the current cell
  - `Ctrl + S` to save the note

### Changed

- Updated `.vixnote` serialization to preserve cell identity through metadata comments.

- Improved route mutation responses with:
  - `ok`
  - `message`
  - `cellId`
  - updated `cell`
  - updated `document`

- Improved the embedded local UI assets.

- Improved the external browser UI assets:
  - `assets/index.html`
  - `assets/css/note.css`
  - `assets/js/note.js`

- Improved mobile layout for the local UI.

- Improved local UI state labels for saved, unsaved, loading, running, and error states.

- Improved route behavior for editing-disabled and save-disabled modes.

### Fixed

- Fixed route response consistency for cell add, update, delete, move, and save operations.
- Fixed save route behavior when a document has no path.
- Fixed API route tests for editing, saving, moving, and deleting cells.
- Fixed server route tests for `PUT` and `DELETE`.
- Fixed parser metadata handling without exposing extra public metadata types.
- Fixed raw string embedded JavaScript output so the browser receives valid JavaScript.

## v0.2.0

### Added

- Added real local UI connection support.

- Added concrete local server runtime support behind `NoteServer`.

- Added support for serving the embedded `index.html`.

- Added support for serving CSS and JavaScript assets.

- Added API route support for:
  - `GET /api/document`
  - `POST /api/cells/<index>/run`
  - `POST /api/run-all`

- Added support for loading `.vixnote` files from disk before serving them.

- Added browser UI document loading from `/api/document`.

- Added browser UI execution through note API routes.

- Added visual cell output rendering in the local UI.

- Added basic execution status display.

- Added basic error display.

### Changed

- Improved `NoteServer` from a pure facade toward a real local UI server.
- Improved `NoteRoutes` JSON serialization for documents, cells, outputs, and results.
- Improved embedded UI assets to connect to the runtime API.
- Improved tests for note assets, routes, and server behavior.

### Expected command

```bash
vix note examples/hello.vixnote
```

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
- Markdown rendering in the HTML exporter supports only a small dependency-free subset.
- C++ cell execution depends on the `vix` command being available.

### Planned Next

- Add real Reply cell execution through Vix Reply.
- Improve HTML export rendering.
- Improve project-aware notes.
- Improve asset packaging and installation.
- Prepare WebView shell usage.
- Polish the public notebook API before `1.0.0`.
