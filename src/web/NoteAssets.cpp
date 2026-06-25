/**
 *
 *  @file NoteAssets.cpp
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

#include <vix/note/web/NoteAssets.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cstdlib>

namespace vix::note
{
  namespace
  {
    bool ends_with(std::string_view value, std::string_view suffix)
    {
      return value.size() >= suffix.size() &&
             value.substr(value.size() - suffix.size()) == suffix;
    }

    bool is_existing_directory(const std::filesystem::path &path)
    {
      if (path.empty())
      {
        return false;
      }

      std::error_code ec;

      return std::filesystem::exists(path, ec) &&
             !ec &&
             std::filesystem::is_directory(path, ec) &&
             !ec;
    }

    void add_unique_path(
        std::vector<std::filesystem::path> &paths,
        const std::filesystem::path &path)
    {
      if (path.empty())
      {
        return;
      }

      const std::filesystem::path normalized =
          path.lexically_normal();

      const auto it =
          std::find(
              paths.begin(),
              paths.end(),
              normalized);

      if (it == paths.end())
      {
        paths.push_back(normalized);
      }
    }
  }

  bool NoteAsset::empty() const noexcept
  {
    return content.empty();
  }

  NoteAssets::NoteAssets()
      : assets_(defaults())
  {
  }

  NoteAssets::NoteAssets(std::vector<NoteAsset> assets)
      : assets_(std::move(assets))
  {
  }

  std::filesystem::path note_installed_asset_directory()
  {
#ifdef VIX_NOTE_INSTALLED_ASSET_DIR
    return std::filesystem::path(VIX_NOTE_INSTALLED_ASSET_DIR);
#else
    return {};
#endif
  }

  std::vector<std::filesystem::path> note_asset_search_paths(
      const NoteAssetResolveOptions &options)
  {
    std::vector<std::filesystem::path> paths;

    add_unique_path(paths, options.customDirectory);

    if (options.useEnvironmentDirectory)
    {
      const char *env = std::getenv("VIX_NOTE_ASSET_DIR");

      if (env != nullptr && env[0] != '\0')
      {
        add_unique_path(paths, std::filesystem::path(env));
      }
    }

    if (options.useInstalledDirectory)
    {
      add_unique_path(paths, note_installed_asset_directory());
    }

    return paths;
  }

  bool load_best_available_note_assets(
      NoteAssets &assets,
      const NoteAssetResolveOptions &options,
      std::string &error)
  {
    error.clear();

    NoteAssetDirectoryOptions directoryOptions;
    directoryOptions.clearBeforeLoad = false;
    directoryOptions.keepEmbeddedFallback = options.keepEmbeddedFallback;

    const std::vector<std::filesystem::path> paths =
        note_asset_search_paths(options);

    std::string lastError;

    for (const std::filesystem::path &path : paths)
    {
      if (!is_existing_directory(path))
      {
        continue;
      }

      std::string loadError;

      if (assets.load_from_directory(path, directoryOptions, loadError))
      {
        return true;
      }

      if (!loadError.empty())
      {
        lastError = loadError;
      }
    }

    if (!lastError.empty())
    {
      error = lastError;
    }

    return false;
  }

  const std::vector<NoteAsset> &NoteAssets::all() const noexcept
  {
    return assets_;
  }

  std::size_t NoteAssets::size() const noexcept
  {
    return assets_.size();
  }

  bool NoteAssets::empty() const noexcept
  {
    return assets_.empty();
  }

  std::optional<NoteAsset> NoteAssets::find(std::string_view path) const
  {
    const std::string normalized =
        normalize_note_asset_path(path);

    const auto it =
        std::find_if(
            assets_.begin(),
            assets_.end(),
            [&](const NoteAsset &asset)
            {
              return normalize_note_asset_path(asset.path) == normalized;
            });

    if (it == assets_.end())
    {
      return std::nullopt;
    }

    return *it;
  }

  bool NoteAssets::contains(std::string_view path) const
  {
    return find(path).has_value();
  }

  void NoteAssets::add_or_replace(NoteAsset asset)
  {
    asset.path = normalize_note_asset_path(asset.path);

    if (asset.contentType.empty())
    {
      asset.contentType = note_asset_content_type(asset.path);
    }

    const auto it =
        std::find_if(
            assets_.begin(),
            assets_.end(),
            [&](const NoteAsset &current)
            {
              return normalize_note_asset_path(current.path) == asset.path;
            });

    if (it == assets_.end())
    {
      assets_.push_back(std::move(asset));
      return;
    }

    *it = std::move(asset);
  }

  bool NoteAssets::load_from_directory(
      const std::filesystem::path &directory,
      NoteAssetDirectoryOptions options,
      std::string &error)
  {
    error.clear();

    std::vector<NoteAsset> loaded =
        from_directory(directory, error);

    if (!error.empty())
    {
      return false;
    }

    if (!options.keepEmbeddedFallback && loaded.size() < 4)
    {
      error =
          "asset directory is missing required Vix Note UI assets: " +
          directory.string();

      return false;
    }

    if (options.clearBeforeLoad || !options.keepEmbeddedFallback)
    {
      clear();
    }

    for (NoteAsset &asset : loaded)
    {
      add_or_replace(std::move(asset));
    }

    return true;
  }

  bool NoteAssets::load_from_directory(
      const std::filesystem::path &directory,
      std::string &error)
  {
    return load_from_directory(
        directory,
        NoteAssetDirectoryOptions{},
        error);
  }

  bool NoteAssets::remove(std::string_view path)
  {
    const std::string normalized =
        normalize_note_asset_path(path);

    const auto oldSize = assets_.size();

    assets_.erase(
        std::remove_if(
            assets_.begin(),
            assets_.end(),
            [&](const NoteAsset &asset)
            {
              return normalize_note_asset_path(asset.path) == normalized;
            }),
        assets_.end());

    return assets_.size() != oldSize;
  }

  void NoteAssets::clear()
  {
    assets_.clear();
  }

  std::vector<NoteAsset> NoteAssets::defaults()
  {
    std::vector<NoteAsset> assets;

    assets.push_back(
        NoteAsset{
            "/",
            "text/html; charset=utf-8",
            default_index_html()});

    assets.push_back(
        NoteAsset{
            "/index.html",
            "text/html; charset=utf-8",
            default_index_html()});

    assets.push_back(
        NoteAsset{
            "/assets/note.css",
            "text/css; charset=utf-8",
            default_css()});

    assets.push_back(
        NoteAsset{
            "/assets/note.js",
            "application/javascript; charset=utf-8",
            default_js()});

    return assets;
  }

  std::vector<NoteAsset> NoteAssets::from_directory(
      const std::filesystem::path &directory,
      std::string &error)
  {
    error.clear();

    std::vector<NoteAsset> assets;

    if (directory.empty())
    {
      error = "empty note asset directory";
      return assets;
    }

    std::error_code ec;

    if (!std::filesystem::exists(directory, ec) ||
        !std::filesystem::is_directory(directory, ec))
    {
      error = "note asset directory does not exist: " + directory.string();
      return assets;
    }

    const std::vector<std::filesystem::path> files{
        std::filesystem::path("index.html"),
        std::filesystem::path("css") / "note.css",
        std::filesystem::path("js") / "note.js"};

    for (const std::filesystem::path &relative : files)
    {
      const std::filesystem::path file = directory / relative;

      if (!std::filesystem::exists(file, ec))
      {
        continue;
      }

      std::string content;
      std::string err;

      if (!read_note_asset_file(file, content, err))
      {
        error = err;
        return {};
      }

      const std::string publicPath =
          note_asset_public_path(relative);

      if (relative == std::filesystem::path("index.html"))
      {
        assets.push_back(
            NoteAsset{
                "/",
                "text/html; charset=utf-8",
                content});

        assets.push_back(
            NoteAsset{
                "/index.html",
                "text/html; charset=utf-8",
                content});

        continue;
      }

      assets.push_back(
          NoteAsset{
              publicPath,
              note_asset_content_type(publicPath),
              std::move(content)});
    }

    return assets;
  }

  std::string NoteAssets::default_index_html()
  {
    return R"VIXNOTE(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Vix Note</title>
  <link rel="stylesheet" href="/assets/note.css">
</head>
<body>
  <main class="note-shell">
    <header class="note-header">
      <div class="note-header__copy">
        <p class="note-eyebrow">Vix Note</p>
        <h1>Visual executable notes for C++</h1>
        <p class="note-subtitle">
          Write explanations, edit cells, run small C++ examples, and keep outputs near the lesson.
        </p>
      </div>

      <div class="note-actions" aria-label="Note actions">
        <button type="button" data-action="add-markdown">Add Markdown</button>
        <button type="button" data-action="add-cpp">Add C++</button>
        <button type="button" data-action="run-all">Run all</button>
        <button type="button" data-action="save" class="note-button--primary">Save</button>
      </div>
    </header>

    <section class="note-status" aria-live="polite">
      <span id="note-title">Loading note...</span>
      <span id="note-state">loading</span>
    </section>

    <section class="note-workspace" id="note-workspace" aria-label="Vix Note workspace">
      <article class="note-empty">
        <h2>Loading Vix Note</h2>
        <p>The local UI is connecting to the note document.</p>
      </article>
    </section>
  </main>

  <script src="/assets/note.js"></script>
</body>
</html>
)VIXNOTE";
  }

  std::string NoteAssets::default_css()
  {
    return R"VIXNOTE(:root {
  color-scheme: light;
  --note-bg: #f6f7fb;
  --note-panel: #ffffff;
  --note-text: #111827;
  --note-muted: #6b7280;
  --note-border: #e5e7eb;
  --note-border-strong: #d1d5db;
  --note-accent: #d57a2a;
  --note-accent-strong: #b85f16;
  --note-code-bg: #111827;
  --note-code-text: #f9fafb;
  --note-soft: #f9fafb;
  --note-success-bg: #ecfdf5;
  --note-success-text: #047857;
  --note-error-bg: #fef2f2;
  --note-error-text: #b91c1c;
  --note-warning-bg: #fffbeb;
  --note-warning-text: #92400e;
  --note-info-bg: #eff6ff;
  --note-info-text: #1d4ed8;
}

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  font-family:
    Inter,
    ui-sans-serif,
    system-ui,
    -apple-system,
    BlinkMacSystemFont,
    "Segoe UI",
    sans-serif;
  background:
    radial-gradient(circle at top left, rgba(213, 122, 42, 0.12), transparent 32rem),
    var(--note-bg);
  color: var(--note-text);
}

button,
select,
textarea {
  font: inherit;
}

button,
select {
  border: 1px solid var(--note-border);
  border-radius: 10px;
  background: var(--note-panel);
  color: var(--note-text);
}

button {
  padding: 0.62rem 0.82rem;
  cursor: pointer;
}

button:hover {
  border-color: var(--note-accent);
}

button:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

textarea {
  width: 100%;
  min-height: 150px;
  resize: vertical;
  border: 0;
  outline: none;
  padding: 18px 20px;
  background: transparent;
  color: inherit;
  line-height: 1.6;
}

select {
  padding: 0.45rem 0.6rem;
  font-weight: 700;
}

.note-shell {
  width: min(1180px, calc(100% - 32px));
  margin: 0 auto;
  padding: 32px 0;
}

.note-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 24px;
  margin-bottom: 20px;
}

.note-header__copy {
  max-width: 720px;
}

.note-eyebrow {
  margin: 0 0 8px;
  color: var(--note-accent);
  font-weight: 800;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.note-header h1 {
  margin: 0;
  font-size: clamp(2rem, 5vw, 4rem);
  line-height: 1;
  letter-spacing: -0.055em;
}

.note-subtitle {
  max-width: 620px;
  margin: 14px 0 0;
  color: var(--note-muted);
  font-size: 1rem;
  line-height: 1.7;
}

.note-actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  flex-wrap: wrap;
  min-width: min(100%, 420px);
}

.note-button--primary {
  border-color: var(--note-accent);
  background: var(--note-accent);
  color: #ffffff;
}

.note-button--primary:hover {
  border-color: var(--note-accent-strong);
  background: var(--note-accent-strong);
}

.note-status {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  margin-bottom: 18px;
  padding: 12px 14px;
  border: 1px solid var(--note-border);
  border-radius: 16px;
  background: rgba(255, 255, 255, 0.72);
  color: var(--note-muted);
  backdrop-filter: blur(8px);
}

#note-title {
  color: var(--note-text);
  font-weight: 800;
}

#note-state {
  padding: 0.32rem 0.55rem;
  border-radius: 999px;
  background: var(--note-soft);
  font-size: 0.82rem;
  font-weight: 800;
  text-transform: uppercase;
}

.note-workspace {
  display: grid;
  gap: 18px;
}

.note-empty {
  border: 1px dashed var(--note-border-strong);
  border-radius: 18px;
  padding: 28px;
  background: rgba(255, 255, 255, 0.7);
  text-align: center;
}

.note-empty h2 {
  margin: 0 0 8px;
}

.note-empty p {
  margin: 0;
  color: var(--note-muted);
}

.note-cell {
  overflow: hidden;
  border: 1px solid var(--note-border);
  border-radius: 18px;
  background: var(--note-panel);
  box-shadow: 0 18px 45px rgba(15, 23, 42, 0.06);
}

.note-cell[data-dirty="true"] {
  border-color: rgba(213, 122, 42, 0.72);
}

.note-cell__bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 14px;
  padding: 12px 14px;
  border-bottom: 1px solid var(--note-border);
  background: var(--note-soft);
}

.note-cell__meta {
  display: flex;
  align-items: center;
  gap: 10px;
  min-width: 0;
}

.note-cell__id {
  overflow: hidden;
  max-width: 210px;
  color: var(--note-muted);
  font-size: 0.85rem;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.note-cell__actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.note-cell__actions button {
  padding: 0.48rem 0.66rem;
  font-size: 0.88rem;
}

.note-cell__editor {
  background: #ffffff;
}

.note-cell--cpp .note-cell__editor,
.note-cell--reply .note-cell__editor {
  background: var(--note-code-bg);
  color: var(--note-code-text);
}

.note-cell--cpp textarea,
.note-cell--reply textarea {
  font-family:
    "SFMono-Regular",
    Consolas,
    "Liberation Mono",
    monospace;
  font-size: 0.94rem;
}

.note-preview {
  padding: 0 20px 20px;
  color: var(--note-muted);
}

.note-preview h1,
.note-preview h2,
.note-preview h3 {
  color: var(--note-text);
}

.note-preview code {
  border-radius: 6px;
  padding: 0.15rem 0.35rem;
  background: var(--note-soft);
  color: var(--note-accent-strong);
}

.note-output-list {
  display: grid;
  gap: 10px;
  padding: 14px;
  border-top: 1px solid var(--note-border);
  background: #fcfcfd;
}

.note-output {
  overflow: auto;
  border: 1px solid var(--note-border);
  border-radius: 12px;
  padding: 12px 14px;
  background: #ffffff;
}

.note-output__kind {
  display: inline-flex;
  margin-bottom: 8px;
  color: var(--note-muted);
  font-size: 0.75rem;
  font-weight: 900;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.note-output pre {
  margin: 0;
  white-space: pre-wrap;
  word-break: break-word;
}

.note-output--stdout {
  background: var(--note-success-bg);
  color: var(--note-success-text);
}

.note-output--stderr {
  background: var(--note-warning-bg);
  color: var(--note-warning-text);
}

.note-output--error,
.note-output--compiler_error,
.note-output--runtime_error {
  background: var(--note-error-bg);
  color: var(--note-error-text);
}

.note-output--hint {
  background: var(--note-info-bg);
  color: var(--note-info-text);
}

.note-output--debug,
.note-output--raw_log {
  background: #f3f4f6;
  color: #374151;
}

@media (max-width: 780px) {
  .note-shell {
    width: min(100% - 20px, 1180px);
    padding: 20px 0;
  }

  .note-header {
    flex-direction: column;
  }

  .note-actions {
    justify-content: flex-start;
    width: 100%;
  }

  .note-actions button {
    flex: 1 1 140px;
  }

  .note-status {
    align-items: flex-start;
    flex-direction: column;
  }

  .note-cell__bar {
    align-items: flex-start;
    flex-direction: column;
  }

  .note-cell__actions {
    width: 100%;
  }

  .note-cell__actions button {
    flex: 1 1 auto;
  }

  textarea {
    min-height: 130px;
    padding: 16px;
  }
}
)VIXNOTE";
  }

  std::string NoteAssets::default_js()
  {
    return R"VIXNOTE((() => {
  const workspace = document.querySelector("#note-workspace");
  const titleEl = document.querySelector("#note-title");
  const stateEl = document.querySelector("#note-state");

  const state = {
    document: null,
    dirty: false,
    busy: false,
    activeCellId: null
  };

  function setStatus(text) {
    if (stateEl) {
      stateEl.textContent = text;
    }
  }

  function setDirty(value) {
    state.dirty = value;
    setStatus(value ? "unsaved" : "saved");
  }

  function escapeHtml(value) {
    return String(value ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll("\"", "&quot;")
      .replaceAll("'", "&#039;");
  }

  function renderMarkdown(source) {
    const lines = String(source ?? "").split(/\r?\n/);
    const html = lines.map((line) => {
      if (line.startsWith("# ")) {
        return `<h1>${escapeHtml(line.slice(2))}</h1>`;
      }

      if (line.startsWith("## ")) {
        return `<h2>${escapeHtml(line.slice(3))}</h2>`;
      }

      if (line.startsWith("### ")) {
        return `<h3>${escapeHtml(line.slice(4))}</h3>`;
      }

      if (line.trim() === "") {
        return "";
      }

      return `<p>${escapeHtml(line)}</p>`;
    });

    return html.join("");
  }

  async function requestJson(path, options = {}) {
    const response = await fetch(path, {
      headers: {
        "Content-Type": "application/json"
      },
      ...options
    });

    const text = await response.text();
    let body = null;

    try {
      body = text ? JSON.parse(text) : null;
    } catch (_) {
      body = { ok: false, error: text };
    }

    if (!response.ok) {
      const message = body?.error || body?.message || `request failed: ${response.status}`;
      throw new Error(message);
    }

    return body;
  }

  function cellClass(kind) {
    return `note-cell note-cell--${kind || "unknown"}`;
  }

  function outputClass(kind) {
    return `note-output note-output--${kind || "text"}`;
  }

  function renderOutputs(outputs) {
    if (!outputs || outputs.length === 0) {
      return "";
    }

    return `
      <div class="note-output-list">
        ${outputs.map((output) => `
          <section class="${outputClass(output.kind)}">
            <span class="note-output__kind">${escapeHtml(output.kind)}</span>
            <pre>${escapeHtml(output.content)}</pre>
          </section>
        `).join("")}
      </div>
    `;
  }

  function renderCell(cell) {
    const kind = cell.kind || "markdown";
    const id = cell.id || `cell-${cell.index + 1}`;
    const execution = cell.executionCount ? ` · run ${cell.executionCount}` : "";

    return `
      <article class="${cellClass(kind)}" data-cell-id="${escapeHtml(id)}" data-dirty="false">
        <div class="note-cell__bar">
          <div class="note-cell__meta">
            <select data-action="change-kind" aria-label="Cell kind">
              <option value="markdown"${kind === "markdown" ? " selected" : ""}>Markdown</option>
              <option value="cpp"${kind === "cpp" ? " selected" : ""}>C++</option>
              <option value="reply"${kind === "reply" ? " selected" : ""}>Reply</option>
              <option value="html"${kind === "html" ? " selected" : ""}>HTML</option>
            </select>
            <span class="note-cell__id">${escapeHtml(id)}${execution}</span>
          </div>

          <div class="note-cell__actions">
            <button type="button" data-action="run-cell">Run</button>
            <button type="button" data-action="move-up">Up</button>
            <button type="button" data-action="move-down">Down</button>
            <button type="button" data-action="delete-cell">Delete</button>
          </div>
        </div>

        <div class="note-cell__editor">
          <textarea spellcheck="false" data-action="edit-source">${escapeHtml(cell.source || "")}</textarea>
        </div>

        ${kind === "markdown" ? `<div class="note-preview">${renderMarkdown(cell.source || "")}</div>` : ""}
        ${renderOutputs(cell.outputs)}
      </article>
    `;
  }

  function renderDocument(documentData) {
    state.document = documentData?.document || documentData;

    if (!state.document) {
      return;
    }

    if (titleEl) {
      titleEl.textContent = state.document.title || state.document.path || "Untitled Vix Note";
    }

    const cells = state.document.cells || [];

    if (cells.length === 0) {
      workspace.innerHTML = `
        <article class="note-empty">
          <h2>Empty note</h2>
          <p>Add a Markdown or C++ cell to start the lesson.</p>
        </article>
      `;
      return;
    }

    workspace.innerHTML = cells.map(renderCell).join("");
  }

  async function loadDocument() {
    setStatus("loading");

    try {
      const data = await requestJson("/api/document");
      renderDocument(data);
      setDirty(false);
    } catch (error) {
      workspace.innerHTML = `
        <article class="note-empty">
          <h2>Cannot load note</h2>
          <p>${escapeHtml(error.message)}</p>
        </article>
      `;
      setStatus("error");
    }
  }

  function findCellElement(target) {
    return target.closest(".note-cell");
  }

  function findCellById(id) {
    return state.document?.cells?.find((cell) => cell.id === id) || null;
  }

  function indexOfCell(id) {
    return state.document?.cells?.findIndex((cell) => cell.id === id) ?? -1;
  }

  async function syncCell(cellElement) {
    const id = cellElement.dataset.cellId;
    const cell = findCellById(id);

    if (!cell) {
      return;
    }

    const source = cellElement.querySelector("[data-action='edit-source']").value;
    const kind = cellElement.querySelector("[data-action='change-kind']").value;

    await requestJson(`/api/cells/${encodeURIComponent(id)}`, {
      method: "PUT",
      body: JSON.stringify({ kind, source })
    });

    cell.source = source;
    cell.kind = kind;
    cellElement.dataset.dirty = "false";
  }

  async function addCell(kind) {
    setStatus("adding");

    const source =
      kind === "cpp"
        ? "#include <iostream>\n\nint main()\n{\n  std::cout << \"Hello from Vix Note\\n\";\n  return 0;\n}\n"
        : "Write your explanation here.";

    await requestJson("/api/cells", {
      method: "POST",
      body: JSON.stringify({ kind, source })
    });

    await loadDocument();
    setDirty(true);
  }

  async function runCell(cellElement) {
    const id = cellElement.dataset.cellId;

    setStatus("running");
    await syncCell(cellElement);

    const data = await requestJson(`/api/cells/${encodeURIComponent(id)}/run`, {
      method: "POST"
    });

    if (data.document) {
      renderDocument({ document: data.document });
    } else {
      await loadDocument();
    }

    setStatus(data.ok ? "ran" : "error");
  }

  async function runAll() {
    setStatus("running");

    const cells = Array.from(document.querySelectorAll(".note-cell"));

    for (const cellElement of cells) {
      if (cellElement.dataset.dirty === "true") {
        await syncCell(cellElement);
      }
    }

    const data = await requestJson("/api/run-all", {
      method: "POST"
    });

    renderDocument({ document: data.document });
    setStatus(data.ok ? "ran" : "error");
  }

  async function moveCell(cellElement, direction) {
    const id = cellElement.dataset.cellId;
    const index = indexOfCell(id);

    if (index < 0) {
      return;
    }

    const targetIndex = direction === "up" ? index - 1 : index + 1;

    if (targetIndex < 0 || targetIndex >= state.document.cells.length) {
      return;
    }

    await syncCell(cellElement);

    const data = await requestJson(`/api/cells/${encodeURIComponent(id)}/move`, {
      method: "POST",
      body: JSON.stringify({ index: targetIndex })
    });

    renderDocument({ document: data.document });
    setDirty(true);
  }

  async function deleteCell(cellElement) {
    const id = cellElement.dataset.cellId;

    await requestJson(`/api/cells/${encodeURIComponent(id)}`, {
      method: "DELETE"
    });

    await loadDocument();
    setDirty(true);
  }

  async function saveDocument() {
    setStatus("saving");

    const cells = Array.from(document.querySelectorAll(".note-cell"));

    for (const cellElement of cells) {
      if (cellElement.dataset.dirty === "true") {
        await syncCell(cellElement);
      }
    }

    await requestJson("/api/document/save", {
      method: "POST"
    });

    setDirty(false);
  }

  document.addEventListener("click", async (event) => {
    const target = event.target;

    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.getAttribute("data-action");

    if (!action) {
      return;
    }

    const cellElement = findCellElement(target);

    try {
      if (action === "add-markdown") {
        await addCell("markdown");
        return;
      }

      if (action === "add-cpp") {
        await addCell("cpp");
        return;
      }

      if (action === "run-all") {
        await runAll();
        return;
      }

      if (action === "save") {
        await saveDocument();
        return;
      }

      if (action === "run-cell" && cellElement) {
        await runCell(cellElement);
        return;
      }

      if (action === "move-up" && cellElement) {
        await moveCell(cellElement, "up");
        return;
      }

      if (action === "move-down" && cellElement) {
        await moveCell(cellElement, "down");
        return;
      }

      if (action === "delete-cell" && cellElement) {
        await deleteCell(cellElement);
      }
    } catch (error) {
      setStatus("error");
      console.error(error);
    }
  });

  document.addEventListener("input", (event) => {
    const target = event.target;

    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.getAttribute("data-action");

    if (action !== "edit-source") {
      return;
    }

    const cellElement = findCellElement(target);

    if (!cellElement) {
      return;
    }

    const id = cellElement.dataset.cellId;
    const cell = findCellById(id);

    if (cell) {
      cell.source = target.value;
    }

    cellElement.dataset.dirty = "true";
    setDirty(true);
  });

  document.addEventListener("change", (event) => {
    const target = event.target;

    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.getAttribute("data-action");

    if (action !== "change-kind") {
      return;
    }

    const cellElement = findCellElement(target);

    if (!cellElement) {
      return;
    }

    const id = cellElement.dataset.cellId;
    const cell = findCellById(id);

    if (cell) {
      cell.kind = target.value;
    }

    cellElement.dataset.dirty = "true";
    setDirty(true);
  });

  document.addEventListener("focusout", async (event) => {
    const target = event.target;

    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.getAttribute("data-action");

    if (action !== "edit-source" && action !== "change-kind") {
      return;
    }

    const cellElement = findCellElement(target);

    if (!cellElement || cellElement.dataset.dirty !== "true") {
      return;
    }

    try {
      await syncCell(cellElement);
    } catch (error) {
      setStatus("error");
      console.error(error);
    }
  });

  document.addEventListener("keydown", async (event) => {
    const target = event.target;

    if (!(target instanceof HTMLTextAreaElement)) {
      return;
    }

    const cellElement = findCellElement(target);

    if (!cellElement) {
      return;
    }

    if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
      event.preventDefault();
      await runCell(cellElement);
      return;
    }

    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
      event.preventDefault();
      await saveDocument();
    }
  });

  loadDocument();
})();
)VIXNOTE";
  }

  bool read_note_asset_file(
      const std::filesystem::path &path,
      std::string &out,
      std::string &err)
  {
    out.clear();
    err.clear();

    std::ifstream in(path, std::ios::binary);

    if (!in.is_open())
    {
      err = "cannot open note asset file: " + path.string();
      return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();

    if (in.bad())
    {
      err = "cannot read note asset file: " + path.string();
      return false;
    }

    out = buffer.str();
    return true;
  }

  std::string note_asset_public_path(const std::filesystem::path &path)
  {
    const std::filesystem::path normalized =
        path.lexically_normal();

    const std::string value =
        normalized.generic_string();

    if (value == "." || value.empty())
    {
      return "/";
    }

    if (value == "index.html")
    {
      return "/";
    }

    if (value == "css/note.css")
    {
      return "/assets/note.css";
    }

    if (value == "js/note.js")
    {
      return "/assets/note.js";
    }

    if (!value.empty() && value.front() == '/')
    {
      return normalize_note_asset_path(value);
    }

    return normalize_note_asset_path("/assets/" + value);
  }

  std::string note_asset_content_type(std::string_view path)
  {
    const std::string normalized =
        normalize_note_asset_path(path);

    if (normalized == "/" || ends_with(normalized, ".html"))
    {
      return "text/html; charset=utf-8";
    }

    if (ends_with(normalized, ".css"))
    {
      return "text/css; charset=utf-8";
    }

    if (ends_with(normalized, ".js"))
    {
      return "application/javascript; charset=utf-8";
    }

    if (ends_with(normalized, ".json"))
    {
      return "application/json; charset=utf-8";
    }

    if (ends_with(normalized, ".svg"))
    {
      return "image/svg+xml";
    }

    if (ends_with(normalized, ".png"))
    {
      return "image/png";
    }

    if (ends_with(normalized, ".jpg") ||
        ends_with(normalized, ".jpeg"))
    {
      return "image/jpeg";
    }

    return "text/plain; charset=utf-8";
  }

  std::string normalize_note_asset_path(std::string_view path)
  {
    if (path.empty())
    {
      return "/";
    }

    std::string normalized(path);

    if (normalized.empty())
    {
      return "/";
    }

    if (normalized.front() != '/')
    {
      normalized.insert(normalized.begin(), '/');
    }

    while (normalized.size() > 1 &&
           normalized.back() == '/')
    {
      normalized.pop_back();
    }

    return normalized;
  }
}
