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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vix::note
{
  namespace
  {
    bool ends_with(std::string_view value, std::string_view suffix)
    {
      return value.size() >= suffix.size() &&
             value.substr(value.size() - suffix.size()) == suffix;
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

  std::string NoteAssets::default_index_html()
  {
    return R"(<!doctype html>
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
      <div>
        <p class="note-eyebrow">Vix Note</p>
        <h1>Visual executable notes for C++</h1>
      </div>

      <div class="note-actions">
        <button type="button" data-action="run-all">Run all</button>
        <button type="button" data-action="save">Save</button>
      </div>
    </header>

    <section class="note-workspace" id="note-workspace">
      <article class="note-cell note-cell--markdown">
        <div class="note-cell__bar">
          <span>Markdown</span>
        </div>
        <div class="note-cell__body">
          <h2>Welcome to Vix Note</h2>
          <p>
            Vix Note is a visual learning workspace for C++ and Vix.cpp.
          </p>
        </div>
      </article>

      <article class="note-cell note-cell--cpp">
        <div class="note-cell__bar">
          <span>C++</span>
          <button type="button" data-action="run-cell">Run</button>
        </div>
        <pre class="note-code"><code>#include &lt;iostream&gt;

int main()
{
  std::cout &lt;&lt; "Hello from Vix Note\n";
  return 0;
}</code></pre>
        <div class="note-output" hidden></div>
      </article>
    </section>
  </main>

  <script src="/assets/note.js"></script>
</body>
</html>
)";
  }

  std::string NoteAssets::default_css()
  {
    return R"(:root {
  color-scheme: light;
  --note-bg: #f6f7fb;
  --note-panel: #ffffff;
  --note-text: #111827;
  --note-muted: #6b7280;
  --note-border: #e5e7eb;
  --note-accent: #d57a2a;
  --note-code-bg: #111827;
  --note-code-text: #f9fafb;
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
  background: var(--note-bg);
  color: var(--note-text);
}

button {
  border: 1px solid var(--note-border);
  border-radius: 10px;
  padding: 0.65rem 0.9rem;
  background: var(--note-panel);
  color: var(--note-text);
  font: inherit;
  cursor: pointer;
}

button:hover {
  border-color: var(--note-accent);
}

.note-shell {
  width: min(1120px, calc(100% - 32px));
  margin: 0 auto;
  padding: 32px 0;
}

.note-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 24px;
  margin-bottom: 24px;
}

.note-eyebrow {
  margin: 0 0 8px;
  color: var(--note-accent);
  font-weight: 700;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.note-header h1 {
  margin: 0;
  font-size: clamp(2rem, 5vw, 4rem);
  line-height: 1;
}

.note-actions {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
}

.note-workspace {
  display: grid;
  gap: 18px;
}

.note-cell {
  overflow: hidden;
  border: 1px solid var(--note-border);
  border-radius: 18px;
  background: var(--note-panel);
  box-shadow: 0 18px 45px rgba(15, 23, 42, 0.06);
}

.note-cell__bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 12px 14px;
  border-bottom: 1px solid var(--note-border);
  color: var(--note-muted);
  font-size: 0.9rem;
  font-weight: 700;
}

.note-cell__body {
  padding: 20px;
}

.note-cell__body h2 {
  margin-top: 0;
}

.note-code {
  margin: 0;
  padding: 20px;
  overflow: auto;
  background: var(--note-code-bg);
  color: var(--note-code-text);
  font-size: 0.95rem;
  line-height: 1.6;
}

.note-output {
  padding: 16px 20px;
  border-top: 1px solid var(--note-border);
  background: #fefce8;
  white-space: pre-wrap;
}

@media (max-width: 720px) {
  .note-header {
    align-items: flex-start;
    flex-direction: column;
  }

  .note-shell {
    width: min(100% - 20px, 1120px);
    padding: 20px 0;
  }
}
)";
  }

  std::string NoteAssets::default_js()
  {
    return R"(() => {
  const workspace = document.querySelector("#note-workspace");

  function showOutput(cell, text) {
    const output = cell.querySelector(".note-output");

    if (!output) {
      return;
    }

    output.hidden = false;
    output.textContent = text;
  }

  document.addEventListener("click", (event) => {
    const target = event.target;

    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.getAttribute("data-action");

    if (action === "run-cell") {
      const cell = target.closest(".note-cell");
      showOutput(cell, "Execution will be connected to the Vix Note kernel.");
      return;
    }

    if (action === "run-all") {
      if (workspace) {
        workspace.dataset.lastRun = new Date().toISOString();
      }

      return;
    }

    if (action === "save") {
      if (workspace) {
        workspace.dataset.lastSave = new Date().toISOString();
      }
    }
  });
})();
)";
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
