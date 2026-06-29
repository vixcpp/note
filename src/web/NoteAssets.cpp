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
    std::string value;
    value.reserve(50310);

    value.append(R"VIXNOTE(<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta
      name="description"
      content="Vix Note is a visual executable note workspace for learning C++ and Vix.cpp faster."
    />

    <title>Vix Note</title>

    <link
      rel="icon"
      type="image/svg+xml"
      href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='14' fill='%23f37726'/%3E%3Cpath d='M18 14h10l7 23 7-23h10L40 50H30L18 14z' fill='white'/%3E%3C/svg%3E"
    />

    <link rel="stylesheet" href="/assets/note.css" />
  </head>

  <body>
    <div class="vn" data-note-app data-kernel="idle">
      <!-- ============ TOP APP BAR (global, minimal) ============ -->
      <header class="vn-AppBar">
        <div class="vn-AppBar__brand">
          <span class="vn-AppBar__logo" aria-hidden="true">V</span>
          <span class="vn-AppBar__title">Vix Note</span>
        </div>

        <nav class="vn-AppBar__menus" aria-label="Main menu" data-menubar>
          <div class="vn-Menu" data-menu="file">
            <button type="button" class="vn-Menu__label" data-menu-button>
              File
            </button>
            <div class="vn-Menu__dropdown" role="menu" hidden>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="new-note"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M13 3v6h6"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M12 12v5M9.5 14.5h5"
                  />
                </svg>
                New note <span class="vn-Menu__hint">⌘N</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="open-note"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M3 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1H3V7z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M3 10h18l-2 8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-.8L3 10z"
                  />
                </svg>
                Open note
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="new-folder"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M12 11v5M9.5 13.5h5"
                  />
                </svg>
                New folder <span class="vn-Menu__hint">⌘⇧N</span>
              </button>
              <div class="vn-Menu__sep"></div>
              <button type="button" class="vn-Menu__item" data-command="save">
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M5 3h11l3 3v15H5V3z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M7 3v5h8V3M8 13h8v5H8z"
                  />
                </svg>
                Save note <span class="vn-Menu__hint">⌘S</span>
              </button>
              <button type="button" class="vn-Menu__item" data-command="reload">
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M20 11a8 8 0 1 0-2.3 5.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M20 5v6h-6"
                  />
                </svg>
                Reload from disk
              </button>
            </div>
          </div>

          <div class="vn-Menu" data-menu="edit">
            <button type="button" class="vn-Menu__label" data-menu-button>
              Edit
            </button>
            <div class="vn-Menu__dropdown" role="menu" hidden>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="insert-below"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="4"
                    y="4"
                    width="16"
                    height="7"
                    rx="1.5"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M12 15v5M9.5 17.5h5"
                  />
                </svg>
                Insert cell below <span class="vn-Menu__hint">B</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="cut-cell"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M5 7h14M10 7V5a1 1 0 0 1 1-1h2a1 1 0 0 1 1 1v2M6 7l1 13a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1l1-13"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M10 11v6M14 11v6"
                  />
                </svg>
                Delete selected cell <span class="vn-Menu__hint">D D</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="duplicate"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="9"
                    y="9"
                    width="11"
                    height="11"
                    rx="2"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M5 15H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v1"
                  />
                </svg>
                Duplicate selected cell
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="move-up"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M12 20V8M6 12l6-6 6 6M5 4h14"
                  />
                </svg>
                Move cell up
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="move-down"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M12 4v12M6 12l6 6 6-6M5 20h14"
                  />
                </svg>
                Move cell down
              </button>
              <div class="vn-Menu__sep"></div>
              <button type="button" class="vn-Menu__item" data-command="to-cpp">
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M8 8l-4 4 4 4M16 8l4 4-4 4M13.5 5l-3 14"
                  />
                </svg>
                Change to C++ <span class="vn-Menu__hint">Y</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="to-markdown"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="3"
                    y="6"
                    width="18"
                    height="12"
                    rx="2"
                    fill="none"
)VIXNOTE");

    value.append(R"VIXNOTE(                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M6 15v-6l2.5 2.5L11 9v6M16 9v5M14 12l2 2 2-2"
                  />
                </svg>
                Change to Markdown <span class="vn-Menu__hint">M</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="to-reply"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="3"
                    y="4"
                    width="18"
                    height="16"
                    rx="2"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M7 9l3 3-3 3M12.5 15h4"
                  />
                </svg>
                Change to Reply <span class="vn-Menu__hint">R</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="to-html"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="3"
                    y="5"
                    width="18"
                    height="14"
                    rx="2"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M9 9.5L6.5 12 9 14.5M15 9.5l2.5 2.5L15 14.5"
                  />
                </svg>
                Change to HTML
              </button>
            </div>
          </div>

          <div class="vn-Menu" data-menu="view">
            <button type="button" class="vn-Menu__label" data-menu-button>
              View
            </button>
            <div class="vn-Menu__dropdown" role="menu" hidden>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="show-explorer"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M8 12h8M8 15h5"
                  />
                </svg>
                Show Explorer
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="show-problems"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M12 3l9 16H3l9-16z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M12 10v4"
                  />
                  <circle cx="12" cy="16.5" r="1" fill="currentColor" />
                </svg>
                Show Problems
              </button>
              <div class="vn-Menu__sep"></div>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="toggle-sidebar"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="3"
                    y="4"
                    width="18"
                    height="16"
                    rx="2"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    d="M9 4v16"
                  />
                </svg>
                Toggle sidebar <span class="vn-Menu__hint">⌘B</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="toggle-focus"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <circle
                    cx="12"
                    cy="12"
                    r="3.5"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M12 3v3M12 18v3M3 12h3M18 12h3"
                  />
                </svg>
                Toggle focus mode
              </button>
            </div>
          </div>

          <div class="vn-Menu" data-menu="run">
            <button type="button" class="vn-Menu__label" data-menu-button>
              Run
            </button>
            <div class="vn-Menu__dropdown" role="menu" hidden>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="run-cell"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M8 5.5v13l11-6.5-11-6.5z"
                  />
                </svg>
                Run selected cell <span class="vn-Menu__hint">⌘↵</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="run-advance"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M5 5v10l8-5-8-5z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M18 6v10m-3-3l3 3 3-3"
                  />
                </svg>
                Run and advance <span class="vn-Menu__hint">⇧↵</span>
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="run-all"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M5 5.5v13l9-6.5-9-6.5z"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M18 6v12"
                  />
                </svg>
                Run all cells
              </button>
              <div class="vn-Menu__sep"></div>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="clear-cell"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M16 4l4 4-9 9H7l-3-3 9-10zM9 10l5 5"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M11 21h9"
                  />
                </svg>
                Clear selected output
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="clear-all"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M15 4l5 5-8 8H8l-4-4 11-9zM9 9l6 6"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M5 21h14"
                  />
                </svg>
                Clear all outputs
              </button>
              <div class="vn-Menu__sep"></div>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="restart"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M20 11a8 8 0 1 0-2.3 5.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
)VIXNOTE");

    value.append(R"VIXNOTE(                    stroke-linejoin="round"
                    d="M20 5v6h-6"
                  />
                </svg>
                Restart kernel
              </button>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="restart-run"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M16 9a6 6 0 1 0-1.7 4.2"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    d="M16 4v5h-5"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linejoin="round"
                    d="M11 15v6l5-3-5-3z"
                  />
                </svg>
                Restart and run all
              </button>
            </div>
          </div>

          <div class="vn-Menu" data-menu="help">
            <button type="button" class="vn-Menu__label" data-menu-button>
              Help
            </button>
            <div class="vn-Menu__dropdown" role="menu" hidden>
              <button
                type="button"
                class="vn-Menu__item"
                data-command="shortcuts"
              >
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <rect
                    x="3"
                    y="6"
                    width="18"
                    height="12"
                    rx="2"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M7 10h0M11 10h0M15 10h0M8 14h8"
                  />
                </svg>
                Keyboard shortcuts <span class="vn-Menu__hint">?</span>
              </button>
              <div class="vn-Menu__sep"></div>
              <button type="button" class="vn-Menu__item" data-command="about">
                <svg
                  class="vn-Menu__icon"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <circle
                    cx="12"
                    cy="12"
                    r="9"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                  />
                  <path
                    fill="none"
                    stroke="currentColor"
                    stroke-width="1.7"
                    stroke-linecap="round"
                    d="M12 11v5"
                  />
                  <circle cx="12" cy="7.8" r="1" fill="currentColor" />
                </svg>
                About Vix Note
              </button>
            </div>
          </div>
        </nav>

        <div class="vn-AppBar__spacer"></div>

        <div class="vn-AppBar__status">
          <span class="vn-KernelName" data-note-project>No project</span>
          <span class="vn-KernelStatus" title="Kernel status">
            <span class="vn-KernelStatus__dot"></span>
            <span class="vn-KernelStatus__text" data-note-kernel>Idle</span>
          </span>
        </div>
      </header>

      <!-- ============ BODY: activity bar + sidebar + editor ============ -->
      <div class="vn-Body">
        <!-- ACTIVITY BAR (VS Code style vertical rail) -->
        <nav class="vn-Activity" aria-label="Activity bar">
          <button
            class="vn-Activity__btn is-active"
            type="button"
            data-activity="explorer"
            title="Explorer"
            aria-label="Explorer"
          >
            <!-- files / explorer (solid) -->
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path
                d="M6 2h7l5 5v13a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2zm7 1.5V8h4.5L13 3.5z"
              />
            </svg>
          </button>
          <button
            class="vn-Activity__btn"
            type="button"
            data-activity="problems"
            title="Problems"
            aria-label="Problems"
          >
            <!-- problems / diagnostics (triangle alert, stroke) -->
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path
                fill="none"
                stroke="currentColor"
                stroke-width="1.9"
                stroke-linejoin="round"
                d="M12 3l9 16H3l9-16z"
              />
              <path
                fill="none"
                stroke="currentColor"
                stroke-width="1.9"
                stroke-linecap="round"
                d="M12 10v4"
              />
              <circle cx="12" cy="16.5" r="1.1" fill="currentColor" />
            </svg>
            <span
              class="vn-Activity__badge"
              data-activity-problems-badge
              hidden
            ></span>
          </button>

          <div class="vn-Activity__spacer"></div>

          <button
            class="vn-Activity__btn"
            type="button"
            data-action="shortcuts"
            title="Keyboard shortcuts"
            aria-label="Keyboard shortcuts"
          >
            <!-- keyboard (stroke) -->
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <rect
                x="3"
                y="6"
                width="18"
                height="12"
                rx="2"
                fill="none"
                stroke="currentColor"
                stroke-width="1.7"
              />
              <path
                fill="none"
                stroke="currentColor"
                stroke-width="1.7"
                stroke-linecap="round"
                d="M7 10h0M11 10h0M15 10h0M8 14h8"
              />
            </svg>
          </button>
        </nav>

        <!-- SIDEBAR (resizable) -->
        <aside class="vn-Sidebar" aria-label="Sidebar" data-sidebar>
          <div class="vn-Sidebar__inner">
            <!-- EXPLORER PANEL -->
            <section class="vn-Panel" data-panel="explorer">
              <header class="vn-Panel__head">
                <span class="vn-Panel__title">
                  Explorer
                  <span class="vn-Panel__count" data-explorer-count>0</span>
                </span>
                <div class="vn-Panel__actions">
                  <button
                    type="button"
                    class="vn-IconBtn"
                    data-action="new-note"
                    title="New note"
                    aria-label="New note"
                  >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linejoin="round"
                        d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"
                      />
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linejoin="round"
                        d="M13 3v6h6"
                      />
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linecap="round"
                        d="M12 12v5M9.5 14.5h5"
                      />
                    </svg>
                  </button>
                  <button
                    type="button"
                    class="vn-IconBtn"
                    data-action="new-folder"
                    title="New folder"
                    aria-label="New folder"
                  >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linejoin="round"
                        d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"
                      />
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linecap="round"
                        d="M12 11v5M9.5 13.5h5"
                      />
                    </svg>
                  </button>
                  <button
                    type="button"
                    class="vn-IconBtn"
                    data-action="open-note"
                    title="Open note"
                    aria-label="Open note"
                  >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linejoin="round"
                        d="M3 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1H3V7z"
                      />
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linejoin="round"
                        d="M3 10h18l-2 8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-.8L3 10z"
                      />
                    </svg>
                  </button>
                  <button
                    type="button"
                    class="vn-IconBtn"
                    data-action="refresh"
                    title="Refresh"
                    aria-label="Refresh"
                  >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        d="M20 11a8 8 0 1 0-2.3 5.7"
                      />
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        d="M20 5v6h-6"
                      />
                    </svg>
                  </button>
                </div>
              </header>

              <div class="vn-Panel__search">
                <svg
                  viewBox="0 0 24 24"
                  class="vn-Panel__searchIcon"
                  aria-hidden="true"
                >
                  <circle
                    cx="11"
                    cy="11"
                    r="7"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="2"
                  />
                  <path
                    d="M21 21l-4.35-4.35"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="2"
                    stroke-linecap="round"
                  />
)VIXNOTE");

    value.append(R"VIXNOTE(                </svg>
                <input
                  type="text"
                  data-explorer-search
                  placeholder="Filter notes…"
                  autocomplete="off"
                  spellcheck="false"
                />
              </div>

              <div class="vn-Tree" data-explorer-list aria-label="Files">
                <p class="vn-Tree__empty">No notes yet.</p>
              </div>
            </section>

            <!-- PROBLEMS / DIAGNOSTICS PANEL -->
            <section class="vn-Panel" data-panel="problems" hidden>
              <header class="vn-Panel__head">
                <span class="vn-Panel__title">
                  Problems
                  <span class="vn-Panel__count" data-problems-count>0</span>
                </span>
                <div class="vn-Panel__actions">
                  <button
                    type="button"
                    class="vn-IconBtn"
                    data-action="run-all"
                    title="Run all cells"
                    aria-label="Run all cells"
                  >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linejoin="round"
                        d="M5 5.5v13l9-6.5-9-6.5z"
                      />
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linecap="round"
                        d="M18 6v12"
                      />
                    </svg>
                  </button>
                  <button
                    type="button"
                    class="vn-IconBtn"
                    data-action="clear-problems"
                    title="Clear problems"
                    aria-label="Clear problems"
                  >
                    <svg viewBox="0 0 24 24" aria-hidden="true">
                      <path
                        fill="none"
                        stroke="currentColor"
                        stroke-width="1.7"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        d="M6 7h12M9 7V5h6v2M8 7l1 13h6l1-13"
                      />
                    </svg>
                  </button>
                </div>
              </header>

              <div
                class="vn-Problems__summary"
                data-problems-summary
                data-state="idle"
              >
                <span class="vn-Problems__dot" aria-hidden="true"></span>
                <span
                  class="vn-Problems__summaryText"
                  data-problems-summary-text
                  >No problems detected.</span
                >
              </div>

              <div class="vn-Problems" data-problems-list aria-label="Problems">
                <p class="vn-Tree__empty">
                  No problems detected. Run a C++ or Reply cell to see compiler
                  and runtime diagnostics here.
                </p>
              </div>
            </section>
          </div>

          <div
            class="vn-Sidebar__resizer"
            data-sidebar-resizer
            role="separator"
            aria-orientation="vertical"
            aria-label="Resize sidebar"
            tabindex="0"
            title="Drag to resize · double-click to reset"
          ></div>
        </aside>

        <!-- EDITOR COLUMN (tabs + toolbar + notebook) -->
        <div class="vn-Editor-col">
          <!-- CENTER TABS BAR -->
          <div class="vn-TabsBar" data-tabs-bar aria-label="Open note tabs">
            <div class="vn-TabsBar__empty">No open notes</div>
          </div>

          <!-- NOTEBOOK TOOLBAR (scoped to the editor zone) -->
          <div class="vn-Toolbar" role="toolbar" aria-label="Notebook toolbar">
            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="save"
              title="Save (Ctrl/⌘ S)"
              aria-label="Save"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M5 3h11l3 3v15H5V3zm2 2v4h8V5H7zm0 7v6h10v-6H7z" />
              </svg>
            </button>

            <span class="vn-Toolbar__sep"></span>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="insert-below"
              title="Insert cell below (B)"
              aria-label="Insert cell below"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M11 5h2v6h6v2h-6v6h-2v-6H5v-2h6V5z" />
              </svg>
            </button>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="cut-cell"
              title="Delete selected cell (D D)"
              aria-label="Delete cell"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M6 7h12l-1 14H7L6 7zm3-3h6l1 2H8l1-2z" />
              </svg>
            </button>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="duplicate"
              title="Duplicate cell"
              aria-label="Duplicate cell"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path
                  d="M16 1H4a2 2 0 00-2 2v14h2V3h12V1zm3 4H8a2 2 0 00-2 2v14a2 2 0 002 2h11a2 2 0 002-2V7a2 2 0 00-2-2zm0 16H8V7h11v14z"
                />
              </svg>
            </button>

            <span class="vn-Toolbar__sep"></span>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="move-up"
              title="Move cell up"
              aria-label="Move cell up"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M12 7l6 6-1.4 1.4L12 9.8l-4.6 4.6L6 13l6-6z" />
              </svg>
            </button>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="move-down"
              title="Move cell down"
              aria-label="Move cell down"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M12 17l-6-6 1.4-1.4L12 14.2l4.6-4.6L18 11l-6 6z" />
              </svg>
            </button>

            <span class="vn-Toolbar__sep"></span>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="run-cell"
              title="Run selected cell (Ctrl/⌘ Enter)"
              aria-label="Run cell"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M8 5v14l11-7z" />
              </svg>
            </button>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="run-all"
              title="Run all cells"
              aria-label="Run all cells"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M5 5v14l8-7-8-7zm9 0v14l8-7-8-7z" />
              </svg>
            </button>

            <button
              class="vn-ToolbarButton"
              type="button"
              data-action="restart"
              title="Restart kernel"
              aria-label="Restart"
            >
              <svg viewBox="0 0 24 24" class="vn-icon">
                <path d="M12 6V3L8 7l4 4V8a4 4 0 11-4 4H6a6 6 0 106-6z" />
              </svg>
            </button>

            <span class="vn-Toolbar__sep"></span>

            <div class="vn-CellTypeSelect" data-cell-type-select>
              <button
                class="vn-CellTypeSelect__button"
                type="button"
                data-action="toolbar-kind"
                data-kind="cpp"
                aria-label="Cell type"
                aria-haspopup="listbox"
                aria-expanded="false"
              >
                <span class="vn-CellTypeSelect__label" data-toolbar-kind-label
                  >C++</span
                >
                <svg
                  class="vn-CellTypeSelect__chevron"
                  viewBox="0 0 24 24"
                  aria-hidden="true"
                >
                  <path d="M7 10l5 5 5-5z" />
                </svg>
              </button>

              <div
                class="vn-CellTypeSelect__menu"
                data-toolbar-kind-menu
                role="listbox"
                hidden
              >
                <button
                  type="button"
                  class="vn-CellTypeSelect__option is-active"
                  data-kind-option="cpp"
                  role="option"
                  aria-selected="true"
                >
                  <span class="vn-CellTypeSelect__optionMain">C++</span>
                  <span class="vn-CellTypeSelect__optionHint"
                    >Executable C++ cell</span
                  >
                </button>

                <button
                  type="button"
                  class="vn-CellTypeSelect__option"
                  data-kind-option="reply"
                  role="option"
                  aria-selected="false"
                >
                  <span class="vn-CellTypeSelect__optionMain">Reply</span>
                  <span class="vn-CellTypeSelect__optionHint"
                    >Reply script cell</span
                  >
                </button>

                <button
                  type="button"
                  class="vn-CellTypeSelect__option"
                  data-kind-option="markdown"
                  role="option"
                  aria-selected="false"
                >
                  <span class="vn-CellTypeSelect__optionMain">Markdown</span>
                  <span class="vn-CellTypeSelect__optionHint"
                    >Text and notes</span
                  >
                </button>

                <button
                  type="button"
                  class="vn-CellTypeSelect__option"
                  data-kind-option="html"
                  role="option"
                  aria-selected="false"
                >
                  <span class="vn-CellTypeSelect__optionMain">HTML</span>
                  <span class="vn-CellTypeSelect__optionHint"
                    >Rendered HTML cell</span
                  >
                </button>
              </div>
            </div>

            <div class="vn-Toolbar__grow"></div>
          </div>

          <!-- NOTEBOOK PANEL -->
          <main class="vn-NotebookPanel" id="workspace">
            <div
              class="vn-Notice"
              data-note-message
              hidden
              aria-live="polite"
            ></div>

            <div
              class="vn-Notebook"
              data-note-cells
              aria-label="Vix Note cells"
              tabindex="0"
            >
              <div class="vn-Notebook__loading">
                <span class="vn-spinner" aria-hidden="true"></span>
                Loading note document…
              </div>
            </div>
          </main>
        </div>
      </div>

      <!-- ============ STATUS BAR (fixed-width slots, never reflows) ============ -->
      <footer class="vn-StatusBar" aria-label="Status bar">
        <div class="vn-StatusBar__left">
          <span class="vn-StatusBar__mode" data-status-mode>Command</span>
          <span class="vn-StatusBar__sep">·</span>
          <span class="vn-StatusBar__pos" data-status-position
            >Cell 0 of 0</span
          >
          <span class="vn-StatusBar__sep">·</span>
          <button
            type="button"
            class="vn-StatusBar__problems"
            data-status-problems
)VIXNOTE");

    value.append(R"VIXNOTE(            title="Show problems"
          >
            <span class="vn-StatusBar__problemsIcon" aria-hidden="true">⚠</span>
            <span data-status-problems-count>0</span>
          </button>
        </div>
        <div class="vn-StatusBar__right">
          <span class="vn-StatusBar__kind" data-status-kind>—</span>
          <span class="vn-StatusBar__sep">·</span>
          <span class="vn-StatusBar__cells">
            <span data-note-cell-count>0</span> cells ·
            <span data-note-exec-count>0</span> run
          </span>
          <span class="vn-StatusBar__sep">·</span>
          <span class="vn-StatusBar__kernel">
            <span class="vn-StatusBar__dot"></span>
            <span class="vn-StatusBar__kernelText" data-status-kernel
              >Idle</span
            >
          </span>
        </div>
      </footer>

      <!-- ============ MODAL (forms / confirm / info) ============ -->
      <div class="vn-Modal" data-modal hidden role="dialog" aria-modal="true">
        <div class="vn-Modal__backdrop" data-modal-close></div>
        <div class="vn-Modal__box">
          <header class="vn-Modal__head">
            <span class="vn-Modal__icon" data-modal-icon aria-hidden="true">
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <circle
                  cx="12"
                  cy="12"
                  r="9"
                  fill="none"
                  stroke="currentColor"
                  stroke-width="1.7"
                />
                <path
                  fill="none"
                  stroke="currentColor"
                  stroke-width="1.7"
                  stroke-linecap="round"
                  d="M12 11v5"
                />
                <circle cx="12" cy="7.8" r="1" fill="currentColor" />
              </svg>
            </span>
            <h2 class="vn-Modal__title" data-modal-title>Title</h2>
            <button
              type="button"
              class="vn-Modal__close"
              data-modal-close
              aria-label="Close"
            >
              ×
            </button>
          </header>
          <div class="vn-Modal__body" data-modal-body></div>
          <footer class="vn-Modal__foot" data-modal-foot></footer>
        </div>
      </div>
    </div>

    <script src="/assets/note.js"></script>
  </body>
</html>
)VIXNOTE");

    return value;
  }

  std::string NoteAssets::default_css()
  {
    std::string value;
    value.reserve(45042);

    value.append(R"VIXNOTE(/**
 *
 *  @file note.css
 *  @author Gaspard Kirira
 *
 *  Vix Note — notebook UI
 *  Lightweight editor: top app bar, activity bar, explorer/problems sidebar,
 *  central tabs + notebook toolbar, stable status bar, custom modals,
 *  VS Code-style inline create/rename, a Problems (diagnostics) panel, and
 *  native drag-and-drop for explorer entries and editor tabs.
 *
 *  Copyright 2026, Gaspard Kirira. MIT License.
 */

/* ============================================================
   DESIGN TOKENS — light notebook palette
   ============================================================ */
:root {
  color-scheme: light;

  --vn-color0: #ffffff;
  --vn-color1: #ffffff;
  --vn-color2: #f5f5f5;
  --vn-color3: #e9e9e9;
  --vn-color4: #bdbdbd;

  --vn-border1: #c8c8c8;
  --vn-border2: #e2e2e2;
  --vn-border3: #eeeeee;

  --vn-toolbar-bg: #ffffff;
  --vn-appbar-bg: #f7f7f7;
  --vn-activity-bg: #ededed;
  --vn-sidebar-bg: #f3f3f3;
  --vn-tabsbar-bg: #f0f0f0;

  --vn-text0: #000000;
  --vn-text1: #212121;
  --vn-text2: #5f6368;
  --vn-text3: #9aa0a6;

  --vn-content1: #212121;

  /* Vix amber/orange brand */
  --vn-brand0: #b2541a;
  --vn-brand1: #f37726;
  --vn-brand2: #f9aa7c;
  --vn-brand3: #fde0cd;

  --vn-accent1: #1976d2;

  /* Cell prompts */
  --vn-prompt-width: 64px;
  --vn-prompt-not-active: #b0b0b0;
  --vn-editor-bg: #f6f6f6;
  --vn-editor-border: #d4d4d4;
  --vn-editor-active-bg: #ffffff;
  --vn-inprompt: #303f9f;
  --vn-outprompt: #d84315;

  /* Cell selection bar */
  --vn-collapser-width: 8px;
  --vn-collapser-selected: #1976d2;
  --vn-collapser-edit: #66bb6a;

  /* Outputs */
  --vn-error-bg: #ffdddd;
  --vn-error: #b71c1c;
  --vn-stderr-bg: #fdf2f2;

  /* Diagnostics / Problems */
  --vn-problem-error: #c62828;
  --vn-problem-error-bg: #fdecea;
  --vn-problem-hint: #1565c0;
  --vn-problem-hint-bg: #e8f1fc;
  --vn-problem-ok: #2e7d32;

  /* Code syntax */
  --vn-kw: #008000;
  --vn-type: #1d6fb8;
  --vn-str: #ba2121;
  --vn-com: #408080;
  --vn-num: #098658;
  --vn-pre: #9c27b0;
  --vn-fn: #795548;
  --vn-punct: #303030;

  --vn-ui-font:
    -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
  --vn-code-font:
    "Menlo", "Consolas", "DejaVu Sans Mono", "Liberation Mono", monospace;
  --vn-code-size: 13px;
  --vn-code-lh: 1.45;

  --vn-appbar-h: 36px;
  --vn-toolbar-h: 38px;
  --vn-tabsbar-h: 36px;
  --vn-statusbar-h: 24px;
  --vn-activity-w: 48px;

  /* fixed slot so Idle/Busy/Error swaps never reflow neighbors */
  --vn-kernel-slot: 46px;

  /* sidebar width, JS-controlled */
  --vn-sidebar-w: 260px;
}

* {
  box-sizing: border-box;
}

html,
body {
  height: 100%;
  margin: 0;
}

body {
  font-family: var(--vn-ui-font);
  font-size: 13px;
  color: var(--vn-text1);
  background: var(--vn-color2);
  overflow: hidden;
}

button,
select,
textarea,
input {
  font: inherit;
}

/* ============================================================
   APP SHELL
   ============================================================ */
.vn {
  display: flex;
  flex-direction: column;
  height: 100vh;
  height: 100dvh;
  background: var(--vn-color2);
}

/* ============================================================
   TOP APP BAR
   ============================================================ */
.vn-AppBar {
  display: flex;
  align-items: center;
  height: var(--vn-appbar-h);
  padding: 0 6px;
  background: var(--vn-appbar-bg);
  border-bottom: 1px solid var(--vn-border2);
  user-select: none;
  flex: 0 0 auto;
  position: relative;
  z-index: 1000;
  overflow: visible;
}

.vn-AppBar__brand {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 0 12px 0 4px;
}
.vn-AppBar__logo {
  display: grid;
  place-items: center;
  width: 19px;
  height: 19px;
  border-radius: 5px;
  background: var(--vn-brand1);
  color: #fff;
  font-weight: 800;
  font-size: 11px;
}
.vn-AppBar__title {
  font-weight: 650;
  font-size: 12.5px;
  color: var(--vn-text1);
  white-space: nowrap;
}

.vn-AppBar__menus {
  display: flex;
  align-items: stretch;
  height: 100%;
}

/* dropdown menu */
.vn-Menu {
  position: relative;
  display: flex;
  align-items: stretch;
}
.vn-Menu__label {
  border: 0;
  background: transparent;
  color: var(--vn-text1);
  padding: 0 10px;
  cursor: pointer;
  font-size: 12.5px;
  border-radius: 4px;
}
.vn-Menu__label:hover,
.vn-Menu.is-open .vn-Menu__label {
  background: var(--vn-color3);
}

.vn-Menu__dropdown {
  position: absolute;
  top: 100%;
  left: 0;
  min-width: 234px;
  padding: 5px;
  background: var(--vn-color1);
  border: 1px solid var(--vn-border1);
  border-radius: 8px;
  box-shadow: 0 10px 28px rgba(0, 0, 0, 0.16);
  z-index: 1001;
}
.vn-Menu__dropdown[hidden] {
  display: none;
}
.vn-Menu__item {
  display: flex;
  align-items: center;
  gap: 9px;
  width: 100%;
  border: 0;
  background: transparent;
  color: var(--vn-text1);
  text-align: left;
  padding: 7px 10px;
  border-radius: 5px;
  cursor: pointer;
  font-size: 12.5px;
}
.vn-Menu__item:hover {
  background: var(--vn-accent1);
  color: #fff;
}
.vn-Menu__hint {
  margin-left: auto;
  color: var(--vn-text3);
  font-size: 11px;
  font-family: var(--vn-code-font);
  white-space: nowrap;
}
.vn-Menu__item:hover .vn-Menu__hint {
  color: rgba(255, 255, 255, 0.85);
}
.vn-Menu__sep {
  height: 1px;
  margin: 5px 6px;
  background: var(--vn-border2);
}

.vn-AppBar__spacer {
  flex: 1 1 auto;
}

.vn-AppBar__status {
  display: flex;
  align-items: center;
  gap: 12px;
  padding-right: 8px;
}
.vn-KernelName {
  color: var(--vn-text2);
  font-size: 12px;
  max-width: 200px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.vn-KernelStatus {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: var(--vn-text2);
}
.vn-KernelStatus__text {
  display: inline-block;
  min-width: var(--vn-kernel-slot);
  text-align: left;
}
.vn-KernelStatus__dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  border: 1.5px solid var(--vn-text3);
  background: transparent;
  flex: 0 0 auto;
  transition:
    background 0.15s,
    border-color 0.15s;
}

.vn[data-kernel="idle"] .vn-KernelStatus__dot,
.vn[data-kernel="idle"] .vn-StatusBar__dot {
  border-color: #9e9e9e;
  background: transparent;
}
.vn[data-kernel="busy"] .vn-KernelStatus__dot,
.vn[data-kernel="busy"] .vn-StatusBar__dot {
  border-color: var(--vn-brand1);
  background: var(--vn-brand1);
  animation: vn-pulse 1s ease-in-out infinite;
}
.vn[data-kernel="error"] .vn-KernelStatus__dot,
.vn[data-kernel="error"] .vn-StatusBar__dot {
  border-color: var(--vn-error);
  background: var(--vn-error);
}
@keyframes vn-pulse {
  0%,
  100% {
    opacity: 1;
  }
  50% {
    opacity: 0.3;
  }
}

.vn-icon {
  width: 18px;
  height: 18px;
  fill: var(--vn-text2);
}

/* ============================================================
   BODY: activity bar + sidebar + editor column
   ============================================================ */
.vn-Body {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
  position: relative;
}

/* ---------- Activity bar ---------- */
.vn-Activity {
  flex: 0 0 var(--vn-activity-w);
  width: var(--vn-activity-w);
  background: var(--vn-activity-bg);
  border-right: 1px solid var(--vn-border2);
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 6px 0;
  gap: 2px;
}
.vn-Activity__btn {
  position: relative;
  display: grid;
  place-items: center;
  width: 40px;
  height: 40px;
  border: 0;
  border-radius: 6px;
  background: transparent;
  cursor: pointer;
  color: var(--vn-text2);
}
.vn-Activity__btn svg {
  width: 22px;
  height: 22px;
  fill: currentColor;
}
.vn-Activity__btn:hover {
  background: var(--vn-color3);
  color: var(--vn-text1);
}
.vn-Activity__btn.is-active {
  color: var(--vn-brand0);
}
.vn-Activity__btn.is-active::before {
  content: "";
  position: absolute;
  left: -6px;
  top: 8px;
  bottom: 8px;
  width: 2px;
  border-radius: 2px;
  background: var(--vn-brand1);
}
.vn-Activity__spacer {
  flex: 1 1 auto;
}

/* Problems badge on the activity bar button */
.vn-Activity__badge {
  position: absolute;
  top: 4px;
  right: 4px;
  min-width: 15px;
  height: 15px;
  padding: 0 4px;
  border-radius: 8px;
  background: var(--vn-problem-error);
  color: #fff;
  font-size: 9.5px;
  font-weight: 800;
  line-height: 15px;
  text-align: center;
}
.vn-Activity__badge[hidden] {
  display: none;
}

/* ---------- Sidebar ---------- */
.vn-Sidebar {
  position: relative;
  width: var(--vn-sidebar-w);
  flex: 0 0 var(--vn-sidebar-w);
  background: var(--vn-sidebar-bg);
  border-right: 1px solid var(--vn-border2);
  display: flex;
  min-height: 0;
}
.vn-Sidebar__inner {
  flex: 1 1 auto;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  min-width: 0;
}
.vn.is-sidebar-collapsed .vn-Sidebar {
  width: 0;
  flex-basis: 0;
  border-right: 0;
  overflow: hidden;
}
.vn.is-sidebar-collapsed .vn-Sidebar__resizer {
  display: none;
}

/* resize handle */
.vn-Sidebar__resizer {
  position: absolute;
  top: 0;
  right: -3px;
  width: 6px;
  height: 100%;
  cursor: col-resize;
  z-index: 5;
  background: transparent;
}
.vn-Sidebar__resizer::after {
  content: "";
  position: absolute;
  top: 0;
  left: 3px;
  width: 1px;
  height: 100%;
  background: transparent;
  transition: background 0.12s;
}
.vn-Sidebar__resizer:hover::after,
.vn-Sidebar__resizer:focus-visible::after,
.vn.is-resizing .vn-Sidebar__resizer::after {
  background: var(--vn-brand1);
  width: 2px;
}
.vn.is-resizing {
  cursor: col-resize;
  user-select: none;
}

/* ---------- Panels (explorer / problems) ---------- */
.vn-Panel {
  flex: 1 1 auto;
  min-height: 0;
  display: flex;
  flex-direction: column;
}
.vn-Panel[hidden] {
  display: none;
}
.vn-Panel__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  padding: 8px 8px 8px 12px;
  flex: 0 0 auto;
}
.vn-Panel__title {
  display: flex;
  align-items: center;
  gap: 7px;
  font-size: 10.5px;
  font-weight: 700;
  letter-spacing: 0.07em;
  text-transform: uppercase;
  color: var(--vn-text2);
}
.vn-Panel__count {
  display: inline-grid;
  place-items: center;
  min-width: 18px;
  height: 16px;
  padding: 0 5px;
  border-radius: 8px;
  background: var(--vn-color3);
  color: var(--vn-text2);
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 0;
}
.vn-Panel__actions {
  display: flex;
  gap: 1px;
}
.vn-IconBtn {
  display: grid;
  place-items: center;
  width: 26px;
  height: 24px;
  border: 0;
  border-radius: 5px;
  background: transparent;
  cursor: pointer;
  color: var(--vn-text2);
}
.vn-IconBtn svg {
  width: 16px;
  height: 16px;
  fill: currentColor;
}
.vn-IconBtn:hover {
  background: var(--vn-color3);
  color: var(--vn-text1);
}
.vn-Panel__search {
  display: flex;
  align-items: center;
  gap: 6px;
  margin: 0 8px 6px;
  padding: 0 8px;
  height: 28px;
  border: 1px solid var(--vn-border1);
  border-radius: 6px;
  background: var(--vn-color1);
}
.vn-Panel__search:focus-within {
  border-color: var(--vn-brand2);
}
.vn-Panel__searchIcon {
  width: 13px;
  height: 13px;
  color: var(--vn-text3);
  flex: 0 0 auto;
}
.vn-Panel__search input {
  flex: 1 1 auto;
  border: 0;
  outline: none;
  background: transparent;
  color: var(--vn-text1);
  font-size: 12.5px;
  min-width: 0;
}
.vn-Menu__icon {
  width: 16px;
  height: 16px;
  flex: 0 0 16px;
  color: currentColor;
}
.vn-Menu__item:hover .vn-Menu__icon {
  color: #fff;
}

.vn-Modal__icon {
  display: inline-grid;
  place-items: center;
  width: 22px;
  height: 22px;
  margin-right: 10px;
  color: var(--vn-brand0);
  flex: 0 0 auto;
}
.vn-Modal__icon svg {
  width: 20px;
  height: 20px;
}

/* ---------- File tree ---------- */
.vn-Tree {
  flex: 1 1 auto;
  overflow-y: auto;
  padding: 2px 6px 12px;
}
.vn-Tree__empty {
  margin: 8px 8px;
  font-size: 12px;
  color: var(--vn-text3);
  line-height: 1.5;
}
.vn-Tree__row {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 4px 6px;
)VIXNOTE");

    value.append(R"VIXNOTE(  padding-left: calc(6px + var(--depth, 0) * 14px);
  border-radius: 5px;
  cursor: pointer;
  color: var(--vn-text1);
  outline: none;
}
.vn-Tree__row:hover {
  background: var(--vn-color3);
}
.vn-Tree__row:focus-visible {
  box-shadow: inset 0 0 0 1px var(--vn-accent1);
}
.vn-Tree__row.is-active {
  background: var(--vn-brand3);
}
.vn-Tree__icon {
  width: 15px;
  height: 15px;
  flex: 0 0 auto;
  fill: var(--vn-text3);
}
.vn-Tree__row.is-active .vn-Tree__icon {
  fill: var(--vn-brand0);
}
.vn-Tree__row.is-loading {
  opacity: 0.72;
}
.vn-Tree__row.is-loading .vn-Tree__label::after {
  content: " loading…";
  color: var(--vn-text3);
  font-size: 11px;
}
.vn-Tree__label {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 12.5px;
}
.vn-Tree__meta {
  flex: 0 0 auto;
  font-size: 10.5px;
  color: var(--vn-text3);
  white-space: nowrap;
}
.vn-Tree__menuBtn {
  flex: 0 0 auto;
  width: 20px;
  height: 20px;
  border: 0;
  border-radius: 4px;
  background: transparent;
  color: var(--vn-text3);
  cursor: pointer;
  font-size: 14px;
  line-height: 1;
  opacity: 0;
  display: grid;
  place-items: center;
}
.vn-Tree__row:hover .vn-Tree__menuBtn {
  opacity: 1;
}
.vn-Tree__menuBtn:hover {
  background: var(--vn-color4);
  color: var(--vn-text1);
}
.vn-Tree__chevron {
  display: inline-grid;
  place-items: center;
  width: 12px;
  height: 16px;
  flex: 0 0 12px;
  color: var(--vn-text3);
  font-size: 11px;
  line-height: 1;
}
.vn-Tree__row:hover .vn-Tree__chevron,
.vn-Tree__row.is-expanded .vn-Tree__chevron {
  color: var(--vn-text1);
}

/* ---------- Drag and drop: explorer ---------- */
/* Source row while it is being dragged. */
.vn-Tree__row.is-dragging {
  opacity: 0.5;
}
/* Folder (or root row) that is a valid drop target under the cursor. */
.vn-Tree__row.is-drop-target {
  background: var(--vn-brand3);
  box-shadow: inset 0 0 0 1px var(--vn-brand1);
}
.vn-Tree__row.is-drop-target .vn-Tree__icon {
  fill: var(--vn-brand0);
}
/* Whole tree highlighted when dropping onto empty space (root target). */
.vn-Tree.is-root-drop-target {
  box-shadow: inset 0 0 0 2px var(--vn-brand2);
  border-radius: 6px;
}

/* ---------- Inline create / rename input (simple VS Code style) ---------- */
.vn-Tree__row--draft,
.vn-Tree__row--rename {
  position: relative;
  cursor: default;
  background: transparent;
}

.vn-Tree__row--draft:hover,
.vn-Tree__row--rename:hover {
  background: transparent;
}

.vn-Tree__row--draft .vn-Tree__icon,
.vn-Tree__row--rename .vn-Tree__icon {
  color: var(--vn-brand0);
  fill: currentColor;
}

.vn-Tree__inputWrap {
  position: relative;
  flex: 1 1 auto;
  min-width: 0;
  display: flex;
  align-items: center;
}

.vn-Tree__input {
  display: block;
  width: 100%;
  min-width: 0;
  height: 24px;
  padding: 2px 7px;
  border: 1px solid var(--vn-accent1);
  border-radius: 3px;
  background: var(--vn-color1);
  color: var(--vn-text1);
  font-size: 12.5px;
  font-family: var(--vn-ui-font);
  line-height: 18px;
  outline: none;
  box-shadow: 0 0 0 1px rgba(25, 118, 210, 0.12);
}

.vn-Tree__input::placeholder {
  color: var(--vn-text3);
}

.vn-Tree__input:focus {
  border-color: var(--vn-accent1);
  box-shadow:
    0 0 0 1px var(--vn-accent1),
    0 2px 8px rgba(0, 0, 0, 0.08);
}

.vn-Tree__input.has-error {
  border-color: var(--vn-problem-error);
  box-shadow: 0 0 0 1px rgba(198, 40, 40, 0.16);
}

.vn-Tree__input.has-error:focus {
  border-color: var(--vn-problem-error);
  box-shadow:
    0 0 0 1px var(--vn-problem-error),
    0 2px 8px rgba(198, 40, 40, 0.12);
}

.vn-Tree__inputHint {
  position: absolute;
  right: 7px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--vn-text3);
  font-size: 10.5px;
  pointer-events: none;
  opacity: 0;
  transition: opacity 120ms ease;
}

.vn-Tree__input:focus + .vn-Tree__inputHint {
  opacity: 0.8;
}

.vn-Tree__input:not(:placeholder-shown) + .vn-Tree__inputHint {
  display: none;
}

.vn-Tree__inputError {
  position: absolute;
  left: 0;
  right: 0;
  top: 28px;
  z-index: 20;
  padding: 5px 7px;
  border-radius: 5px;
  background: var(--vn-problem-error-bg);
  border: 1px solid #f5c6cb;
  color: var(--vn-problem-error);
  font-size: 11px;
  line-height: 1.35;
  white-space: normal;
  box-shadow: 0 8px 18px rgba(0, 0, 0, 0.12);
}

/* ---------- Custom cell type dropdown ---------- */
.vn-CellTypeSelect {
  position: relative;
  flex: 0 0 auto;
}

.vn-CellTypeSelect__button {
  display: inline-flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  min-width: 112px;
  height: 32px;
  padding: 0 10px 0 12px;
  border: 1px solid var(--vn-border1);
  border-radius: 7px;
  background: var(--vn-color1);
  color: var(--vn-text1);
  cursor: pointer;
  font-size: 13px;
  font-weight: 600;
}

.vn-CellTypeSelect__button:hover {
  background: var(--vn-color2);
  border-color: var(--vn-brand2);
}

.vn-CellTypeSelect.is-open .vn-CellTypeSelect__button,
.vn-CellTypeSelect__button:focus-visible {
  outline: none;
  border-color: var(--vn-brand1);
  box-shadow: 0 0 0 2px rgba(243, 119, 38, 0.16);
}

.vn-CellTypeSelect__chevron {
  width: 16px;
  height: 16px;
  fill: var(--vn-text2);
  transition: transform 120ms ease;
}

.vn-CellTypeSelect.is-open .vn-CellTypeSelect__chevron {
  transform: rotate(180deg);
}

.vn-CellTypeSelect__menu {
  position: absolute;
  top: calc(100% + 6px);
  left: 0;
  z-index: 80;
  min-width: 190px;
  padding: 5px;
  border: 1px solid var(--vn-border1);
  border-radius: 9px;
  background: var(--vn-color1);
  box-shadow: 0 12px 28px rgba(0, 0, 0, 0.16);
}

.vn-CellTypeSelect__menu[hidden] {
  display: none;
}

.vn-CellTypeSelect__option {
  display: flex;
  flex-direction: column;
  gap: 2px;
  width: 100%;
  padding: 8px 10px;
  border: 0;
  border-radius: 6px;
  background: transparent;
  color: var(--vn-text1);
  text-align: left;
  cursor: pointer;
}

.vn-CellTypeSelect__option:hover {
  background: var(--vn-color3);
}

.vn-CellTypeSelect__option.is-active {
  background: var(--vn-brand3);
  color: var(--vn-brand0);
}

.vn-CellTypeSelect__optionMain {
  font-size: 12.5px;
  font-weight: 700;
}

.vn-CellTypeSelect__optionHint {
  font-size: 11px;
  color: var(--vn-text2);
}

.vn-CellTypeSelect__option.is-active .vn-CellTypeSelect__optionHint {
  color: var(--vn-brand0);
}

/* ============================================================
   PROBLEMS / DIAGNOSTICS PANEL
   ============================================================ */
.vn-Problems__summary {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 0 8px 6px;
  padding: 7px 10px;
  border-radius: 6px;
  background: var(--vn-color1);
  border: 1px solid var(--vn-border2);
  font-size: 12px;
  color: var(--vn-text2);
  flex: 0 0 auto;
}
.vn-Problems__dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: var(--vn-text3);
  flex: 0 0 auto;
}
.vn-Problems__summary[data-state="running"] .vn-Problems__dot {
  background: var(--vn-brand1);
  animation: vn-pulse 1s ease-in-out infinite;
}
.vn-Problems__summary[data-state="success"] .vn-Problems__dot {
  background: var(--vn-problem-ok);
}
.vn-Problems__summary[data-state="failed"] .vn-Problems__dot {
  background: var(--vn-problem-error);
}
.vn-Problems__summary[data-state="failed"] .vn-Problems__summaryText {
  color: var(--vn-problem-error);
  font-weight: 600;
}
.vn-Problems__summary[data-state="success"] .vn-Problems__summaryText {
  color: var(--vn-problem-ok);
}

.vn-Problems {
  flex: 1 1 auto;
  overflow-y: auto;
  padding: 0 6px 12px;
}
.vn-Problems__loading {
  display: flex;
  align-items: center;
  gap: 9px;
  padding: 16px 10px;
  color: var(--vn-text2);
  font-size: 12.5px;
}

.vn-Problems__group {
  margin-bottom: 6px;
}
.vn-Problems__group.is-stale {
  opacity: 0.55;
}
.vn-Problems__groupHead {
  display: flex;
  align-items: center;
  gap: 7px;
  width: 100%;
  border: 0;
  background: transparent;
  cursor: pointer;
  padding: 4px 6px;
  border-radius: 5px;
  color: var(--vn-text2);
  text-align: left;
}
.vn-Problems__groupHead:hover {
  background: var(--vn-color3);
  color: var(--vn-text1);
}
.vn-Problems__cellIcon {
  width: 14px;
  height: 14px;
  flex: 0 0 auto;
}
.vn-Problems__cellName {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 11.5px;
  font-weight: 700;
  letter-spacing: 0.02em;
  text-transform: uppercase;
}
.vn-Problems__cellCount {
  flex: 0 0 auto;
  min-width: 16px;
  height: 15px;
  padding: 0 5px;
  border-radius: 8px;
  background: var(--vn-color3);
  color: var(--vn-text2);
  font-size: 10px;
  font-weight: 700;
  line-height: 15px;
  text-align: center;
}

.vn-Problem {
  display: flex;
  align-items: flex-start;
  gap: 8px;
  width: 100%;
  border: 0;
  background: transparent;
  cursor: pointer;
  text-align: left;
  padding: 5px 8px 5px 22px;
  border-radius: 5px;
  color: var(--vn-text1);
}
.vn-Problem:hover {
  background: var(--vn-color3);
}
.vn-Problem__icon {
  width: 15px;
  height: 15px;
  flex: 0 0 auto;
  margin-top: 1px;
}
.vn-Problem--error .vn-Problem__icon {
  color: var(--vn-problem-error);
}
.vn-Problem--hint .vn-Problem__icon {
  color: var(--vn-problem-hint);
}
.vn-Problem__body {
  display: flex;
  flex-direction: column;
  gap: 1px;
  min-width: 0;
}
.vn-Problem__message {
  font-size: 12px;
  line-height: 1.4;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.vn-Problem__kind {
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 0.05em;
  text-transform: uppercase;
  color: var(--vn-text3);
}
.vn-Problem--error .vn-Problem__kind {
  color: var(--vn-problem-error);
}
.vn-Problem--hint .vn-Problem__kind {
  color: var(--vn-problem-hint);
}

/* ============================================================
   EDITOR COLUMN
   ============================================================ */
.vn-Editor-col {
  position: relative;
  flex: 1 1 auto;
  min-width: 0;
  min-height: 0;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  background: var(--vn-color0);
}

/* ---------- Center tabs bar ---------- */
.vn-TabsBar {
  display: flex;
  align-items: stretch;
  height: var(--vn-tabsbar-h);
  background: var(--vn-tabsbar-bg);
  border-bottom: 1px solid var(--vn-border2);
  flex: 0 0 auto;
  overflow-x: auto;
  scrollbar-width: none;
}
.vn-TabsBar::-webkit-scrollbar {
  display: none;
}
.vn-TabsBar__empty {
  display: flex;
  align-items: center;
  padding: 0 14px;
  font-size: 12px;
  color: var(--vn-text3);
}
.vn-Tab {
  position: relative;
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 0 8px 0 14px;
  border-right: 1px solid var(--vn-border2);
  background: transparent;
  cursor: pointer;
  max-width: 220px;
  flex: 0 0 auto;
  color: var(--vn-text2);
}
.vn-Tab:hover {
  background: var(--vn-color3);
}
.vn-Tab.is-active {
  background: var(--vn-color1);
  color: var(--vn-text1);
  box-shadow: inset 0 -2px 0 var(--vn-brand1);
}
.vn-Tab__label {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 12.5px;
}
.vn-Tab__dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--vn-brand1);
  flex: 0 0 auto;
}
.vn-Tab__close {
  width: 20px;
  height: 20px;
  border: 0;
  border-radius: 4px;
  background: transparent;
  color: var(--vn-text3);
  cursor: pointer;
  font-size: 16px;
  line-height: 1;
  display: grid;
  place-items: center;
  flex: 0 0 auto;
  opacity: 0;
}
.vn-Tab:hover .vn-Tab__close,
.vn-Tab.is-active .vn-Tab__close {
  opacity: 1;
}
.vn-Tab__close:hover {
  background: var(--vn-color4);
  color: var(--vn-text1);
}

/* ---------- Drag and drop: tabs ---------- */
/* Source tab while it is being dragged. */
.vn-Tab.is-dragging {
  opacity: 0.5;
}
/* Insertion markers (drop before / after the hovered tab). */
.vn-Tab.is-drop-before::before,
.vn-Tab.is-drop-after::after {
  content: "";
  position: absolute;
  top: 4px;
  bottom: 4px;
  width: 2px;
)VIXNOTE");

    value.append(R"VIXNOTE(  background: var(--vn-brand1);
  border-radius: 2px;
}
.vn-Tab.is-drop-before::before {
  left: 0;
}
.vn-Tab.is-drop-after::after {
  right: 0;
}

/* ---------- Notebook toolbar ---------- */
.vn-Toolbar {
  position: relative;
  z-index: 40;
  display: flex;
  align-items: center;
  gap: 1px;
  height: var(--vn-toolbar-h);
  padding: 0 6px;
  background: var(--vn-toolbar-bg);
  border-bottom: 1px solid var(--vn-border2);
  flex: 0 0 auto;
  overflow: visible;
  width: 100%;
  max-width: 100%;
}
.vn-Toolbar::-webkit-scrollbar {
  display: none;
}

.vn-CellTypeSelect {
  position: relative;
  z-index: 120;
  flex: 0 0 auto;
}

.vn-CellTypeSelect__menu {
  position: absolute;
  top: calc(100% + 6px);
  left: 0;
  z-index: 1000;
  min-width: 190px;
}
.vn-ToolbarButton {
  display: grid;
  place-items: center;
  width: 30px;
  height: 30px;
  border: 0;
  border-radius: 5px;
  background: transparent;
  cursor: pointer;
  flex: 0 0 auto;
}
.vn-ToolbarButton:hover {
  background: var(--vn-color3);
}
.vn-ToolbarButton:active {
  background: var(--vn-color4);
}
.vn-ToolbarButton:disabled {
  opacity: 0.4;
  cursor: default;
  background: transparent;
}
.vn-Toolbar__sep {
  width: 1px;
  height: 20px;
  margin: 0 5px;
  background: var(--vn-border2);
  flex: 0 0 auto;
}

.vn-Toolbar__grow {
  flex: 1 1 auto;
}
.vn-Toolbar__title {
  color: var(--vn-text1);
  font-size: 12.5px;
  font-weight: 600;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 240px;
}
.vn-Toolbar__filename {
  color: var(--vn-text3);
  font-size: 12px;
  padding: 0 4px;
  white-space: nowrap;
}

/* ---------- Notebook panel ---------- */
.vn-NotebookPanel {
  flex: 1 1 auto;
  min-width: 0;
  min-height: 0;
  background: var(--vn-color2);
  overflow-y: auto;
  display: flex;
  flex-direction: column;
}

.vn-Notice {
  margin: 8px 12px 0;
  padding: 8px 12px;
  border-radius: 6px;
  font-size: 12.5px;
  border: 1px solid transparent;
}
.vn-Notice--success {
  background: #e8f5e9;
  border-color: #c8e6c9;
  color: #2e7d32;
}
.vn-Notice--error {
  background: #fdecea;
  border-color: #f5c6cb;
  color: #c62828;
}
.vn-Notice--warning {
  background: #fff8e1;
  border-color: #ffe082;
  color: #f57f17;
}
.vn-Notice--info {
  background: #e3f2fd;
  border-color: #bbdefb;
  color: #1565c0;
}

.vn-Notebook {
  flex: 1 1 auto;
  max-width: 1100px;
  width: 100%;
  margin: 0 auto;
  padding: 16px 16px 240px;
  outline: none;
}
.vn.is-focus .vn-Notebook {
  max-width: 760px;
}

.vn-Notebook__loading {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 40px;
  color: var(--vn-text2);
  font-size: 13px;
}
.vn-spinner {
  width: 16px;
  height: 16px;
  border: 2px solid var(--vn-border1);
  border-top-color: var(--vn-brand1);
  border-radius: 50%;
  animation: vn-spin 0.7s linear infinite;
}
@keyframes vn-spin {
  to {
    transform: rotate(360deg);
  }
}
.vn-Notebook__empty {
  margin: 30px 10px;
  padding: 40px;
  border: 1px dashed var(--vn-border1);
  border-radius: 8px;
  text-align: center;
  color: var(--vn-text2);
  background: var(--vn-color1);
}

/* ============================================================
   CELLS
   ============================================================ */
.vn-Cell {
  position: relative;
  display: flex;
  padding: 4px 0;
}
.vn-Cell__collapser {
  flex: 0 0 var(--vn-collapser-width);
  width: var(--vn-collapser-width);
  border-radius: 2px;
  background: transparent;
  cursor: pointer;
  transition: background 0.1s;
}
.vn-Cell.is-selected > .vn-Cell__collapser {
  background: var(--vn-collapser-selected);
}
.vn-Cell.is-editing > .vn-Cell__collapser {
  background: var(--vn-collapser-edit);
}
/* Cell with active error diagnostics: red rail accent. */
.vn-Cell.has-problem > .vn-Cell__collapser {
  background: var(--vn-problem-error);
}
.vn-Cell.is-selected .vn-Cell__body {
  box-shadow: inset 0 0 0 1px var(--vn-collapser-selected);
}
.vn-Cell.is-editing .vn-Cell__body {
  box-shadow: inset 0 0 0 1px var(--vn-collapser-edit);
}
.vn-Cell__body {
  flex: 1 1 auto;
  min-width: 0;
  border-radius: 3px;
}

/* Flash when navigated to from the Problems panel. */
.vn-Cell.is-flash .vn-Cell__body {
  animation: vn-cell-flash 0.9s ease-out;
}
@keyframes vn-cell-flash {
  0% {
    box-shadow: inset 0 0 0 2px var(--vn-brand1);
    background: var(--vn-brand3);
  }
  100% {
    box-shadow: inset 0 0 0 0 transparent;
    background: transparent;
  }
}

.vn-InputArea {
  display: flex;
  align-items: stretch;
}
.vn-InputPrompt {
  flex: 0 0 var(--vn-prompt-width);
  width: var(--vn-prompt-width);
  padding: 6px 8px 6px 0;
  text-align: right;
  color: var(--vn-inprompt);
  font-family: var(--vn-code-font);
  font-size: 12px;
  line-height: var(--vn-code-lh);
  user-select: none;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.vn-InputPrompt--empty {
  color: var(--vn-prompt-not-active);
}

.vn-Editor {
  flex: 1 1 auto;
  min-width: 0;
  border: 1px solid var(--vn-editor-border);
  border-radius: 5px;
  background: var(--vn-editor-bg);
  overflow: hidden;
}
.vn-Cell.is-editing .vn-Editor {
  background: var(--vn-editor-active-bg);
  border-color: var(--vn-brand2);
}
.vn-Editor__wrap {
  position: relative;
}
.vn-Editor__highlight,
.vn-Editor textarea {
  margin: 0;
  border: 0;
  padding: 6px 10px;
  font-family: var(--vn-code-font);
  font-size: var(--vn-code-size);
  line-height: var(--vn-code-lh);
  tab-size: 2;
  white-space: pre;
  word-wrap: normal;
}
.vn-Editor__highlight {
  position: absolute;
  inset: 0;
  pointer-events: none;
  overflow: hidden;
  color: var(--vn-punct);
  white-space: pre;
  z-index: 0;
}
.vn-Editor textarea {
  position: relative;
  display: block;
  width: 100%;
  min-height: 1.6em;
  resize: none;
  background: transparent;
  color: transparent;
  caret-color: #000;
  outline: none;
  overflow: hidden;
  white-space: pre;
  z-index: 2;
}
.vn-Editor--plain textarea {
  color: var(--vn-content1);
  caret-color: #000;
}
.vn-Editor--plain .vn-Editor__highlight {
  display: none;
}

.vn-Editor__lineFocus {
  position: absolute;
  left: 0;
  right: 0;
  top: 0;
  pointer-events: none;
  background: rgba(255, 255, 255, 0.055);
  border-left: 2px solid rgba(243, 119, 38, 0.72);
  opacity: 0;
  transition:
    opacity 120ms ease,
    transform 80ms ease;
  z-index: 1;
}
.vn-Cell.is-editing .vn-Editor__lineFocus {
  opacity: 1;
}

/* Output row */
.vn-OutputArea {
  display: flex;
  margin-top: 2px;
}
.vn-OutputArea.is-collapsed .vn-OutputArea__list {
  display: none;
}
.vn-OutputPrompt {
  flex: 0 0 var(--vn-prompt-width);
  width: var(--vn-prompt-width);
  padding: 6px 8px 6px 0;
  text-align: right;
  color: var(--vn-outprompt);
  font-family: var(--vn-code-font);
  font-size: 12px;
  line-height: var(--vn-code-lh);
  user-select: none;
}
.vn-OutputArea__list {
  flex: 1 1 auto;
  min-width: 0;
}
.vn-Output {
  padding: 4px 10px;
  font-family: var(--vn-code-font);
  font-size: var(--vn-code-size);
  line-height: var(--vn-code-lh);
}
.vn-Output pre {
  margin: 0;
  white-space: pre-wrap;
  word-break: break-word;
}
.vn-Output--stdout {
  color: var(--vn-content1);
}
.vn-Output--stderr {
  background: var(--vn-stderr-bg);
  color: #5d4037;
}
.vn-Output--error,
.vn-Output--compiler_error,
.vn-Output--runtime_error {
  background: var(--vn-error-bg);
  color: var(--vn-error);
}
.vn-Output--hint,
.vn-Output--info {
  color: var(--vn-accent1);
}
.vn-Output--debug,
.vn-Output--raw_log {
  color: var(--vn-text2);
}
.vn-Output--running {
  color: var(--vn-text2);
  font-style: italic;
}
.vn-Output__kind {
  display: inline-block;
  margin-right: 8px;
  padding: 0 5px;
  border-radius: 3px;
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  font-family: var(--vn-ui-font);
  color: var(--vn-text3);
  vertical-align: top;
}

/* Markdown / HTML rendered cells */
.vn-RenderedMarkdown {
  flex: 1 1 auto;
  min-width: 0;
  padding: 4px 10px 8px 0;
  color: var(--vn-content1);
  line-height: 1.6;
  cursor: text;
}
.vn-RenderedMarkdown h1,
.vn-RenderedMarkdown h2,
.vn-RenderedMarkdown h3 {
  margin: 0.4em 0 0.3em;
  font-weight: 600;
  line-height: 1.25;
}
.vn-RenderedMarkdown h1 {
  font-size: 1.8em;
  border-bottom: 1px solid var(--vn-border2);
  padding-bottom: 0.2em;
}
.vn-RenderedMarkdown h2 {
  font-size: 1.4em;
}
.vn-RenderedMarkdown h3 {
  font-size: 1.15em;
}
.vn-RenderedMarkdown p {
  margin: 0.5em 0;
}
.vn-RenderedMarkdown code {
  font-family: var(--vn-code-font);
  font-size: 0.9em;
  background: var(--vn-color2);
  padding: 0.1em 0.35em;
  border-radius: 4px;
}
.vn-RenderedMarkdown pre {
  background: var(--vn-color2);
  border: 1px solid var(--vn-border2);
  border-radius: 6px;
  padding: 10px 12px;
  overflow-x: auto;
}
.vn-RenderedMarkdown pre code {
  background: transparent;
  padding: 0;
}
.vn-RenderedHTML {
  flex: 1 1 auto;
  min-width: 0;
  padding: 4px 10px 8px 0;
  color: var(--vn-content1);
  line-height: 1.6;
}
.vn-Cell.is-editing .vn-RenderedMarkdown,
.vn-Cell.is-editing .vn-RenderedHTML {
  display: none;
}
.vn-Cell:not(.is-editing) .vn-MarkdownCell .vn-InputArea,
.vn-Cell:not(.is-editing) .vn-HtmlCell .vn-InputArea {
  display: none;
}

/* Syntax tokens */
.tok-kw {
  color: var(--vn-kw);
  font-weight: 600;
}
.tok-type {
  color: var(--vn-type);
}
.tok-str {
  color: var(--vn-str);
}
.tok-com {
  color: var(--vn-com);
  font-style: italic;
}
.tok-num {
  color: var(--vn-num);
}
.tok-pre {
  color: var(--vn-pre);
  font-weight: 600;
}
.tok-fn {
  color: var(--vn-fn);
}
.tok-punct {
  color: var(--vn-punct);
}

/* Inline "+" insert button between cells */
.vn-CellInsert {
  position: relative;
  height: 0;
  margin-left: var(--vn-collapser-width);
}
.vn-CellInsert__btn {
  position: absolute;
  left: 50%;
  top: -10px;
  transform: translateX(-50%);
  width: 22px;
  height: 18px;
  border: 1px solid var(--vn-border1);
  border-radius: 9px;
  background: var(--vn-color1);
  color: var(--vn-text2);
  font-size: 13px;
  line-height: 1;
  cursor: pointer;
  opacity: 0;
  transition: opacity 0.12s;
  display: grid;
  place-items: center;
  z-index: 2;
}
.vn-CellInsert:hover .vn-CellInsert__btn {
  opacity: 1;
}
.vn-CellInsert__btn:hover {
  border-color: var(--vn-brand1);
  color: var(--vn-brand0);
}

/* Cell hover toolbar */
.vn-Cell__toolbar {
  position: absolute;
  top: 2px;
  right: 4px;
  display: flex;
  gap: 1px;
  background: var(--vn-color1);
  border: 1px solid var(--vn-border2);
  border-radius: 6px;
  padding: 1px;
  opacity: 0;
  transition: opacity 0.12s;
  z-index: 3;
}
.vn-Cell:hover .vn-Cell__toolbar,
.vn-Cell.is-selected .vn-Cell__toolbar {
  opacity: 1;
}
.vn-Cell__toolbar button {
  display: grid;
  place-items: center;
  width: 24px;
  height: 22px;
  border: 0;
  background: transparent;
  border-radius: 4px;
  cursor: pointer;
}
.vn-Cell__toolbar button:hover {
  background: var(--vn-color3);
}
.vn-Cell__toolbar svg {
  width: 15px;
  height: 15px;
  fill: var(--vn-text2);
}
.vn-Cell.is-running .vn-InputPrompt {
  color: var(--vn-outprompt);
}
.vn-Cell.is-running .vn-InputPrompt::after {
  content: " *";
}

/* ============================================================
   STATUS BAR — fixed-width slots, no reflow on kernel change
   ============================================================ */
.vn-StatusBar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: var(--vn-statusbar-h);
  padding: 0 12px;
  background: var(--vn-appbar-bg);
  border-top: 1px solid var(--vn-border2);
  flex: 0 0 auto;
  font-size: 11.5px;
  color: var(--vn-text2);
}
.vn-StatusBar__left,
.vn-StatusBar__right {
  display: flex;
  align-items: center;
  gap: 8px;
}
.vn-StatusBar__sep {
  color: var(--vn-text3);
}
.vn-StatusBar__mode {
  font-weight: 600;
  color: var(--vn-text1);
  display: inline-block;
  min-width: 56px;
}
.vn-StatusBar__pos {
  display: inline-block;
  min-width: 78px;
}
.vn-StatusBar__kind {
)VIXNOTE");

    value.append(R"VIXNOTE(  display: inline-block;
  min-width: 56px;
  text-align: right;
}
.vn-StatusBar__kernel {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}
.vn-StatusBar__kernelText {
  display: inline-block;
  min-width: var(--vn-kernel-slot);
}
.vn-StatusBar__dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  border: 1.5px solid var(--vn-text3);
  background: transparent;
  flex: 0 0 auto;
}

/* Status-bar Problems indicator */
.vn-StatusBar__problems {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  border: 0;
  background: transparent;
  color: var(--vn-text2);
  cursor: pointer;
  padding: 0 4px;
  height: 18px;
  border-radius: 4px;
  font-size: 11.5px;
}
.vn-StatusBar__problems:hover {
  background: var(--vn-color3);
  color: var(--vn-text1);
}
.vn-StatusBar__problems.has-errors {
  color: var(--vn-problem-error);
}
.vn-StatusBar__problemsIcon {
  font-size: 11px;
  line-height: 1;
}

/* ============================================================
   MODAL (forms / confirm / info)
   ============================================================ */
.vn-Modal {
  position: fixed;
  inset: 0;
  z-index: 100;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
}
.vn-Modal[hidden] {
  display: none;
}
.vn-Modal__backdrop {
  position: absolute;
  inset: 0;
  background: rgba(0, 0, 0, 0.28);
}
.vn-Modal__box {
  position: relative;
  width: min(480px, 100%);
  max-height: 82vh;
  background: var(--vn-color1);
  border: 1px solid var(--vn-border1);
  border-radius: 10px;
  box-shadow: 0 18px 50px rgba(0, 0, 0, 0.3);
  display: flex;
  flex-direction: column;
  overflow: hidden;
}
.vn-Modal__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 18px;
  border-bottom: 1px solid var(--vn-border2);
}
.vn-Modal__title {
  margin: 0;
  font-size: 15px;
  font-weight: 600;
}
.vn-Modal__close {
  border: 0;
  background: transparent;
  font-size: 22px;
  line-height: 1;
  color: var(--vn-text2);
  cursor: pointer;
}
.vn-Modal__close:hover {
  color: var(--vn-text0);
}
.vn-Modal__body {
  padding: 16px 18px;
  overflow-y: auto;
  font-size: 13px;
  line-height: 1.6;
  color: var(--vn-text1);
}
.vn-Modal__text {
  margin: 0;
  color: var(--vn-text2);
}
.vn-Modal__foot {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  padding: 12px 18px 16px;
}
.vn-Modal__foot:empty {
  display: none;
}

/* Form fields inside modal */
.vn-Form {
  display: flex;
  flex-direction: column;
  gap: 14px;
}
.vn-Form__field {
  display: flex;
  flex-direction: column;
  gap: 5px;
}
.vn-Form__label {
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 0.05em;
  text-transform: uppercase;
  color: var(--vn-text3);
}
.vn-Form__input {
  height: 36px;
  padding: 0 12px;
  border: 1px solid var(--vn-border1);
  border-radius: 7px;
  background: var(--vn-color1);
  color: var(--vn-text1);
  font-size: 13px;
  font-family: var(--vn-code-font);
  outline: none;
}
.vn-Form__input:focus {
  border-color: var(--vn-brand2);
  box-shadow: 0 0 0 3px var(--vn-brand3);
}
.vn-Form__hint {
  font-size: 11.5px;
  color: var(--vn-text3);
}

/* Buttons */
.vn-Btn {
  height: 34px;
  padding: 0 16px;
  border: 1px solid var(--vn-border1);
  border-radius: 7px;
  background: var(--vn-color1);
  color: var(--vn-text1);
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
}
.vn-Btn--ghost:hover {
  background: var(--vn-color2);
}
.vn-Btn--primary {
  border-color: var(--vn-brand1);
  background: var(--vn-brand1);
  color: #fff;
}
.vn-Btn--primary:hover {
  background: var(--vn-brand0);
  border-color: var(--vn-brand0);
}
.vn-Btn--danger {
  border-color: #d32f2f;
  background: #d32f2f;
  color: #fff;
}
.vn-Btn--danger:hover {
  background: #b71c1c;
  border-color: #b71c1c;
}

/* Shortcuts grid */
.vn-Modal__body kbd {
  display: inline-block;
  min-width: 18px;
  padding: 1px 6px;
  border: 1px solid var(--vn-border1);
  border-bottom-width: 2px;
  border-radius: 5px;
  background: var(--vn-color2);
  font-family: var(--vn-code-font);
  font-size: 11px;
  text-align: center;
}
.vn-Shortcuts {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 8px 18px;
  align-items: center;
}
.vn-Shortcuts__group {
  grid-column: 1 / -1;
  margin-top: 8px;
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--vn-text3);
}

/* ============================================================
   CONTEXT MENU (custom, replaces native menu)
   ============================================================ */
.vn-Context {
  position: fixed;
  z-index: 200;
  min-width: 180px;
  padding: 5px;
  background: var(--vn-color1);
  border: 1px solid var(--vn-border1);
  border-radius: 8px;
  box-shadow: 0 10px 28px rgba(0, 0, 0, 0.2);
}
.vn-Context__item {
  display: block;
  width: 100%;
  border: 0;
  background: transparent;
  text-align: left;
  padding: 7px 10px;
  border-radius: 5px;
  cursor: pointer;
  color: var(--vn-text1);
  font-size: 12.5px;
}
.vn-Context__item:hover {
  background: var(--vn-accent1);
  color: #fff;
}
.vn-Context__item.is-danger {
  color: #c62828;
}
.vn-Context__item.is-danger:hover {
  background: #d32f2f;
  color: #fff;
}
.vn-Context__item.is-disabled {
  color: var(--vn-text3);
  cursor: default;
}
.vn-Context__item.is-disabled:hover {
  background: transparent;
  color: var(--vn-text3);
}
.vn-Context__sep {
  height: 1px;
  margin: 4px 6px;
  background: var(--vn-border2);
}

/* ============================================================
   SCROLLBARS
   ============================================================ */
.vn-Tree::-webkit-scrollbar,
.vn-Problems::-webkit-scrollbar,
.vn-NotebookPanel::-webkit-scrollbar,
.vn-Modal__body::-webkit-scrollbar {
  width: 10px;
  height: 10px;
}
.vn-Tree::-webkit-scrollbar-thumb,
.vn-Problems::-webkit-scrollbar-thumb,
.vn-NotebookPanel::-webkit-scrollbar-thumb,
.vn-Modal__body::-webkit-scrollbar-thumb {
  background: #c4c4c4;
  border: 2px solid var(--vn-color2);
  border-radius: 6px;
}
.vn-Tree::-webkit-scrollbar-thumb:hover,
.vn-Problems::-webkit-scrollbar-thumb:hover,
.vn-NotebookPanel::-webkit-scrollbar-thumb:hover {
  background: #a8a8a8;
}

/* ============================================================
   REDUCED MOTION
   ============================================================ */
@media (prefers-reduced-motion: reduce) {
  .vn-Cell.is-flash .vn-Cell__body,
  .vn-spinner,
  .vn[data-kernel="busy"] .vn-KernelStatus__dot,
  .vn[data-kernel="busy"] .vn-StatusBar__dot,
  .vn-Problems__summary[data-state="running"] .vn-Problems__dot {
    animation: none;
  }
}
/* ---------- About modal ---------- */
.vn-About {
  max-width: 560px;
  color: var(--vn-text1);
  line-height: 1.55;
}

.vn-About__hero {
  display: flex;
  align-items: center;
  gap: 14px;
  margin-bottom: 14px;
}

.vn-About__logo {
  display: grid;
  place-items: center;
  width: 44px;
  height: 44px;
  border-radius: 12px;
  background: var(--vn-brand1);
  color: #fff;
  font-size: 22px;
  font-weight: 800;
  flex: 0 0 auto;
}

.vn-About h2 {
  margin: 0;
  font-size: 20px;
  line-height: 1.2;
}

.vn-About h2 + p {
  margin: 4px 0 0;
  color: var(--vn-text2);
}

.vn-About__section {
  margin-top: 16px;
}

.vn-About__section h3 {
  margin: 0 0 7px;
  font-size: 12px;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: var(--vn-text2);
}

.vn-About ul {
  margin: 0;
  padding-left: 18px;
}

.vn-About li {
  margin: 4px 0;
}

.vn-About code {
  padding: 1px 5px;
  border-radius: 4px;
  background: var(--vn-color2);
  border: 1px solid var(--vn-border2);
  font-family: var(--vn-code-font);
  font-size: 12px;
}

.vn-About__meta {
  margin: 18px 0 0;
  padding-top: 12px;
  border-top: 1px solid var(--vn-border2);
  color: var(--vn-text3);
  font-size: 12px;
}
/* ============================================================
   RESPONSIVE
   ============================================================ */
@media (max-width: 620px) {
  .vn-Sidebar {
    position: absolute;
    top: 0;
    left: var(--vn-activity-w);
    height: 100%;
    z-index: 30;
    width: min(var(--vn-sidebar-w), 72vw) !important;
    flex-basis: auto;
    box-shadow: 4px 0 18px rgba(0, 0, 0, 0.18);
    transform: translateX(0);
    transition: transform 0.18s ease;
  }
  .vn.is-sidebar-collapsed .vn-Sidebar {
    transform: translateX(-120%);
    width: min(var(--vn-sidebar-w), 72vw) !important;
    border-right: 1px solid var(--vn-border2);
  }
  .vn-Sidebar__resizer {
    display: none;
  }
}

@media (max-width: 620px) {
  :root {
    --vn-prompt-width: 46px;
    --vn-sidebar-w: 240px;
    --vn-activity-w: 44px;
  }
  .vn-AppBar__menus {
    display: none;
  }
  .vn-KernelName {
    display: none;
  }
  .vn-Notebook {
    padding: 12px 8px 200px;
  }
  .vn-Toolbar__title,
  .vn-Toolbar__filename {
    display: none;
  }
  .vn-StatusBar__pos,
  .vn-StatusBar__cells {
    display: none;
  }
}

@media (max-width: 420px) {
  .vn-AppBar__title {
    display: none;
  }
  .vn-StatusBar__kind {
    display: none;
  }
}
)VIXNOTE");

    return value;
  }

  std::string NoteAssets::default_js()
  {
    std::string value;
    value.reserve(155060);

    value.append(R"VIXNOTE(/*
 * Vix Note — notebook frontend
 *
 * Lightweight editor experience over the existing Vix Note HTTP API:
 *   GET    /api/document
 *   POST   /api/cells                 { kind, source, index? }
 *   PUT    /api/cells/<id>            { kind, source }
 *   DELETE /api/cells/<id>
 *   POST   /api/cells/<id>/run
 *   POST   /api/cells/<id>/move       { index }
 *   POST   /api/run-all
 *   POST   /api/document/save
 *   POST   /api/document/new          { path, title }
 *   POST   /api/document/open         { path }
 *   POST   /api/directory/create      { path }
 *   POST   /api/directory/list        { path }
 *   POST   /api/path/delete           { path, recursive }
 *   POST   /api/path/rename           { path, newName }
 *   POST   /api/path/move             { path, directory }
 *
 * No framework, no external dependency, vanilla JS only.
 *
 * Copyright 2026, Gaspard Kirira. MIT License.
 */
(() => {
  "use strict";

  /* ==========================================================
   * State
   * ======================================================== */
  const state = {
    document: null,
    selectedId: null,
    editing: false,
    kernel: "idle",
    busy: false,
    sidebarCollapsed: false,
    sidebarWidth: 260,
    focusMode: false,
    activePanel: "explorer", // explorer | problems
    explorer: {
      rootPath: ".",
      currentPath: ".",
      selectedDirPath: ".",
      loadingPath: null,
      entries: new Map(),
      loadedDirs: new Set(),
      expandedDirs: new Set(["."]),
      draft: null,
    },
    tabs: [], // [{ path, title, dirty, doc? }]
    activeTabPath: null,

    diagnostics: {
      status: "idle",
      items: [],
      byCell: new Map(),
    },

    // Drag and drop. Explorer and tab drags never mix.
    drag: {
      explorer: null, // { path, type }
      tab: null, // { path, fromIndex }
    },
  };

  const DEFAULT_SIDEBAR_WIDTH = 260;
  const MIN_SIDEBAR_WIDTH = 190;
  const MAX_SIDEBAR_WIDTH = 520;
  const TABS_STORAGE_KEY = "vix-note:tabs:v1";

  const app = document.querySelector("[data-note-app]");

  const sel = {
    cells: "[data-note-cells]",
    project: "[data-note-project]",
    cellCount: "[data-note-cell-count]",
    execCount: "[data-note-exec-count]",
    kernel: "[data-note-kernel]",
    message: "[data-note-message]",
    toolbarKind: '[data-action="toolbar-kind"]',
    statusMode: "[data-status-mode]",
    statusPosition: "[data-status-position]",
    statusKind: "[data-status-kind]",
    statusKernel: "[data-status-kernel]",
    sidebar: "[data-sidebar]",
    sidebarResizer: "[data-sidebar-resizer]",
    explorerList: "[data-explorer-list]",
    explorerCount: "[data-explorer-count]",
    explorerSearch: "[data-explorer-search]",
    tabsBar: "[data-tabs-bar]",
    problemsList: "[data-problems-list]",
    problemsCount: "[data-problems-count]",
    problemsSummary: "[data-problems-summary]",
    problemsSummaryText: "[data-problems-summary-text]",
    problemsBadge: "[data-activity-problems-badge]",
    statusProblems: "[data-status-problems]",
    statusProblemsCount: "[data-status-problems-count]",
  };

  const $ = (s, root = document) => root.querySelector(s);
  const $all = (s, root = document) => Array.from(root.querySelectorAll(s));

  /* ==========================================================
   * Helpers
   * ======================================================== */
  function escapeHtml(value) {
    return String(value ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function normalizeKind(kind) {
    return String(kind || "unknown").toLowerCase();
  }

  function safeClass(value) {
    return String(value || "unknown")
      .toLowerCase()
      .replace(/[^a-z0-9_-]/g, "-");
  }

  function isCodeKind(kind) {
    const k = normalizeKind(kind);
    return k === "cpp" || k === "reply";
  }

  function pad2(n) {
    return String(n).padStart(2, "0");
  }

  function suggestedNoteName() {
    const d = new Date();
    const stamp =
      `${d.getFullYear()}${pad2(d.getMonth() + 1)}${pad2(d.getDate())}` +
      `_${pad2(d.getHours())}${pad2(d.getMinutes())}${pad2(d.getSeconds())}`;
    return `note_${stamp}.vixnote`;
  }

  function baseName(path) {
    return (
      String(path || "")
        .split(/[\\/]/)
        .pop() || String(path || "")
    );
  }

  function stripExtension(name) {
    const base = baseName(name);
    const dot = base.lastIndexOf(".");
    return dot > 0 ? base.slice(0, dot) : base;
  }

  function titleFromFileName(name) {
    const stem = stripExtension(name).replace(/[_-]+/g, " ").trim();
    if (!stem) return "New Note";
    return stem.replace(/\b\w/g, (c) => c.toUpperCase());
  }

  function noteTitleFromPath(path) {
    return titleFromFileName(baseName(path));
  }

  function defaultIntroSource(title) {
    return `# ${title}\n\nStart writing your lesson here.`;
  }

  function isDefaultIntroSource(source, title) {
    const text = String(source || "").trim();

    return (
      text === `# ${title}\n\nStart writing your lesson here.` ||
      text === `# ${title}\n\nStart writing your note here.`
    );
  }

  async function retitleActiveDocumentAfterRename(oldPath, newPath, type) {
    if (type !== "file") {
      return;
    }

    if (!String(newPath || "").endsWith(".vixnote")) {
      return;
    }

    if (!state.document) {
      return;
    }

    if (
      normalizeExplorerPath(state.document.path) !==
      normalizeExplorerPath(oldPath)
    ) {
      return;
    }

    const oldTitle = noteTitleFromPath(oldPath);
    const newTitle = noteTitleFromPath(newPath);
    const currentTitle = String(state.document.title || "").trim();

    if (currentTitle && currentTitle !== oldTitle) {
      state.document.path = newPath;
      document.title = `${currentTitle} · Vix Note`;
      return;
    }

    state.document.path = newPath;
    state.document.title = newTitle;

    document.title = `${newTitle} · Vix Note`;

    const tab = activeTab();
    if (tab) {
      tab.title = newTitle;
    }

    const first = cells()[0];

    if (
      first &&
      normalizeKind(first.kind) === "markdown" &&
      isDefaultIntroSource(first.source, oldTitle)
    ) {
      first.source = defaultIntroSource(newTitle);

      await api(`/api/cells/${encodeURIComponent(first.id)}`, {
        method: "PUT",
        body: JSON.stringify({
          kind: first.kind,
          source: first.source,
        }),
      });
    }

    await api("/api/document/update", {
      method: "POST",
      body: JSON.stringify({
        title: newTitle,
        save: true,
      }),
    });

    renderDocument(
      {
        ok: true,
        document: state.document,
      },
      {
        fullRepaint: true,
      },
    );

    persistTabs();
  }

  function kindLabel(kind) {
    switch (normalizeKind(kind)) {
      case "markdown":
        return "Markdown";
      case "reply":
        return "Reply";
      case "cpp":
        return "C++";
      case "html":
        return "HTML";
      default:
        return "Code";
    }
  }

  function relativeTime(epochMs) {
    if (!epochMs) return "";
    const diff = Date.now() - epochMs;
    const sec = Math.round(diff / 1000);
    if (sec < 45) return "just now";
    const min = Math.round(sec / 60);
    if (min < 60) return min === 1 ? "1 min ago" : `${min} min ago`;
    const hr = Math.round(min / 60);
    if (hr < 24) return hr === 1 ? "1 hour ago" : `${hr} hours ago`;
    const day = Math.round(hr / 24);
    if (day === 1) return "Yesterday";
    if (day < 7) return `${day} days ago`;
    const wk = Math.round(day / 7);
    if (wk < 5) return wk === 1 ? "1 week ago" : `${wk} weeks ago`;
    return new Date(epochMs).toLocaleDateString();
  }

  function setText(s, value) {
    const el = $(s);
    if (el) el.textContent = value;
  }

  function setKernel(status) {
    state.kernel = status;
    app.setAttribute("data-kernel", status);
    const label =
      status === "busy" ? "Busy" : status === "error" ? "Error" : "Idle";
    setText(sel.kernel, label);
    setText(sel.statusKernel, label);
  }

  let messageTimer = null;
  function setMessage(message, kind = "info") {
    const el = $(sel.message);
    if (!el) return;
    clearTimeout(messageTimer);
    if (!message) {
      el.hidden = true;
      el.textContent = "";
      el.className = "vn-Notice";
      return;
    }
    el.hidden = false;
    el.textContent = message;
    el.className = `vn-Notice vn-Notice--${safeClass(kind)}`;
    if (kind === "success" || kind === "info") {
      messageTimer = setTimeout(() => setMessage(""), 2600);
    }
  }

  function clearMessageQuietly() {
    setMessage("");
  }

  function setBusy(busy) {
    state.busy = busy;
    for (const b of $all(".vn-ToolbarButton")) b.disabled = busy;
  }

  /* ==========================================================
   * API layer
   * ======================================================== */
  async function api(path, options = {}, config = {}) {
    const headers = {
      Accept: "application/json",
      ...(options.body ? { "Content-Type": "application/json" } : {}),
      ...(options.headers || {}),
    };
    const response = await fetch(path, { ...options, headers });
    const contentType = response.headers.get("content-type") || "";
    const text = await response.text();

    let body;
    if (contentType.includes("application/json")) {
      try {
        body = text ? JSON.parse(text) : {};
      } catch (_) {
        body = { ok: false, error: text || "Invalid JSON response" };
      }
    } else {
      body = { ok: false, error: text };
    }

    if (!response.ok && !config.allowErrorResponse) {
      const msg =
        body.error ||
        body.message ||
        body?.result?.message ||
        `Request failed (${response.status})`;
      throw new Error(msg);
    }
    return body;
  }

  function unwrapDocument(payload) {
    if (!payload) return null;
    if (payload.document) return payload.document;
    return payload;
  }

  /* ==========================================================
   * Model accessors
   * ======================================================== */
  function cells() {
    return state.document && Array.isArray(state.document.cells)
      ? state.document.cells
      : [];
  }
  function findCell(id) {
    return cells().find((c) => String(c.id) === String(id)) || null;
  }
  function cellIndex(id) {
    return cells().findIndex((c) => String(c.id) === String(id));
  }
  function projectLabel(project) {
    if (!project || !project.enabled) return "No project";
    return project.projectName || project.projectRoot || "Project";
  }
  function executedCount() {
    return cells().filter((c) => Number(c.executionCount || 0) > 0).length;
  }
  function currentDocPath() {
    return (state.document && state.document.path) || "";
  }
  function normalizeExplorerPath(path) {
    let p = String(path || ".")
      .trim()
      .replaceAll("\\", "/");

    while (p.startsWith("./")) {
      p = p.slice(2);
    }

    while (p.length > 1 && p.endsWith("/")) {
      p = p.slice(0, -1);
    }

    return p || ".";
  }

  function parentPath(path) {
    const p = normalizeExplorerPath(path);

    if (p === "." || !p.includes("/")) {
      return ".";
    }

    const parts = p.split("/");
    parts.pop();

    return parts.join("/") || ".";
  }

  function documentDisplayTitle(doc) {
    if (!doc) {
      return "Untitled note";
    }

    const title = String(doc.title || "").trim();

    if (title) {
      return title;
    }

    const file = baseName(doc.path || "");

    if (file) {
      return file;
    }

    return "Untitled note";
  }

  function ancestorDirectoriesForPath(path) {
    const normalized = normalizeExplorerPath(path);

    const dirs = ["."];
    const parts = normalized.split("/");

    parts.pop();

)VIXNOTE");

    value.append(R"VIXNOTE(    let acc = "";

    for (const part of parts) {
      if (!part || part === ".") {
        continue;
      }

      acc = acc ? `${acc}/${part}` : part;
      dirs.push(acc);
    }

    return Array.from(new Set(dirs));
  }

  async function loadExplorerForDocumentPath(path) {
    const normalized = normalizeExplorerPath(path);
    const dirs = ancestorDirectoriesForPath(normalized);

    for (const dir of dirs) {
      state.explorer.expandedDirs.add(dir);

      await loadDirectory(dir, {
        silent: true,
        force: true,
      });
    }

    renderExplorer();
  }

  function joinExplorerPath(dir, name) {
    const folder = normalizeExplorerPath(dir || ".");
    const cleanName = String(name || "")
      .trim()
      .replaceAll("\\", "/")
      .split("/")
      .filter(Boolean)
      .pop();

    if (!cleanName) {
      return "";
    }

    return folder === "." ? cleanName : `${folder}/${cleanName}`;
  }
  function knownDirectories() {
    const dirs = Array.from(state.explorer.entries.values())
      .filter((entry) => entry.type === "dir")
      .map((entry) => normalizeExplorerPath(entry.path));

    if (!dirs.includes(".")) {
      dirs.unshift(".");
    }

    return Array.from(new Set(dirs)).sort((a, b) => {
      if (a === ".") return -1;
      if (b === ".") return 1;
      return a.localeCompare(b);
    });
  }

  function shouldShowEntry(entry) {
    if (!entry) {
      return false;
    }

    if (entry.type === "dir") {
      return true;
    }

    return entry.openable || String(entry.path || "").endsWith(".vixnote");
  }

  function entryModified(entry) {
    const modified = Number(entry && entry.modified ? entry.modified : 0);
    return Number.isFinite(modified) ? modified : 0;
  }

  function directChildrenOf(parent) {
    const parentPathValue = normalizeExplorerPath(parent);

    return Array.from(state.explorer.entries.values())
      .filter(shouldShowEntry)
      .filter((entry) => {
        const path = normalizeExplorerPath(entry.path);

        if (path === parentPathValue) {
          return false;
        }

        return parentPath(path) === parentPathValue;
      })
      .sort((a, b) => {
        if (a.type !== b.type) {
          return a.type === "dir" ? -1 : 1;
        }

        return String(a.title || baseName(a.path)).localeCompare(
          String(b.title || baseName(b.path)),
          undefined,
          { sensitivity: "base" },
        );
      });
  }

  function removeTabState(path) {
    const normalized = normalizeExplorerPath(path);

    state.tabs = state.tabs.filter((tab) => {
      return normalizeExplorerPath(tab.path) !== normalized;
    });

    if (
      state.activeTabPath &&
      normalizeExplorerPath(state.activeTabPath) === normalized
    ) {
      state.activeTabPath = state.tabs.length ? state.tabs[0].path : null;
    }

    state.explorer.entries.delete(normalized);

    persistTabs();
    renderOpenTabs();
    renderTabsBar();
    renderExplorer();
  }

  function isMissingNoteError(error) {
    const message = String(
      error && error.message ? error.message : error || "",
    ).toLowerCase();

    return (
      message.includes("cannot open note file") ||
      message.includes("not found") ||
      message.includes("no such file") ||
      message.includes("failed to load note")
    );
  }

  function isVirtualUnsavedDocument(doc) {
    const path = normalizeExplorerPath(doc?.path || "");
    const title = String(doc?.title || "")
      .trim()
      .toLowerCase();

    return (
      !path ||
      path === "untitled.vixnote" ||
      path === "untitled" ||
      title === "tmp"
    );
  }

  function buildExplorerTreeRows(parent = ".", depth = 0, rows = []) {
    const parentPathValue = normalizeExplorerPath(parent);

    if (parentPathValue === ".") {
      const root = state.explorer.entries.get(".") || {
        path: ".",
        type: "dir",
        title: ".",
        modified: 0,
        openable: false,
      };

      rows.push({
        ...root,
        depth: 0,
      });

      pushDraftRowIfMatches(".", 1, rows);
    }

    if (
      parentPathValue !== "." &&
      !state.explorer.expandedDirs.has(parentPathValue)
    ) {
      return rows;
    }

    const children = directChildrenOf(parentPathValue);

    for (const child of children) {
      const path = normalizeExplorerPath(child.path);

      rows.push({
        ...child,
        depth: depth + 1,
      });

      if (child.type === "dir" && state.explorer.expandedDirs.has(path)) {
        pushDraftRowIfMatches(path, depth + 2, rows);
        buildExplorerTreeRows(path, depth + 1, rows);
      } else if (child.type === "dir") {
        pushDraftRowIfMatches(path, depth + 2, rows);
      }
    }

    return rows;
  }

  function pushDraftRowIfMatches(parentPathValue, depth, rows) {
    const draft = state.explorer.draft;
    if (!draft) return;
    if (normalizeExplorerPath(draft.parentPath) !== parentPathValue) return;

    rows.push({
      isDraft: true,
      draftKind: draft.kind,
      path: `__draft__:${parentPathValue}`,
      type: draft.kind === "dir" ? "dir" : "file",
      depth,
    });
  }

  function explorerTreeRows() {
    const filter = (
      ($(sel.explorerSearch) && $(sel.explorerSearch).value) ||
      ""
    )
      .trim()
      .toLowerCase();

    if (!filter) {
      return buildExplorerTreeRows(".", 0, []);
    }

    return Array.from(state.explorer.entries.values())
      .filter(shouldShowEntry)
      .filter((entry) => {
        return (
          String(entry.path || "")
            .toLowerCase()
            .includes(filter) ||
          String(entry.title || "")
            .toLowerCase()
            .includes(filter)
        );
      })
      .sort((a, b) => {
        if (a.type !== b.type) {
          return a.type === "dir" ? -1 : 1;
        }

        return String(a.path || "").localeCompare(String(b.path || ""));
      })
      .map((entry) => ({
        ...entry,
        depth:
          normalizeExplorerPath(entry.path) === "."
            ? 0
            : normalizeExplorerPath(entry.path).split("/").length,
      }));
  }

  async function toggleDirectory(path) {
    const dirPath = normalizeExplorerPath(path);

    state.explorer.selectedDirPath = dirPath;
    state.explorer.currentPath = dirPath;

    if (state.explorer.expandedDirs.has(dirPath) && dirPath !== ".") {
      state.explorer.expandedDirs.delete(dirPath);
      renderExplorer();
      return;
    }

    state.explorer.expandedDirs.add(dirPath);

    await loadDirectory(dirPath, {
      force: false,
      silent: true,
    });
  }

  /* ==========================================================
   * Markdown renderer (small, dependency-free)
   * ======================================================== */
  function renderMarkdown(source) {
    const lines = String(source || "").split(/\r?\n/);
    const blocks = [];
    let para = [];
    let inCode = false;
    let code = [];

    const inline = (line) => {
      let s = escapeHtml(line);
      s = s.replace(/`([^`]+)`/g, "<code>$1</code>");
      s = s.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
      s = s.replace(/\*([^*]+)\*/g, "<em>$1</em>");
      return s;
    };
    const flushPara = () => {
      if (!para.length) return;
      blocks.push(`<p>${para.map(inline).join("<br>")}</p>`);
      para = [];
    };

    for (const line of lines) {
      const t = line.trim();
      if (t.startsWith("```")) {
        if (inCode) {
          blocks.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
          code = [];
          inCode = false;
        } else {
          flushPara();
          inCode = true;
        }
        continue;
      }
      if (inCode) {
        code.push(line);
        continue;
      }
      if (!t) {
        flushPara();
        continue;
      }
      if (t.startsWith("### ")) {
        flushPara();
        blocks.push(`<h3>${inline(t.slice(4))}</h3>`);
        continue;
      }
      if (t.startsWith("## ")) {
        flushPara();
        blocks.push(`<h2>${inline(t.slice(3))}</h2>`);
        continue;
      }
      if (t.startsWith("# ")) {
        flushPara();
        blocks.push(`<h1>${inline(t.slice(2))}</h1>`);
        continue;
      }
      para.push(line);
    }
    if (inCode)
      blocks.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
    flushPara();
    return blocks.length ? blocks.join("\n") : "<p></p>";
  }

  /* ==========================================================
   * Syntax highlighter
   * ======================================================== */
  const CPP_KEYWORDS = new Set([
    "alignas",
    "alignof",
    "and",
    "auto",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "class",
    "const",
    "constexpr",
    "consteval",
    "constinit",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "nullptr",
    "operator",
    "or",
    "private",
    "protected",
    "public",
    "register",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "while",
    "wchar_t",
    "override",
    "final",
    "concept",
    "requires",
    "co_await",
    "co_return",
    "co_yield",
  ]);
  const CPP_TYPES = new Set([
    "string",
    "vector",
    "map",
    "set",
    "unordered_map",
    "unordered_set",
    "array",
    "size_t",
    "ssize_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "ostream",
    "istream",
    "ostringstream",
    "istringstream",
    "optional",
    "pair",
    "tuple",
    "shared_ptr",
    "unique_ptr",
    "function",
    "string_view",
    "filesystem",
    "path",
    "atomic",
    "thread",
    "mutex",
  ]);
  const REPLY_KEYWORDS = new Set([
    "true",
    "false",
    "null",
    "let",
    "const",
    "var",
    "fn",
    "if",
    "else",
    "while",
    "for",
    "return",
    "print",
    "println",
  ]);

  function tokenizeCode(source, kind) {
    const k = normalizeKind(kind);
    const isReply = k === "reply";
    const keywords = isReply ? REPLY_KEYWORDS : CPP_KEYWORDS;
    const types = isReply ? new Set() : CPP_TYPES;

    let out = "";
    const s = String(source ?? "");
    let i = 0;
    const n = s.length;
    const push = (cls, text) => {
      out += `<span class="${cls}">${escapeHtml(text)}</span>`;
    };
    const raw = (text) => {
      out += escapeHtml(text);
    };

    while (i < n) {
      const c = s[i];
      if (!isReply && c === "#" && (i === 0 || s[i - 1] === "\n")) {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-pre", s.slice(i, j));
        i = j;
        continue;
      }
      if (c === "/" && s[i + 1] === "/") {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      if (isReply && c === "#") {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      if (c === "/" && s[i + 1] === "*") {
        let j = i + 2;
        while (j < n && !(s[j] === "*" && s[j + 1] === "/")) j++;
        j = Math.min(n, j + 2);
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      if (c === '"' || c === "'") {
        const q = c;
        let j = i + 1;
        while (j < n) {
          if (s[j] === "\\") {
)VIXNOTE");

    value.append(R"VIXNOTE(            j += 2;
            continue;
          }
          if (s[j] === q) {
            j++;
            break;
          }
          if (s[j] === "\n") break;
          j++;
        }
        push("tok-str", s.slice(i, j));
        i = j;
        continue;
      }
      if (/[0-9]/.test(c)) {
        let j = i;
        while (j < n && /[0-9a-fA-FxXuUlL.']/.test(s[j])) j++;
        push("tok-num", s.slice(i, j));
        i = j;
        continue;
      }
      if (/[A-Za-z_]/.test(c)) {
        let j = i;
        while (j < n && /[A-Za-z0-9_]/.test(s[j])) j++;
        const word = s.slice(i, j);
        let k2 = j;
        while (k2 < n && s[k2] === " ") k2++;
        const isCall = s[k2] === "(";
        if (keywords.has(word)) push("tok-kw", word);
        else if (types.has(word)) push("tok-type", word);
        else if (!isReply && /^[A-Z]/.test(word)) push("tok-type", word);
        else if (isCall) push("tok-fn", word);
        else raw(word);
        i = j;
        continue;
      }
      raw(c);
      i++;
    }
    if (!out.endsWith("\n")) out += "\n";
    return out;
  }

  /* ==========================================================
   * Output rendering
   * ======================================================== */
  function renderOutputs(outputs) {
    const list = Array.isArray(outputs) ? outputs : [];
    if (!list.length) return "";
    return list
      .map((o) => {
        const kind = normalizeKind(o.kind);
        const content = String(o.content ?? "");
        if (kind === "html") {
          return `<div class="vn-Output vn-Output--html">${content}</div>`;
        }
        const label =
          kind === "stdout"
            ? ""
            : `<span class="vn-Output__kind">${escapeHtml(kind)}</span>`;
        return `<div class="vn-Output vn-Output--${safeClass(kind)}">${label}<pre>${escapeHtml(content)}</pre></div>`;
      })
      .join("");
  }

  function runningOutputHtml(message) {
    return `<div class="vn-Output vn-Output--running"><pre>${escapeHtml(message)}</pre></div>`;
  }

  /* ==========================================================
   * Cell icons
   * ======================================================== */
  const ICONS = {
    run: '<svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>',
    up: '<svg viewBox="0 0 24 24"><path d="M12 7l6 6-1.4 1.4L12 9.8l-4.6 4.6L6 13l6-6z"/></svg>',
    down: '<svg viewBox="0 0 24 24"><path d="M12 17l-6-6 1.4-1.4L12 14.2l4.6-4.6L18 11l-6 6z"/></svg>',
    del: '<svg viewBox="0 0 24 24"><path d="M6 7h12l-1 14H7L6 7zm3-3h6l1 2H8l1-2z"/></svg>',
    copy: '<svg viewBox="0 0 24 24"><path d="M16 1H4a2 2 0 00-2 2v14h2V3h12V1zm3 4H8a2 2 0 00-2 2v14a2 2 0 002 2h11a2 2 0 002-2V7a2 2 0 00-2-2zm0 16H8V7h11v14z"/></svg>',
    edit: '<svg viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 000-1.41l-2.34-2.34a1 1 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
  };

  /* ==========================================================
   * Cell rendering pieces
   * ======================================================== */
  function inPrompt(cell) {
    if (!isCodeKind(cell.kind))
      return `<div class="vn-InputPrompt vn-InputPrompt--empty"></div>`;
    const n = Number(cell.executionCount || 0);
    const label = n > 0 ? `In&nbsp;[${n}]:` : `In&nbsp;[&nbsp;]:`;
    const cls =
      n > 0 ? "vn-InputPrompt" : "vn-InputPrompt vn-InputPrompt--empty";
    return `<div class="${cls}">${label}</div>`;
  }
  function outPrompt(cell) {
    if (!isCodeKind(cell.kind)) return "";
    const n = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    if (!outs.length) return `<div class="vn-OutputPrompt"></div>`;
    const label = n > 0 ? `Out[${n}]:` : `Out[&nbsp;]:`;
    return `<div class="vn-OutputPrompt">${label}</div>`;
  }

  function editorBlock(cell) {
    const kind = normalizeKind(cell.kind);
    const source = String(cell.source ?? "");
    const code = isCodeKind(kind);
    const editorCls = code ? "vn-Editor" : "vn-Editor vn-Editor--plain";
    const highlight = code
      ? `<div class="vn-Editor__highlight" data-highlight>${tokenizeCode(source, kind)}</div>`
      : "";

    return `
  <div class="${editorCls}">
    <div class="vn-Editor__wrap">
      ${highlight}
      <div class="vn-Editor__lineFocus" data-line-focus aria-hidden="true"></div>
      <textarea spellcheck="false" data-action="edit-source" rows="1">${escapeHtml(source)}</textarea>
    </div>
  </div>`;
  }

  function cellBody(cell) {
    const kind = normalizeKind(cell.kind);
    if (kind === "markdown") {
      return `
        <div class="vn-MarkdownCell">
          <div class="vn-RenderedMarkdown" data-rendered>${renderMarkdown(cell.source)}</div>
          <div class="vn-InputArea">
            <div class="vn-InputPrompt vn-InputPrompt--empty"></div>
            ${editorBlock(cell)}
          </div>
        </div>`;
    }
    if (kind === "html") {
      return `
        <div class="vn-HtmlCell">
          <div class="vn-RenderedHTML" data-rendered>${String(cell.source || "")}</div>
          <div class="vn-InputArea">
            <div class="vn-InputPrompt vn-InputPrompt--empty"></div>
            ${editorBlock(cell)}
          </div>
        </div>`;
    }
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    const outputArea = outs.length
      ? `<div class="vn-OutputArea">
           ${outPrompt(cell)}
           <div class="vn-OutputArea__list">${renderOutputs(outs)}</div>
         </div>`
      : "";
    return `
      <div class="vn-CodeCell">
        <div class="vn-InputArea">
          ${inPrompt(cell)}
          ${editorBlock(cell)}
        </div>
        ${outputArea}
      </div>`;
  }

  function cellToolbar(cell) {
    const firstBtn = isCodeKind(cell.kind)
      ? `<button type="button" data-cell-action="run" title="Run">${ICONS.run}</button>`
      : `<button type="button" data-cell-action="edit" title="Edit source">${ICONS.edit}</button>`;
    return `
      <div class="vn-Cell__toolbar">
        ${firstBtn}
        <button type="button" data-cell-action="duplicate" title="Duplicate">${ICONS.copy}</button>
        <button type="button" data-cell-action="up" title="Move up">${ICONS.up}</button>
        <button type="button" data-cell-action="down" title="Move down">${ICONS.down}</button>
        <button type="button" data-cell-action="delete" title="Delete">${ICONS.del}</button>
      </div>`;
  }

  function renderCell(cell) {
    const kind = normalizeKind(cell.kind);
    const id = String(cell.id || "");
    const selected = state.selectedId === id;
    const editing = selected && state.editing;
    const typeClass =
      kind === "markdown"
        ? "vn-Cell--markdown"
        : kind === "html"
          ? "vn-Cell--html"
          : "vn-Cell--code";
    const classes = [
      "vn-Cell",
      typeClass,
      selected ? "is-selected" : "",
      editing ? "is-editing" : "",
    ]
      .filter(Boolean)
      .join(" ");

    return `
      <div class="vn-CellInsert">
        <button class="vn-CellInsert__btn" type="button" data-insert-after="${escapeHtml(id)}" title="Insert cell below">+</button>
      </div>
      <div class="${classes}" data-cell-id="${escapeHtml(id)}" data-kind="${escapeHtml(kind)}" tabindex="-1">
        <div class="vn-Cell__collapser" data-cell-action="select"></div>
        <div class="vn-Cell__body">
          ${cellToolbar(cell)}
          ${cellBody(cell)}
        </div>
      </div>`;
  }

  function renderEmpty(message) {
    const c = $(sel.cells);
    if (c)
      c.innerHTML = `<div class="vn-Notebook__empty">${escapeHtml(message)}</div>`;
  }

  function clearEditorNoOpenNote() {
    state.document = null;
    state.selectedId = null;
    state.editing = false;
    state.kernel = "idle";
    state.activeTabPath = null;

    setText(sel.cellCount, "0");
    setText(sel.execCount, "0");
    setText(sel.statusPosition, "Cell 0 of 0");
    setText(sel.statusKind, "—");
    setText(sel.statusMode, "Command");

    document.title = "Vix Note";

    if (state.diagnostics) clearDiagnostics();

    renderEmpty(
      "No open note. Open a note from the explorer or create a new one.",
    );
    renderOpenTabs();
    renderTabsBar();
  }

  function updateStatusBar() {
    const list = cells();
    const idx = cellIndex(state.selectedId);
    setText(sel.statusMode, state.editing ? "Edit" : "Command");
    setText(
      sel.statusPosition,
      `Cell ${idx >= 0 ? idx + 1 : 0} of ${list.length}`,
    );
    const cell = findCell(state.selectedId);
    setText(sel.statusKind, cell ? kindLabel(cell.kind) : "—");
  }

  /* ==========================================================
   * Keyed reconcile — guarantees one cell = one DOM node
   * ======================================================== */
  function cellSignature(cell) {
    const kind = normalizeKind(cell.kind);
    const exec = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs.length : 0;
    const selected = state.selectedId === String(cell.id);
    const ed = selected && state.editing;
    return `${kind}|${exec}|${outs}|${selected ? 1 : 0}|${ed ? 1 : 0}`;
  }

  function buildCellNodes(cell) {
    const tpl = document.createElement("template");
    tpl.innerHTML = renderCell(cell).trim();
    return Array.from(tpl.content.children);
  }

  function patchCellOutputs(cellEl, cell) {
    const codeCell = $(".vn-CodeCell", cellEl);
    if (!codeCell) return;
    const inP = $(".vn-InputPrompt", codeCell);
    if (inP) {
      const tmp = document.createElement("div");
      tmp.innerHTML = inPrompt(cell);
      const fresh = tmp.firstElementChild;
      if (fresh) inP.replaceWith(fresh);
    }
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    let oa = $(".vn-OutputArea", codeCell);
    if (!outs.length) {
      if (oa) oa.remove();
      return;
    }
    const html = `<div class="vn-OutputArea">${outPrompt(cell)}<div class="vn-OutputArea__list">${renderOutputs(outs)}</div></div>`;
    const tmp = document.createElement("div");
    tmp.innerHTML = html;
    const fresh = tmp.firstElementChild;
    if (oa) oa.replaceWith(fresh);
    else codeCell.appendChild(fresh);
  }

  function insertBarOf(cellEl) {
    const prev = cellEl.previousElementSibling;
    return prev && prev.classList.contains("vn-CellInsert") ? prev : null;
  }

  function reconcileCells(container, list) {
    const existing = new Map();
    for (const el of $all(".vn-Cell", container))
      existing.set(el.dataset.cellId, el);

    const desired = [];
    for (const cell of list) {
      const id = String(cell.id);
      const sig = cellSignature(cell);
      const prev = existing.get(id);

      if (prev && prev.dataset.sig === sig) {
        desired.push([insertBarOf(prev), prev]);
        existing.delete(id);
      } else if (prev && state.editing && state.selectedId === id) {
        patchCellOutputs(prev, cell);
        prev.dataset.sig = sig;
        desired.push([insertBarOf(prev), prev]);
        existing.delete(id);
      } else {
        const nodes = buildCellNodes(cell);
        const bar = nodes[0];
        const cellEl = nodes[1];
        if (cellEl && cellEl.dataset) cellEl.dataset.sig = sig;
        if (prev) {
          const oldBar = insertBarOf(prev);
          if (oldBar) oldBar.remove();
          prev.remove();
          existing.delete(id);
        }
        desired.push([bar, cellEl]);
      }
    }

    for (const [, el] of existing) {
      const bar = insertBarOf(el);
      if (bar) bar.remove();
      el.remove();
    }

    const frag = document.createDocumentFragment();
    for (const [bar, cellEl] of desired) {
      if (bar) frag.appendChild(bar);
      if (cellEl) frag.appendChild(cellEl);
    }
    container.appendChild(frag);
  }

  function renderDocument(payload, opts = {}) {
    const doc = unwrapDocument(payload);
)VIXNOTE");

    value.append(R"VIXNOTE(    if (!doc) {
      renderEmpty("Unable to load the note document.");
      return;
    }
    state.document = doc;

    const title = documentDisplayTitle(doc);
    const count = Number(doc.cellCount || (doc.cells ? doc.cells.length : 0));
    const project = doc.project || null;

    setText(sel.project, projectLabel(project));
    setText(sel.cellCount, String(count));
    setText(sel.execCount, String(executedCount()));
    document.title = `${title} · Vix Note`;

    if (doc.path && !isVirtualUnsavedDocument(doc)) {
      syncActiveTab({
        ...doc,
        title,
      });

      renderOpenTabs();
      renderTabsBar();
    } else {
      renderTabsBar();
    }
    const container = $(sel.cells);
    if (!container) return;
    const list = cells();
    if (!list.length) {
      renderEmpty(
        "This note has no cells yet. Use the toolbar + or the insert button to add a cell.",
      );
      updateStatusBar();
      return;
    }
    if (!state.selectedId || !findCell(state.selectedId)) {
      state.selectedId = String(list[0].id);
    }

    const firstPaint = !$(".vn-Cell", container);
    const scroller = container.closest(".vn-NotebookPanel");
    const prevScroll = scroller ? scroller.scrollTop : 0;

    if (firstPaint || opts.fullRepaint) {
      container.innerHTML = list.map(renderCell).join("");
      for (const el of $all(".vn-Cell", container)) {
        const cell = findCell(el.dataset.cellId);
        if (cell) el.dataset.sig = cellSignature(cell);
      }
    } else {
      reconcileCells(container, list);
    }

    if (scroller) scroller.scrollTop = prevScroll;
    autosizeAll();
    syncToolbarKind();
    updateStatusBar();

    if (state.diagnostics && state.diagnostics.items.length) {
      recomputeDiagnosticsState();
    } else {
      refreshCellProblemBadges();
    }
  }

  /* ==========================================================
   * Editor autosize + highlight + textarea editing
   * ======================================================== */
  const EDITOR_INDENT = "  ";

  function autosize(textarea) {
    if (!textarea) return;

    textarea.style.height = "auto";

    const nextHeight = Math.max(textarea.scrollHeight, 96);
    textarea.style.height = `${nextHeight}px`;

    const wrap = textarea.closest(".vn-Editor__wrap");
    const hl = wrap ? $("[data-highlight]", wrap) : null;

    if (hl) {
      hl.style.height = `${nextHeight}px`;
    }

    updateLineFocus(textarea);
  }

  function autosizeAll() {
    for (const ta of $all('textarea[data-action="edit-source"]')) {
      autosize(ta);
      updateHighlight(ta);
      updateLineFocus(ta);
    }
  }

  function updateHighlight(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");
    if (!wrap) return;
    const hl = $("[data-highlight]", wrap);
    if (!hl) return;
    const cellEl = textarea.closest(".vn-Cell");
    const kind = cellEl ? cellEl.dataset.kind : "cpp";
    hl.innerHTML = tokenizeCode(textarea.value, kind);
  }

  function syncScroll(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");
    if (!wrap) return;
    const hl = $("[data-highlight]", wrap);
    if (hl) {
      hl.scrollTop = textarea.scrollTop;
      hl.scrollLeft = textarea.scrollLeft;
    }
    updateLineFocus(textarea);
  }

  function textareaLineInfo(textarea) {
    const value = textarea.value || "";
    const cursor = textarea.selectionStart || 0;
    const before = value.slice(0, cursor);
    const lines = before.split("\n");
    const line = lines.length;
    const column = lines[lines.length - 1].length + 1;
    return { line, column };
  }

  function updateCursorStatus(textarea) {
    const info = textareaLineInfo(textarea);
    setText(sel.statusPosition, `Ln ${info.line}, Col ${info.column}`);
  }

  function updateLineFocus(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");
    if (!wrap) return;
    const focus = $("[data-line-focus]", wrap);
    if (!focus) return;
    const info = textareaLineInfo(textarea);
    const style = window.getComputedStyle(textarea);
    const lineHeight = Number.parseFloat(style.lineHeight) || 22;
    const paddingTop = Number.parseFloat(style.paddingTop) || 0;
    const top = paddingTop + (info.line - 1) * lineHeight - textarea.scrollTop;
    focus.style.transform = `translateY(${top}px)`;
    focus.style.height = `${lineHeight}px`;
  }

  function markTextareaChanged(textarea) {
    autosize(textarea);
    updateHighlight(textarea);
    updateLineFocus(textarea);
    updateCursorStatus(textarea);

    const cellEl = textarea.closest(".vn-Cell");
    if (!cellEl) return;

    const cell = findCell(cellEl.dataset.cellId);
    if (cell) cell.source = textarea.value;

    setDirty(true);

    const kind = cellEl.dataset.kind;
    if (kind === "markdown") {
      const rendered = $("[data-rendered]", cellEl);
      if (rendered) rendered.innerHTML = renderMarkdown(textarea.value);
    } else if (kind === "html") {
      const rendered = $("[data-rendered]", cellEl);
      if (rendered) rendered.innerHTML = String(textarea.value || "");
    }
  }

  function selectedLineRange(textarea) {
    const value = textarea.value;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    let lineStart = value.lastIndexOf("\n", Math.max(0, start - 1)) + 1;
    let lineEnd = value.indexOf("\n", end);
    if (lineEnd === -1) lineEnd = value.length;
    return { lineStart, lineEnd, start, end };
  }

  function indentTextarea(textarea) {
    const value = textarea.value;
    const { lineStart, lineEnd, start, end } = selectedLineRange(textarea);
    const selected = value.slice(lineStart, lineEnd);

    if (start === end) {
      textarea.value = value.slice(0, start) + EDITOR_INDENT + value.slice(end);
      textarea.selectionStart = textarea.selectionEnd =
        start + EDITOR_INDENT.length;
      markTextareaChanged(textarea);
      return;
    }

    const indented = selected
      .split("\n")
      .map((line) => EDITOR_INDENT + line)
      .join("\n");

    textarea.value =
      value.slice(0, lineStart) + indented + value.slice(lineEnd);
    textarea.selectionStart = start + EDITOR_INDENT.length;
    textarea.selectionEnd =
      end + EDITOR_INDENT.length * selected.split("\n").length;
    markTextareaChanged(textarea);
  }

  function outdentTextarea(textarea) {
    const value = textarea.value;
    const { lineStart, lineEnd, start, end } = selectedLineRange(textarea);
    const selected = value.slice(lineStart, lineEnd);
    const lines = selected.split("\n");

    let removedBeforeStart = 0;
    let removedTotal = 0;

    const outdented = lines
      .map((line, index) => {
        let remove = 0;
        if (line.startsWith(EDITOR_INDENT)) remove = EDITOR_INDENT.length;
        else if (line.startsWith("\t")) remove = 1;
        else if (line.startsWith(" ")) remove = 1;
        if (index === 0) removedBeforeStart = remove;
        removedTotal += remove;
        return line.slice(remove);
      })
      .join("\n");

    textarea.value =
      value.slice(0, lineStart) + outdented + value.slice(lineEnd);
    textarea.selectionStart = Math.max(lineStart, start - removedBeforeStart);
    textarea.selectionEnd = Math.max(
      textarea.selectionStart,
      end - removedTotal,
    );
    markTextareaChanged(textarea);
  }

  function toggleCommentTextarea(textarea) {
    const cellEl = textarea.closest(".vn-Cell");
    const kind = normalizeKind(cellEl ? cellEl.dataset.kind : "cpp");
    const marker = kind === "html" ? null : kind === "markdown" ? "> " : "// ";
    if (!marker) return;

    const value = textarea.value;
    const { lineStart, lineEnd, start, end } = selectedLineRange(textarea);
    const selected = value.slice(lineStart, lineEnd);
    const lines = selected.split("\n");

    const allCommented = lines
      .filter((line) => line.trim() !== "")
      .every((line) => line.trimStart().startsWith(marker.trim()));

    let delta = 0;

    const next = lines
      .map((line) => {
        if (line.trim() === "") return line;
        const indent = line.match(/^\s*/)?.[0] || "";
        if (allCommented) {
          const afterIndent = line.slice(indent.length);
          if (afterIndent.startsWith(marker)) {
            delta -= marker.length;
            return indent + afterIndent.slice(marker.length);
          }
          if (afterIndent.startsWith(marker.trim())) {
            delta -= marker.trim().length;
            return (
              indent + afterIndent.slice(marker.trim().length).replace(/^ /, "")
            );
          }
          return line;
        }
        delta += marker.length;
        return indent + marker + line.slice(indent.length);
      })
      .join("\n");

    textarea.value = value.slice(0, lineStart) + next + value.slice(lineEnd);
    textarea.selectionStart = start;
    textarea.selectionEnd = Math.max(start, end + delta);
    markTextareaChanged(textarea);
  }

  function moveCurrentLine(textarea, direction) {
    const value = textarea.value;
    const cursor = textarea.selectionStart;
    const lines = value.split("\n");

    let pos = 0;
    let lineIndex = 0;
    for (; lineIndex < lines.length; ++lineIndex) {
      const nextPos = pos + lines[lineIndex].length + 1;
      if (cursor < nextPos) break;
      pos = nextPos;
    }

    const target = lineIndex + direction;
    if (target < 0 || target >= lines.length) return;

    const currentLine = lines[lineIndex];
    lines[lineIndex] = lines[target];
    lines[target] = currentLine;

    textarea.value = lines.join("\n");

    const movedBy =
      direction < 0 ? -(lines[lineIndex].length + 1) : lines[target].length + 1;
    const nextCursor = Math.max(
      0,
      Math.min(textarea.value.length, cursor + movedBy),
    );
    textarea.selectionStart = textarea.selectionEnd = nextCursor;
    markTextareaChanged(textarea);
  }

  /* ==========================================================
   * Selection / modes
   * ======================================================== */
  function cssEscape(v) {
    return String(v).replace(/["\\]/g, "\\$&");
  }
  function cellElById(id) {
    return $(`.vn-Cell[data-cell-id="${cssEscape(id)}"]`);
  }

  function selectCell(id, { edit = false, focus = true } = {}) {
    state.selectedId = String(id);
    state.editing = edit;
    for (const el of $all(".vn-Cell")) {
      const isSel = el.dataset.cellId === String(id);
      el.classList.toggle("is-selected", isSel);
      el.classList.toggle("is-editing", isSel && edit);
    }
    const el = cellElById(id);
    if (el) {
      if (edit) {
        const ta = $('textarea[data-action="edit-source"]', el);
        if (ta && focus) {
          ta.focus({ preventScroll: true });
          autosize(ta);
        }
      } else if (focus) {
        el.focus({ preventScroll: true });
      }
    }
    syncToolbarKind();
    updateStatusBar();
  }

  function enterEditMode() {
    if (state.selectedId) selectCell(state.selectedId, { edit: true });
  }

  function exitEditMode(options = {}) {
    const repaint = options.repaint !== false;
    state.editing = false;
    if (app) {
      app.classList.remove("is-editing");
      app.classList.add("is-command");
    }
    setText(sel.statusMode, "Command");
    if (repaint) {
      renderDocument(
        { ok: true, document: state.document },
        { fullRepaint: true },
      );
    }
  }

  function toggleCellEdit(cellId) {
    const id = String(cellId || "");
    if (!id) return;
    if (state.editing && state.selectedId === id) {
      const cellEl = cellElById(id);
      if (cellEl) localUpdateFromDom(cellEl);
      exitEditMode();
      return;
    }
    selectCell(id, { edit: true, focus: true });
  }

  function enterCommandMode() {
    state.editing = false;
    const el = cellElById(state.selectedId);
    if (el) {
      el.classList.remove("is-editing");
      el.focus({ preventScroll: true });
    }
)VIXNOTE");

    value.append(R"VIXNOTE(    updateStatusBar();
  }
  function selectAdjacent(delta) {
    const list = cells();
    const idx = cellIndex(state.selectedId);
    if (idx < 0) return;
    const next = idx + delta;
    if (next < 0 || next >= list.length) return;
    selectCell(list[next].id, { edit: false });
    const el = cellElById(list[next].id);
    if (el) el.scrollIntoView({ block: "nearest" });
  }

  function toolbarKindLabel(kind) {
    switch (normalizeKind(kind)) {
      case "cpp":
        return "C++";
      case "reply":
        return "Reply";
      case "markdown":
        return "Markdown";
      case "html":
        return "HTML";
      default:
        return "C++";
    }
  }

  function closeToolbarKindMenu() {
    const button = $(sel.toolbarKind);
    const menu = $("[data-toolbar-kind-menu]");
    const root = $("[data-cell-type-select]");

    if (button) {
      button.setAttribute("aria-expanded", "false");
    }

    if (menu) {
      menu.setAttribute("hidden", "");
    }

    if (root) {
      root.classList.remove("is-open");
    }
  }
  function toggleToolbarKindMenu() {
    const button = $(sel.toolbarKind);
    const menu = $("[data-toolbar-kind-menu]");
    const root = $("[data-cell-type-select]");

    if (!button || !menu) {
      return;
    }

    const nextOpen = menu.hasAttribute("hidden");

    if (nextOpen) {
      menu.removeAttribute("hidden");
    } else {
      menu.setAttribute("hidden", "");
    }

    button.setAttribute("aria-expanded", nextOpen ? "true" : "false");

    if (root) {
      root.classList.toggle("is-open", nextOpen);
    }
  }

  function setToolbarKind(kind, options = {}) {
    const nextKind = ["cpp", "reply", "markdown", "html"].includes(
      normalizeKind(kind),
    )
      ? normalizeKind(kind)
      : "cpp";

    const button = $(sel.toolbarKind);
    const label = $("[data-toolbar-kind-label]");

    if (button) {
      button.dataset.kind = nextKind;
    }

    if (label) {
      label.textContent = toolbarKindLabel(nextKind);
    }

    for (const option of $all("[data-kind-option]")) {
      const active = option.dataset.kindOption === nextKind;
      option.classList.toggle("is-active", active);
      option.setAttribute("aria-selected", active ? "true" : "false");
    }

    if (options.applyToCell !== false && state.selectedId) {
      changeKind(state.selectedId, nextKind);
    }
  }

  function syncToolbarKind() {
    const cell = findCell(state.selectedId);
    const kind = cell ? normalizeKind(cell.kind) : currentToolbarKind();

    setToolbarKind(
      ["cpp", "reply", "markdown", "html"].includes(kind) ? kind : "cpp",
      { applyToCell: false },
    );
  }

  /* ==========================================================
   * Cell sync
   * ======================================================== */
  function localUpdateFromDom(cellEl) {
    const cell = findCell(cellEl.dataset.cellId);
    if (!cell) return null;
    const ta = $('textarea[data-action="edit-source"]', cellEl);
    if (ta) cell.source = ta.value;
    return cell;
  }
  async function pushCell(cellEl) {
    const cell = localUpdateFromDom(cellEl);
    if (!cell) return;
    const id = encodeURIComponent(cellEl.dataset.cellId);
    const result = await api(`/api/cells/${id}`, {
      method: "PUT",
      body: JSON.stringify({ kind: cell.kind, source: cell.source }),
    });
    if (result.document) state.document = unwrapDocument(result.document);
  }

  /* ==========================================================
   * Dirty tracking (per active tab)
   * ======================================================== */
  function setDirty(dirty) {
    const tab = activeTab();
    if (tab) tab.dirty = !!dirty;
    persistTabs();
    renderOpenTabs();
    renderTabsBar();
  }
  function isDirty() {
    const tab = activeTab();
    return !!(tab && tab.dirty);
  }

  /* ==========================================================
   * Cell actions
   * ======================================================== */
  function defaultSource(kind) {
    const k = normalizeKind(kind);
    if (k === "cpp")
      return '#include <iostream>\n\nint main()\n{\n  std::cout << "Hello from Vix Note\\n";\n  return 0;\n}\n';
    if (k === "reply") return 'x = 1 + 2 * 3\nprintln("x =", x)\n';
    if (k === "html")
      return "<section>\n  <h2>Hello</h2>\n  <p>Rendered by the note UI.</p>\n</section>\n";
    return "Write your explanation here.";
  }

  async function addCell(
    kind,
    { afterId = null, atIndex = null, source = null } = {},
  ) {
    setMessage("");
    setBusy(true);
    const body = {
      kind,
      source: source != null ? source : defaultSource(kind),
    };
    if (atIndex != null) body.index = atIndex;
    else if (afterId != null) {
      const idx = cellIndex(afterId);
      if (idx >= 0) body.index = idx + 1;
    }
    try {
      const result = await api("/api/cells", {
        method: "POST",
        body: JSON.stringify(body),
      });
      const newId = result.cellId || result.cell?.id || null;
      if (result.document) {
        state.selectedId = newId || state.selectedId;
        renderDocument(result.document);
      } else await loadDocument();
      if (newId) {
        selectCell(newId, { edit: normalizeKind(kind) !== "markdown" });
        const el = cellElById(newId);
        if (el) el.scrollIntoView({ block: "nearest" });
      }
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to add cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function duplicateCell(id) {
    const cell = findCell(id);
    if (!cell) return;
    await addCell(normalizeKind(cell.kind), {
      afterId: id,
      source: String(cell.source || ""),
    });
  }

  async function runCellById(id) {
    const cellEl = cellElById(id);
    const cell = findCell(id);
    if (!cellEl || !cell) return;

    if (!isCodeKind(cell.kind)) {
      localUpdateFromDom(cellEl);
      try {
        await pushCell(cellEl);
      } catch (_) {}
      return;
    }

    cellEl.classList.add("is-running");
    setMessage("");
    setKernel("busy");
    setDiagnosticsRunning();

    const codeCell = $(".vn-CodeCell", cellEl);
    if (codeCell) {
      let oa = $(".vn-OutputArea", codeCell);
      if (!oa) {
        codeCell.insertAdjacentHTML(
          "beforeend",
          `<div class="vn-OutputArea"><div class="vn-OutputPrompt">Out[&nbsp;]:</div><div class="vn-OutputArea__list">${runningOutputHtml("Running…")}</div></div>`,
        );
      } else {
        const listEl = $(".vn-OutputArea__list", oa);
        if (listEl) listEl.innerHTML = runningOutputHtml("Running…");
      }
    }

    try {
      await pushCell(cellEl);
      const key = encodeURIComponent(id);
      const result = await api(
        `/api/cells/${key}/run`,
        { method: "POST" },
        { allowErrorResponse: true },
      );

      if (result.document) {
        state.document = unwrapDocument(result.document);
        setText(sel.execCount, String(executedCount()));
        const fresh = findCell(id);
        const stillEl = cellElById(id);
        if (fresh && stillEl) {
          patchCellOutputs(stillEl, fresh);
          stillEl.dataset.sig = cellSignature(fresh);
        } else {
          renderDocument(result.document);
        }
      } else {
        await loadDocument();
      }

      const status = normalizeKind(result?.result?.status);

      setCellDiagnostics(id, result?.result);

      if (status === "failure") {
        setKernel("error");

        /*
         * Do not show a global flash for normal C++ execution errors.
         * The error already appears in the cell output and in the Problems panel.
         */
        clearMessageQuietly();

        setTimeout(() => setKernel("idle"), 1200);
      } else if (status === "skipped") {
        setKernel("idle");
        setMessage(result?.result?.message || "Cell skipped.", "warning");
      } else {
        setKernel("idle");
      }
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to run cell.", "error");
      state.diagnostics.status = "failed";
      renderProblems();
      setTimeout(() => setKernel("idle"), 1200);
    } finally {
      const el = cellElById(id);
      if (el) el.classList.remove("is-running");
    }
  }

  async function runAll() {
    setMessage("");
    setBusy(true);
    setKernel("busy");
    setDiagnosticsRunning();
    try {
      for (const el of $all(".vn-Cell")) {
        try {
          await pushCell(el);
        } catch (_) {}
      }
      const result = await api(
        "/api/run-all",
        { method: "POST" },
        { allowErrorResponse: true },
      );
      if (result.document) renderDocument(result.document);

      setRunAllDiagnostics(result);

      if (result.ok) {
        setKernel("idle");
        clearMessageQuietly();
      } else if (result.stopped) {
        setKernel("error");
        clearMessageQuietly();
        setTimeout(() => setKernel("idle"), 1200);
      } else {
        setKernel("error");
        clearMessageQuietly();
        setTimeout(() => setKernel("idle"), 1200);
      }
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to run all cells.", "error");
      state.diagnostics.status = "failed";
      renderProblems();
      setTimeout(() => setKernel("idle"), 1200);
    } finally {
      setBusy(false);
    }
  }

  async function moveCellById(id, direction) {
    const idx = cellIndex(id);
    if (idx < 0) return;
    const target = direction === "up" ? idx - 1 : idx + 1;
    const list = cells();
    if (target < 0 || target >= list.length) return;
    setMessage("");
    setBusy(true);
    try {
      const cellEl = cellElById(id);
      if (cellEl) {
        try {
          await pushCell(cellEl);
        } catch (_) {}
      }
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}/move`, {
        method: "POST",
        body: JSON.stringify({ index: target }),
      });
      state.selectedId = String(id);
      if (result.document) renderDocument(result.document);
      else await loadDocument();
      selectCell(id, { edit: false });
      const el = cellElById(id);
      if (el) el.scrollIntoView({ block: "nearest" });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to move cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function deleteCellById(id) {
    setMessage("");
    setBusy(true);
    const idx = cellIndex(id);
    const list = cells();
    const fallbackId =
      idx >= 0
        ? (list[idx + 1] && list[idx + 1].id) ||
          (list[idx - 1] && list[idx - 1].id) ||
          null
        : null;
    try {
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}`, { method: "DELETE" });
      state.selectedId = fallbackId ? String(fallbackId) : null;
      if (result.document) renderDocument(result.document);
      else await loadDocument();
      if (state.selectedId) selectCell(state.selectedId, { edit: false });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to delete cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function changeKind(id, newKind) {
    const cellEl = cellElById(id);
    const cell = findCell(id);
    if (!cellEl || !cell) return;
    if (normalizeKind(cell.kind) === normalizeKind(newKind)) {
      selectCell(id, { edit: false });
      return;
    }
    localUpdateFromDom(cellEl);
    cell.kind = newKind || "cpp";
    setBusy(true);
    try {
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}`, {
        method: "PUT",
        body: JSON.stringify({ kind: cell.kind, source: cell.source }),
      });
      if (result.document) {
        state.selectedId = String(id);
        renderDocument(result.document);
      }
)VIXNOTE");

    value.append(R"VIXNOTE(      selectCell(id, { edit: false });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to change cell type.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function saveNote() {
    setMessage("");
    setBusy(true);
    try {
      for (const el of $all(".vn-Cell")) {
        try {
          await pushCell(el);
        } catch (_) {}
      }
      const saved = await api("/api/document/save", { method: "POST" });
      const savedDoc =
        unwrapDocument(saved?.document || saved) || state.document;

      if (savedDoc && savedDoc.path) {
        state.document = savedDoc;

        touchExplorerEntry(savedDoc.path, "file", baseName(savedDoc.path), {
          modified: Date.now(),
          openable: true,
          extension: ".vixnote",
        });

        openTab(savedDoc.path, documentDisplayTitle(savedDoc));
        state.activeTabPath = normalizeExplorerPath(savedDoc.path);

        await loadDirectory(parentPath(savedDoc.path), {
          silent: true,
          force: true,
        });

        renderExplorer();
        renderTabsBar();
        renderOpenTabs();
      }

      setDirty(false);
      clearMessageQuietly();
    } catch (error) {
      setMessage(error.message || "Failed to save note.", "error");
      setDirty(true);
    } finally {
      setBusy(false);
    }
  }

  /* ==========================================================
   * VS Code-style INLINE create (files + folders)
   * ======================================================== */
  function inlineCreateParent(explicitDir) {
    if (explicitDir != null) {
      return normalizeExplorerPath(explicitDir);
    }

    const selected = normalizeExplorerPath(
      state.explorer.selectedDirPath || ".",
    );
    const entry = state.explorer.entries.get(selected);

    if (entry && entry.type === "file") {
      return parentPath(selected);
    }

    return selected || ".";
  }

  async function startInlineCreate(kind, explicitDir = null) {
    const search = $(sel.explorerSearch);
    if (search && search.value) {
      search.value = "";
    }

    const parent = inlineCreateParent(explicitDir);

    if (parent !== ".") {
      state.explorer.expandedDirs.add(parent);
    }

    state.explorer.selectedDirPath = parent;
    state.explorer.currentPath = parent;
    state.explorer.draft = { kind, parentPath: parent, error: null };

    if (state.activePanel !== "explorer" || state.sidebarCollapsed) {
      setPanel("explorer");
    }

    if (parent !== "." && !state.explorer.loadedDirs.has(parent)) {
      await loadDirectory(parent, { silent: true, force: false });
    } else {
      renderExplorer();
    }

    focusDraftInput();
  }

  function cancelInlineCreate() {
    if (!state.explorer.draft) return;
    state.explorer.draft = null;
    renderExplorer();
  }

  function focusDraftInput() {
    requestAnimationFrame(() => {
      const input = $(".vn-Tree__input");
      if (!input) return;
      input.focus();
      input.select();
    });
  }

  function validateDraftName(rawName) {
    const draft = state.explorer.draft;
    if (!draft) return { ok: false };

    let name = String(rawName || "")
      .trim()
      .replaceAll("\\", "/");
    name = baseName(name);

    if (!name) {
      return { ok: false, error: "A name is required." };
    }

    if (/[<>:"|?*]/.test(name)) {
      return { ok: false, error: "Name contains invalid characters." };
    }

    if (draft.kind === "file" && !name.endsWith(".vixnote")) {
      name = `${name}.vixnote`;
    }

    const path = joinExplorerPath(draft.parentPath, name);
    if (!path) {
      return { ok: false, error: "Invalid name." };
    }

    const existing = state.explorer.entries.get(normalizeExplorerPath(path));
    if (existing) {
      return {
        ok: false,
        error:
          draft.kind === "dir"
            ? "A folder with that name already exists."
            : "A file with that name already exists.",
      };
    }

    return { ok: true, name, path };
  }

  async function commitInlineCreate(rawName) {
    const draft = state.explorer.draft;
    if (!draft) return;

    const result = validateDraftName(rawName);
    if (!result.ok) {
      if (!String(rawName || "").trim()) {
        cancelInlineCreate();
        return;
      }
      draft.error = result.error || "Invalid name.";
      renderExplorer();
      focusDraftInput();
      return;
    }

    const { name, path } = result;
    const kind = draft.kind;

    state.explorer.draft = null;

    setBusy(true);
    setMessage("");

    try {
      if (kind === "dir") {
        await api("/api/directory/create", {
          method: "POST",
          body: JSON.stringify({ path }),
        });

        touchExplorerEntry(path, "dir", baseName(path), {
          modified: Date.now(),
          openable: false,
        });

        state.explorer.expandedDirs.add(parentPath(path));
        state.explorer.expandedDirs.add(path);
        state.explorer.selectedDirPath = path;

        await loadDirectory(parentPath(path), { silent: true, force: true });
        clearMessageQuietly();
      } else {
        const title = titleFromFileName(name);
        const doc = await api("/api/document/new", {
          method: "POST",
          body: JSON.stringify({ path, title }),
        });
        const d = unwrapDocument(doc);

        openTab(d.path, d.title || title);
        state.selectedId = null;
        renderDocument(doc, { fullRepaint: true });
        setDirty(false);

        touchExplorerEntry(d.path, "file", baseName(d.path), {
          modified: Date.now(),
          openable: true,
          extension: ".vixnote",
        });

        state.explorer.selectedDirPath = parentPath(d.path);
        await loadDirectory(parentPath(d.path), { silent: true, force: true });
        clearMessageQuietly();
      }
    } catch (error) {
      setMessage(
        error.message ||
          (kind === "dir"
            ? "Failed to create folder."
            : "Failed to create note."),
        "error",
      );
    } finally {
      setBusy(false);
      renderExplorer();
    }
  }

  function newNote(dir = null) {
    return startInlineCreate("file", dir);
  }
  function newFolder(parentDir = null) {
    return startInlineCreate("dir", parentDir);
  }

  /* ==========================================================
   * Open note
   * ======================================================== */
  async function openNote(prefill) {
    const data = await showModalForm({
      title: "Open note",
      fields: [
        {
          name: "path",
          label: "Path",
          value: prefill || "lessons/intro.vixnote",
          placeholder: "lessons/intro.vixnote",
          hint: "Path to an existing .vixnote file",
        },
      ],
      confirm: "Open",
    });
    if (!data) return;
    const path = (data.path || "").trim();
    if (!path) return;
    await openNotePath(path);
  }

  async function openNotePath(path, options = {}) {
    const silent = !!options.silent;
    if (!path) return;
    if (!path.endsWith(".vixnote")) {
      setMessage("Note path must end with .vixnote", "error");
      return;
    }
    setBusy(true);

    if (!silent) {
      setMessage("");
    }
    try {
      const doc = await api("/api/document/open", {
        method: "POST",
        body: JSON.stringify({ path }),
      });
      const d = unwrapDocument(doc);

      clearDiagnostics();

      openTab(d.path, documentDisplayTitle(d));
      state.selectedId = null;
      renderDocument(doc, { fullRepaint: true });
      setDirty(false);
      persistTabs();

      touchExplorerEntry(d.path, "file", baseName(d.path), {
        modified: Date.now(),
        openable: true,
        extension: ".vixnote",
      });

      await loadExplorerForDocumentPath(d.path);
      if (!silent) {
        setMessage("");
      }
    } catch (error) {
      if (isMissingNoteError(error)) {
        removeTabState(path);
        console.debug(`[Vix Note] Removed stale tab: ${path}`);
        if (state.activeTabPath) {
          await openNotePath(state.activeTabPath, { silent: true });
        } else {
          clearEditorNoOpenNote();
          await loadDirectory(".", { silent: true, force: true });
        }
        return;
      }
      setMessage(error.message || "Failed to open note.", "error");
    } finally {
      setBusy(false);
    }
  }

  /* ==========================================================
   * Delete / rename
   * ======================================================== */
  async function deletePath(path, options = {}) {
    if (!path) return;
    const recursive = !!options.recursive;
    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/delete", {
        method: "POST",
        body: JSON.stringify({ path, recursive }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to delete path.");
      }

      const deletedActiveDocument =
        result.currentDeleted ||
        normalizeExplorerPath(path) === normalizeExplorerPath(currentDocPath());

      state.explorer.entries.delete(normalizeExplorerPath(path));

      const prefix = `${normalizeExplorerPath(path)}/`;
      for (const key of Array.from(state.explorer.entries.keys())) {
        if (key.startsWith(prefix)) {
          state.explorer.entries.delete(key);
        }
      }

      state.explorer.loadedDirs.delete(normalizeExplorerPath(path));
      state.explorer.expandedDirs.delete(normalizeExplorerPath(path));

      await loadDirectory(parentPath(path), { silent: true, force: true });

      if (deletedActiveDocument) {
        state.tabs = state.tabs.filter((tab) => {
          return (
            normalizeExplorerPath(tab.path) !== normalizeExplorerPath(path)
          );
        });
        state.activeTabPath = state.tabs.length ? state.tabs[0].path : null;
        if (state.activeTabPath) {
          await openNotePath(state.activeTabPath, { silent: true });
        } else {
          state.document = null;
          state.selectedId = null;
          await loadDocument();
        }
      }

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();
      clearMessageQuietly();
    } catch (error) {
      setMessage(error.message || "Failed to delete path.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function startInlineRename(path, type = "file") {
    const normalized = normalizeExplorerPath(path);
    cancelInlineCreate();
    state.explorer.rename = {
      path: normalized,
      type,
      error: null,
    };
    renderExplorer();
    requestAnimationFrame(() => {
      const input = $(".vn-Tree__input--rename");
      if (!input) return;
      input.focus();
      const value = input.value;
      const dot = value.lastIndexOf(".");
      if (type === "file" && dot > 0) {
        input.setSelectionRange(0, dot);
      } else {
        input.select();
      }
    });
  }

  function cancelInlineRename() {
    if (!state.explorer.rename) return;
    state.explorer.rename = null;
    renderExplorer();
  }

  async function commitInlineRename(rawName) {
    const ren = state.explorer.rename;
    if (!ren) return;

    const oldPath = ren.path;
    const type = ren.type;

    let newName = String(rawName || "")
      .trim()
      .replaceAll("\\", "/");
    newName = baseName(newName);

    if (!newName || newName === baseName(oldPath)) {
      cancelInlineRename();
      return;
    }

    if (
      type === "file" &&
      oldPath.endsWith(".vixnote") &&
      !newName.endsWith(".vixnote")
    ) {
      newName = `${newName}.vixnote`;
    }

    const newPathOptimistic = joinExplorerPath(parentPath(oldPath), newName);
    if (state.explorer.entries.has(normalizeExplorerPath(newPathOptimistic))) {
      ren.error = "A file or folder with that name already exists.";
      renderExplorer();
)VIXNOTE");

    value.append(R"VIXNOTE(      requestAnimationFrame(() => {
        const input = $(".vn-Tree__input--rename");
        if (input) input.focus();
      });
      return;
    }

    state.explorer.rename = null;
    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/rename", {
        method: "POST",
        body: JSON.stringify({ path: oldPath, newName }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to rename path.");
      }

      const newPath = normalizeExplorerPath(
        result.newPath || joinExplorerPath(parentPath(oldPath), newName),
      );

      applyPathMoveToState(oldPath, newPath, type);

      await retitleActiveDocumentAfterRename(oldPath, newPath, type);

      await loadDirectory(parentPath(newPath), { silent: true, force: true });

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();
      clearMessageQuietly();
    } catch (error) {
      state.explorer.rename = {
        path: oldPath,
        type,
        error:
          error.message || "Path not found. Save the note before renaming it.",
      };

      renderExplorer();

      requestAnimationFrame(() => {
        const input = $(".vn-Tree__input--rename");
        if (input) {
          input.focus();
          input.select();
        }
      });

      setMessage(
        error.message || "Path not found. Save the note before renaming it.",
        "error",
      );
    } finally {
      setBusy(false);
    }
  }

  /*
   * Shared state surgery for rename + move. Updates explorer entries,
   * loaded/expanded dir sets, selection, tabs and active document path
   * so that an old path (file or dir) becomes a new path everywhere.
   */
  function applyPathMoveToState(oldPath, newPath, type) {
    const oldNorm = normalizeExplorerPath(oldPath);
    const newNorm = normalizeExplorerPath(newPath);
    if (oldNorm === newNorm) return;

    const oldEntry = state.explorer.entries.get(oldNorm);
    state.explorer.entries.delete(oldNorm);

    const oldPrefix = `${oldNorm}/`;
    const newPrefix = `${newNorm}/`;
    for (const [key, entry] of Array.from(state.explorer.entries.entries())) {
      if (key.startsWith(oldPrefix)) {
        const movedChildPath = normalizeExplorerPath(
          newPrefix + key.slice(oldPrefix.length),
        );
        state.explorer.entries.delete(key);
        state.explorer.entries.set(movedChildPath, {
          ...entry,
          path: movedChildPath,
          title: baseName(movedChildPath),
        });
      }
    }

    state.explorer.entries.set(newNorm, {
      ...(oldEntry || {}),
      path: newNorm,
      type,
      title: baseName(newNorm),
      modified: Date.now(),
      openable: type === "file" ? newNorm.endsWith(".vixnote") : false,
      extension: type === "file" ? ".vixnote" : "",
    });

    if (state.explorer.loadedDirs.has(oldNorm)) {
      state.explorer.loadedDirs.delete(oldNorm);
      state.explorer.loadedDirs.add(newNorm);
    }
    if (state.explorer.expandedDirs.has(oldNorm)) {
      state.explorer.expandedDirs.delete(oldNorm);
      state.explorer.expandedDirs.add(newNorm);
    }
    // Re-map any loaded/expanded descendants of a moved directory.
    for (const set of [
      state.explorer.loadedDirs,
      state.explorer.expandedDirs,
    ]) {
      for (const dir of Array.from(set)) {
        if (dir.startsWith(oldPrefix)) {
          set.delete(dir);
          set.add(
            normalizeExplorerPath(newPrefix + dir.slice(oldPrefix.length)),
          );
        }
      }
    }

    if (state.explorer.selectedDirPath === oldNorm) {
      state.explorer.selectedDirPath = newNorm;
    }
    if (state.explorer.currentPath === oldNorm) {
      state.explorer.currentPath = newNorm;
    }

    // Tabs: exact match (file move/rename) and prefixed (dir move/rename).
    for (const tab of state.tabs) {
      const tabPath = normalizeExplorerPath(tab.path);
      if (tabPath === oldNorm) {
        const oldTitle = noteTitleFromPath(oldNorm);
        const newTitle = noteTitleFromPath(newNorm);
        tab.path = newNorm;
        if (
          !tab.title ||
          tab.title === baseName(oldNorm) ||
          tab.title === oldTitle
        ) {
          tab.title = newTitle;
        }
      } else if (tabPath.startsWith(oldPrefix)) {
        tab.path = normalizeExplorerPath(
          newPrefix + tabPath.slice(oldPrefix.length),
        );
      }
    }

    if (state.activeTabPath) {
      const activeNorm = normalizeExplorerPath(state.activeTabPath);
      if (activeNorm === oldNorm) {
        state.activeTabPath = newNorm;
      } else if (activeNorm.startsWith(oldPrefix)) {
        state.activeTabPath = normalizeExplorerPath(
          newPrefix + activeNorm.slice(oldPrefix.length),
        );
      }
    }

    // Active document path (keep the document open after a move/rename).
    if (
      state.document &&
      normalizeExplorerPath(state.document.path) === oldNorm
    ) {
      state.document.path = newNorm;
    } else if (
      state.document &&
      normalizeExplorerPath(state.document.path).startsWith(oldPrefix)
    ) {
      state.document.path = normalizeExplorerPath(
        newPrefix +
          normalizeExplorerPath(state.document.path).slice(oldPrefix.length),
      );
    }
  }

  async function confirmDelete(label, type = "file") {
    return showModalConfirm({
      title: "Delete from disk",
      body:
        type === "dir"
          ? `Delete folder “${escapeHtml(label)}” from disk? Empty folders are deleted directly. Non-empty folders need recursive delete.`
          : `Delete file “${escapeHtml(label)}” from disk? This cannot be undone from Vix Note.`,
      confirm: "Delete",
      danger: true,
    });
  }

  /* ==========================================================
   * Explorer drag and drop (move files / folders)
   * ======================================================== */

  // A move is allowed when:
  //  - we are not moving the root
  //  - source and destination differ
  //  - a folder is not dropped into itself or any of its descendants
  //  - the file/folder is not already a direct child of the target
  function canMoveExplorerPath(sourcePath, sourceType, targetDir) {
    const src = normalizeExplorerPath(sourcePath);
    const dir = normalizeExplorerPath(targetDir);

    if (!src || src === ".") return false; // never move the root
    if (!dir) return false;

    // Dropping a folder into itself.
    if (sourceType === "dir" && dir === src) return false;

    // Dropping a folder into one of its own descendants.
    if (sourceType === "dir" && dir.startsWith(`${src}/`)) return false;

    // No-op: already a direct child of the target directory.
    if (parentPath(src) === dir) return false;

    return true;
  }

  async function moveExplorerPath(sourcePath, targetDir) {
    const src = normalizeExplorerPath(sourcePath);
    const dir = normalizeExplorerPath(targetDir);

    const entry = state.explorer.entries.get(src);
    const type = entry ? entry.type : src.endsWith(".vixnote") ? "file" : "dir";

    if (!canMoveExplorerPath(src, type, dir)) {
      return;
    }

    const oldParent = parentPath(src);
    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/move", {
        method: "POST",
        body: JSON.stringify({ path: src, directory: dir }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to move path.");
      }

      const newPath = normalizeExplorerPath(
        result.newPath || joinExplorerPath(dir, baseName(src)),
      );

      // Reflect the move across explorer entries, tabs and active document.
      applyPathMoveToState(src, newPath, type);

      if (dir !== ".") {
        state.explorer.expandedDirs.add(dir);
      }

      // Silently reload both the old and the new parent directories.
      await loadDirectory(oldParent, { silent: true, force: true });
      await loadDirectory(dir, { silent: true, force: true });

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();
      persistTabs();
      // No success flash: a move is a quiet action.
      clearMessageQuietly();
    } catch (error) {
      setMessage(error.message || "Failed to move path.", "error");
    } finally {
      setBusy(false);
    }
  }

  function clearExplorerDropTargets() {
    for (const el of $all(".vn-Tree__row.is-drop-target")) {
      el.classList.remove("is-drop-target");
    }
    const listEl = $(sel.explorerList);
    if (listEl) listEl.classList.remove("is-root-drop-target");
  }

  function explorerDropDirForRow(row) {
    // Returns the target directory for a drop on this row, or null if the
    // row is not a valid drop target.
    if (!row) return null;
    const path = row.getAttribute("data-tree-path");
    const type = row.getAttribute("data-tree-type");
    if (!path) return null;
    // Only folders (and the root row ".") are valid drop targets.
    if (type === "dir") return normalizeExplorerPath(path);
    return null;
  }

  /* ==========================================================
   * Outputs (client-side display only)
   * ======================================================== */
  function clearCellOutput(id) {
    const cellEl = cellElById(id);
    if (!cellEl) return;
    const oa = $(".vn-OutputArea", cellEl);
    if (oa) oa.remove();
    const cell = findCell(id);
    if (cell) cell.outputs = [];
    state.diagnostics.items = state.diagnostics.items.filter(
      (d) => d.cellId !== String(id),
    );
    recomputeDiagnosticsState();
    renderProblems();
  }
  function clearAllOutputs() {
    for (const cell of cells()) cell.outputs = [];
    for (const oa of $all(".vn-OutputArea")) oa.remove();
    clearDiagnostics();
    setMessage("Outputs cleared from view.", "info");
  }

  async function restartKernel(runAfter = false) {
    setKernel("busy");
    setMessage("Restarting kernel…", "info");
    setTimeout(async () => {
      setKernel("idle");
      if (runAfter) await runAll();
      else setMessage("Kernel restarted.", "info");
    }, 400);
  }

  async function restoreFirstAvailableTab() {
    while (state.tabs.length) {
      const path = state.activeTabPath || state.tabs[0].path;

      try {
        await openNotePath(path, { silent: true });

        if (
          !state.document ||
          normalizeExplorerPath(currentDocPath()) !==
            normalizeExplorerPath(path)
        ) {
          removeTabState(path);
          continue;
        }

        await loadExplorerForDocumentPath(path);
        return true;
      } catch (error) {
        removeTabState(path);
      }
    }

    clearEditorNoOpenNote();
    await loadDirectory(".", { silent: true, force: true });
    return false;
  }

  function isStartupScratchDocument(doc) {
    if (!doc) return true;

    const title = String(doc.title || "")
      .trim()
      .toLowerCase();
    const path = normalizeExplorerPath(doc.path || "").toLowerCase();
    const list = Array.isArray(doc.cells) ? doc.cells : [];
    const firstSource = String(list[0]?.source || "").toLowerCase();

    return (
      title === "tmp" &&
      (path === "untitled.vixnote" || path === "untitled") &&
      firstSource.includes("start writing your note here")
    );
  }

  /* ==========================================================
   * Load
   * ======================================================== */
  async function loadDocument() {
    setMessage("");
    const restored = restorePersistedTabs();

    if (restored) {
      renderOpenTabs();
      renderTabsBar();
      if (state.activeTabPath) {
        const ok = await restoreFirstAvailableTab();
        if (ok) setKernel("idle");
        return;
      }
      clearEditorNoOpenNote();
      await loadDirectory(".", { silent: true, force: true });
      return;
    }

    try {
      const doc = await api("/api/document");
)VIXNOTE");

    value.append(R"VIXNOTE(      const d = unwrapDocument(doc);

      if (isStartupScratchDocument(d)) {
        clearEditorNoOpenNote();

        await loadDirectory(".", {
          silent: true,
          force: true,
        });

        setKernel("idle");
        return;
      }

      if (d && d.path && !state.activeTabPath) {
        openTab(d.path, documentDisplayTitle(d));
      }

      renderDocument(doc);
      setKernel("idle");

      if (d && d.path) {
        await loadExplorerForDocumentPath(d.path);
      } else {
        await loadDirectory(".", {
          silent: true,
          force: true,
        });
      }
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to load note document.", "error");
      renderEmpty("Unable to load the note document.");
      await loadDirectory(".", { silent: true, force: true });
    }
  }

  /* ==========================================================
   * Explorer (backend-backed model)
   * ======================================================== */
  function touchExplorerEntry(path, type, title, options = {}) {
    const normalized = normalizeExplorerPath(path);
    if (!normalized) return;

    const existing = state.explorer.entries.get(normalized);

    state.explorer.entries.set(normalized, {
      path: normalized,
      type,
      title: title || (existing && existing.title) || baseName(normalized),
      modified:
        options.modified ?? (existing && existing.modified) ?? Date.now(),
      openable: options.openable ?? (existing && existing.openable) ?? false,
      extension: options.extension ?? (existing && existing.extension) ?? "",
      size: options.size ?? (existing && existing.size) ?? 0,
    });

    if (normalized !== ".") {
      const parts = normalized.split("/");
      parts.pop();
      let acc = "";
      for (const part of parts) {
        if (!part) continue;
        acc = acc ? `${acc}/${part}` : part;
        if (!state.explorer.entries.has(acc)) {
          state.explorer.entries.set(acc, {
            path: acc,
            type: "dir",
            title: part,
            modified: 0,
            openable: false,
            extension: "",
            size: 0,
          });
        }
      }
    }
  }

  function mergeDirectoryList(payload, requestedPath) {
    const dirPath = normalizeExplorerPath(
      requestedPath || payload?.path || ".",
    );

    touchExplorerEntry(dirPath, "dir", baseName(dirPath), {
      modified: 0,
      openable: false,
    });

    state.explorer.loadedDirs.add(dirPath);
    state.explorer.expandedDirs.add(dirPath);

    const entries = Array.isArray(payload?.entries) ? payload.entries : [];

    // Remove stale children of this directory that no longer exist on disk.
    const incomingPaths = new Set();
    for (const entry of entries) {
      const name = String(
        entry.name || baseName(entry.path || "") || "",
      ).trim();
      if (!name) continue;
      let path;
      if (dirPath === ".") {
        path = normalizeExplorerPath(entry.path || name);
      } else {
        const rawPath = normalizeExplorerPath(entry.path || name);
        if (rawPath === dirPath || rawPath.startsWith(`${dirPath}/`)) {
          path = rawPath;
        } else {
          path = normalizeExplorerPath(`${dirPath}/${name}`);
        }
      }
      incomingPaths.add(path);
    }

    for (const key of Array.from(state.explorer.entries.keys())) {
      if (key === dirPath) continue;
      if (parentPath(key) !== dirPath) continue;
      if (!incomingPaths.has(key)) {
        // Drop the stale entry and any of its descendants.
        state.explorer.entries.delete(key);
        const prefix = `${key}/`;
        for (const child of Array.from(state.explorer.entries.keys())) {
          if (child.startsWith(prefix)) state.explorer.entries.delete(child);
        }
      }
    }

    for (const entry of entries) {
      const name = String(
        entry.name || baseName(entry.path || "") || "",
      ).trim();
      if (!name) continue;

      let path;
      if (dirPath === ".") {
        path = normalizeExplorerPath(entry.path || name);
      } else {
        const rawPath = normalizeExplorerPath(entry.path || name);
        if (rawPath === dirPath || rawPath.startsWith(`${dirPath}/`)) {
          path = rawPath;
        } else {
          path = normalizeExplorerPath(`${dirPath}/${name}`);
        }
      }

      if (!path || path === dirPath) continue;

      const type = entry.type === "dir" ? "dir" : "file";
      touchExplorerEntry(path, type, name || baseName(path), {
        modified: Number(entry.modified || 0),
        openable: !!entry.openable,
        extension: entry.extension || "",
        size: Number(entry.size || 0),
      });
    }
  }

  async function loadDirectory(path = ".", options = {}) {
    const dirPath = normalizeExplorerPath(path);
    const force = !!options.force;
    const silent = !!options.silent;

    if (!force && state.explorer.loadedDirs.has(dirPath)) {
      state.explorer.expandedDirs.add(dirPath);
      renderExplorer();
      return;
    }

    state.explorer.loadingPath = dirPath;
    if (!silent) clearMessageQuietly();
    renderExplorer();

    try {
      const payload = await api("/api/directory/list", {
        method: "POST",
        body: JSON.stringify({ path: dirPath }),
      });

      if (!payload || payload.ok === false) {
        throw new Error(payload?.error || "Failed to list directory");
      }

      state.explorer.currentPath = dirPath;
      mergeDirectoryList(payload, dirPath);
      if (!silent) clearMessageQuietly();
    } catch (error) {
      if (!silent) {
        setMessage(error.message || "Failed to load directory.", "error");
      }
    } finally {
      state.explorer.loadingPath = null;
      renderExplorer();
    }
  }

  async function refreshExplorer(path = ".") {
    const dirPath = normalizeExplorerPath(path || ".");
    state.explorer.draft = null;
    state.explorer.rename = null;
    state.explorer.entries.clear();
    state.explorer.loadedDirs.clear();
    state.explorer.expandedDirs.clear();
    state.explorer.expandedDirs.add(".");
    await loadDirectory(dirPath, { force: true, silent: false });
  }

  function fileIcon() {
    return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3v6h6"/></svg>';
  }

  function dirIcon(entry) {
    const path = normalizeExplorerPath(entry && entry.path);
    const loaded = state.explorer.loadedDirs.has(path);
    const expanded = state.explorer.expandedDirs.has(path);

    if (loaded && expanded) {
      return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1H3V7z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 10h18l-2 8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-.8L3 10z"/></svg>';
    }
    return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"/></svg>';
  }

  function draftIcon(kind) {
    return kind === "dir"
      ? '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"/></svg>'
      : fileIcon();
  }

  function draftRowHtml(row) {
    const draft = state.explorer.draft;
    const isDir = draft.kind === "dir";
    const placeholder = isDir ? "new-folder" : "new-note.vixnote";
    const defaultValue = isDir ? "" : suggestedNoteName();
    const errorAttr = draft.error ? " has-error" : "";
    const errorHtml = draft.error
      ? `<span class="vn-Tree__inputError">${escapeHtml(draft.error)}</span>`
      : "";
    const hint = isDir ? "Enter to create folder" : "Enter to create note";

    return `
    <div
      class="vn-Tree__row vn-Tree__row--draft"
      style="--depth:${Number(row.depth || 0)}"
    >
      <span class="vn-Tree__chevron"></span>
      ${draftIcon(draft.kind)}

      <div class="vn-Tree__inputWrap">
        <input
          class="vn-Tree__input${errorAttr}"
          type="text"
          data-tree-input
          value="${escapeHtml(defaultValue)}"
          placeholder="${escapeHtml(placeholder)}"
          autocomplete="off"
          spellcheck="false"
        />
        <span class="vn-Tree__inputHint">${escapeHtml(hint)}</span>
        ${errorHtml}
      </div>
    </div>`;
  }

  function renameRowHtml(row) {
    const ren = state.explorer.rename;
    const errorAttr = ren.error ? " has-error" : "";
    const errorHtml = ren.error
      ? `<span class="vn-Tree__inputError">${escapeHtml(ren.error)}</span>`
      : "";
    const icon = row.type === "dir" ? dirIcon(row) : fileIcon();
    const chevron =
      row.type === "dir"
        ? `<span class="vn-Tree__chevron">${
            state.explorer.expandedDirs.has(normalizeExplorerPath(row.path))
              ? "▾"
              : "▸"
          }</span>`
        : `<span class="vn-Tree__chevron"></span>`;

    return `
      <div
        class="vn-Tree__row vn-Tree__row--rename"
        style="--depth:${Number(row.depth || 0)}"
      >
        ${chevron}
        ${icon}
       <div class="vn-Tree__inputWrap">
          <input
            class="vn-Tree__input vn-Tree__input--rename${errorAttr}"
            type="text"
            data-tree-rename-input
            value="${escapeHtml(baseName(row.path))}"
            autocomplete="off"
            spellcheck="false"
          />
          <span class="vn-Tree__inputHint">Enter to rename</span>
          ${errorHtml}
        </div>
      </div>`;
  }

  function renderExplorer() {
    const listEl = $(sel.explorerList);
    if (!listEl) return;

    const entries = explorerTreeRows();
    const loadingPath = state.explorer.loadingPath;
    const renamePath = state.explorer.rename
      ? normalizeExplorerPath(state.explorer.rename.path)
      : null;

    const realCount = entries.filter((e) => !e.isDraft).length;
    setText(sel.explorerCount, String(realCount));

    if (loadingPath && !entries.length && !state.explorer.draft) {
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        Loading ${escapeHtml(loadingPath)}…
      </p>`;
      return;
    }

    if (!entries.length) {
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        No notes found. Create one with <strong>New note</strong> or refresh the explorer.
      </p>`;
      return;
    }

    listEl.innerHTML = entries
      .map((e) => {
        if (e.isDraft) {
          return draftRowHtml(e);
        }

        const path = normalizeExplorerPath(e.path);

        if (renamePath && path === renamePath) {
          return renameRowHtml(e);
        }

        const active =
          e.type === "file" && path === state.activeTabPath ? " is-active" : "";
        const loading =
          e.type === "dir" && path === loadingPath ? " is-loading" : "";
        const meta =
          e.type === "file"
            ? `<span class="vn-Tree__meta">${escapeHtml(relativeTime(entryModified(e)))}</span>`
            : "";
        const icon = e.type === "dir" ? dirIcon(e) : fileIcon();
        const depth = Number(e.depth || 0);
        const expanded = state.explorer.expandedDirs.has(path);

        // The root row and every file/dir except root are draggable.
        // All folders (and the root) are drop targets.
        const draggable = path !== "." ? ' draggable="true"' : "";

        return `
        <div
)VIXNOTE");

    value.append(R"VIXNOTE(          class="vn-Tree__row${active}${loading}${expanded ? " is-expanded" : ""}"
          data-tree-path="${escapeHtml(path)}"
          data-tree-type="${escapeHtml(e.type)}"
          data-tree-openable="${e.openable ? "true" : "false"}"
          style="--depth:${depth}"
          tabindex="0"${draggable}
        >
          ${
            e.type === "dir"
              ? `<span class="vn-Tree__chevron">${expanded ? "▾" : "▸"}</span>`
              : `<span class="vn-Tree__chevron"></span>`
          }
          ${icon}
          <span class="vn-Tree__label" title="${escapeHtml(path)}">
            ${escapeHtml(e.type === "file" ? baseName(path) : e.title || baseName(path))}
          </span>
          ${meta}
          <button
            class="vn-Tree__menuBtn"
            type="button"
            data-tree-menu="${escapeHtml(path)}"
            title="More actions"
            aria-label="More actions"
          >⋯</button>
        </div>`;
      })
      .join("");
  }

  function persistTabs() {
    try {
      const payload = {
        activeTabPath: state.activeTabPath,
        tabs: state.tabs.map((tab) => ({
          path: tab.path,
          title: tab.title || baseName(tab.path),
          dirty: false,
        })),
      };
      localStorage.setItem(TABS_STORAGE_KEY, JSON.stringify(payload));
    } catch (_) {}
  }

  function restorePersistedTabs() {
    try {
      const raw = localStorage.getItem(TABS_STORAGE_KEY);
      if (!raw) return false;
      const payload = JSON.parse(raw);
      const tabs = Array.isArray(payload.tabs) ? payload.tabs : [];

      state.tabs = tabs
        .filter((tab) => tab && tab.path)
        .map((tab) => ({
          path: normalizeExplorerPath(tab.path),
          title: tab.title || baseName(tab.path),
          dirty: false,
        }));

      state.activeTabPath =
        payload.activeTabPath &&
        state.tabs.some(
          (tab) => tab.path === normalizeExplorerPath(payload.activeTabPath),
        )
          ? normalizeExplorerPath(payload.activeTabPath)
          : state.tabs.length
            ? state.tabs[0].path
            : null;

      return true;
    } catch (_) {
      state.tabs = [];
      state.activeTabPath = null;
      return false;
    }
  }

  function clearPersistedTabs() {
    try {
      localStorage.setItem(
        TABS_STORAGE_KEY,
        JSON.stringify({ activeTabPath: null, tabs: [] }),
      );
    } catch (_) {}
  }

  /* ==========================================================
   * Tabs
   * ======================================================== */
  function activeTab() {
    return state.tabs.find((t) => t.path === state.activeTabPath) || null;
  }

  function openTab(path, title) {
    if (!path) return;
    const normalized = normalizeExplorerPath(path);
    let tab = state.tabs.find((t) => t.path === normalized);
    if (!tab) {
      tab = {
        path: normalized,
        title: title || baseName(normalized),
        dirty: false,
      };
      state.tabs.push(tab);
    } else if (title) {
      tab.title = title;
    }
    state.activeTabPath = normalized;
    persistTabs();
    renderOpenTabs();
    renderTabsBar();
  }

  function syncActiveTab(doc) {
    if (!doc || !doc.path) return;
    const title = documentDisplayTitle(doc);
    if (state.activeTabPath !== doc.path) {
      openTab(doc.path, title);
    } else {
      const tab = activeTab();
      if (tab) tab.title = title;
    }
  }

  async function switchTab(path) {
    if (!path || path === state.activeTabPath) return;
    if (isDirty()) {
      const proceed = await showModalConfirm({
        title: "Unsaved changes",
        body: `“${escapeHtml(activeTab().title)}” has unsaved changes. Switch anyway? Your unsaved edits in the editor may be replaced.`,
        confirm: "Switch tab",
        danger: true,
      });
      if (!proceed) return;
    }
    state.activeTabPath = path;
    persistTabs();
    await openNotePath(path);
  }

  async function closeTab(path) {
    const normalized = normalizeExplorerPath(path);
    const idx = state.tabs.findIndex((t) => t.path === normalized);
    if (idx < 0) return;

    const tab = state.tabs[idx];
    if (tab.dirty) {
      const proceed = await showModalConfirm({
        title: "Close tab",
        body: `“${escapeHtml(tab.title)}” has unsaved changes. Close without saving?`,
        confirm: "Close tab",
        danger: true,
      });
      if (!proceed) return;
    }

    state.tabs.splice(idx, 1);

    if (state.activeTabPath === normalized) {
      const next = state.tabs[idx] || state.tabs[idx - 1] || null;
      if (next) {
        state.activeTabPath = next.path;
        persistTabs();
        await openNotePath(next.path);
      } else {
        state.activeTabPath = null;
        clearPersistedTabs();
        clearEditorNoOpenNote();
        return;
      }
    } else {
      persistTabs();
    }

    renderOpenTabs();
    renderTabsBar();
  }

  // Reorder tabs without touching disk. position is "before" | "after".
  function reorderTabs(sourcePath, targetPath, position) {
    const src = normalizeExplorerPath(sourcePath);
    const tgt = normalizeExplorerPath(targetPath);
    if (src === tgt) return;

    const fromIndex = state.tabs.findIndex(
      (t) => normalizeExplorerPath(t.path) === src,
    );
    if (fromIndex < 0) return;

    // Pull the source tab out (keeps its dirty state, since it's the object).
    const [moved] = state.tabs.splice(fromIndex, 1);

    let targetIndex = state.tabs.findIndex(
      (t) => normalizeExplorerPath(t.path) === tgt,
    );
    if (targetIndex < 0) {
      // Target vanished; put it back where it was.
      state.tabs.splice(fromIndex, 0, moved);
      return;
    }

    const insertAt = position === "after" ? targetIndex + 1 : targetIndex;
    state.tabs.splice(insertAt, 0, moved);

    persistTabs();
    renderTabsBar();
  }

  function tabDot(tab) {
    return tab.dirty
      ? '<span class="vn-Tab__dot" title="Unsaved changes"></span>'
      : "";
  }

  function renderTabsBar() {
    const bar = $(sel.tabsBar);
    if (!bar) return;
    if (!state.tabs.length) {
      bar.innerHTML = `<div class="vn-TabsBar__empty">No open notes</div>`;
      return;
    }
    bar.innerHTML = state.tabs
      .map((t) => {
        const active = t.path === state.activeTabPath ? " is-active" : "";
        return `<div class="vn-Tab${active}" data-tab-path="${escapeHtml(t.path)}" title="${escapeHtml(t.path)}" draggable="true">
            ${tabDot(t)}
            <span class="vn-Tab__label">${escapeHtml(t.title)}</span>
            <button class="vn-Tab__close" type="button" data-tab-close="${escapeHtml(t.path)}" aria-label="Close tab">×</button>
          </div>`;
      })
      .join("");
  }

  /* ==========================================================
   * Diagnostics / Problems
   * ======================================================== */
  const DIAGNOSTIC_SEVERITY = {
    compiler_error: "error",
    runtime_error: "error",
    error: "error",
    stderr: "error",
    hint: "hint",
  };

  let diagnosticSeq = 0;

  function severityForOutputKind(kind) {
    return DIAGNOSTIC_SEVERITY[normalizeKind(kind)] || null;
  }

  function cellLabelFor(cellId) {
    const cell = findCell(cellId);
    if (cell && cell.title) return String(cell.title);
    const idx = cellIndex(cellId);
    if (idx >= 0) return `Cell ${idx + 1}`;
    return String(cellId || "cell");
  }

  function diagnosticsFromResult(result, cellId) {
    const outputs = Array.isArray(result?.outputs) ? result.outputs : [];
    const items = [];
    for (const out of outputs) {
      const severity = severityForOutputKind(out.kind);
      if (!severity) continue;
      const message = String(out.content ?? "").trim();
      if (!message) continue;
      items.push({
        id: `diag-${++diagnosticSeq}`,
        cellId: String(cellId),
        cellLabel: cellLabelFor(cellId),
        severity,
        kind: normalizeKind(out.kind),
        message,
        ts: Date.now(),
      });
    }
    return items;
  }

  function setCellDiagnostics(cellId, result) {
    const id = String(cellId);
    state.diagnostics.items = state.diagnostics.items.filter(
      (d) => d.cellId !== id,
    );
    const fresh = diagnosticsFromResult(result, id);
    state.diagnostics.items.push(...fresh);
    recomputeDiagnosticsState();
    renderProblems();
  }

  function setRunAllDiagnostics(runResult) {
    const results = Array.isArray(runResult?.results) ? runResult.results : [];
    const executable = cells().filter((c) => isCodeKind(c.kind));
    state.diagnostics.items = [];
    const n = Math.min(results.length, executable.length);
    for (let i = 0; i < n; i++) {
      const cell = executable[i];
      const items = diagnosticsFromResult(results[i], cell.id);
      state.diagnostics.items.push(...items);
    }
    recomputeDiagnosticsState();
    renderProblems();
  }

  function clearDiagnostics() {
    state.diagnostics.items = [];
    state.diagnostics.status = "idle";
    state.diagnostics.byCell = new Map();
    renderProblems();
    refreshCellProblemBadges();
  }

  function setDiagnosticsRunning() {
    state.diagnostics.status = "running";
    renderProblems();
  }

  function recomputeDiagnosticsState() {
    state.diagnostics.items = state.diagnostics.items.filter(
      (d) => findCell(d.cellId) !== null,
    );

    const byCell = new Map();
    let errorCount = 0;
    for (const d of state.diagnostics.items) {
      if (d.severity === "error") {
        errorCount++;
        byCell.set(d.cellId, (byCell.get(d.cellId) || 0) + 1);
      }
    }
    state.diagnostics.byCell = byCell;
    state.diagnostics.status = errorCount > 0 ? "failed" : "success";

    refreshCellProblemBadges();
  }

  function errorDiagnosticCount() {
    return state.diagnostics.items.filter((d) => d.severity === "error").length;
  }

  const PROBLEM_ICONS = {
    error:
      '<svg viewBox="0 0 24 24" class="vn-Problem__icon" aria-hidden="true"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.7"/><path d="M9 9l6 6M15 9l-6 6" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/></svg>',
    hint: '<svg viewBox="0 0 24 24" class="vn-Problem__icon" aria-hidden="true"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.7"/><path d="M12 11v5" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/><circle cx="12" cy="7.8" r="1" fill="currentColor"/></svg>',
  };

  function firstMessageLine(message) {
    const line = String(message || "").split(/\r?\n/)[0] || "";
    return line.length > 160 ? `${line.slice(0, 159)}…` : line;
  }

  function renderProblems() {
    const errorCount = errorDiagnosticCount();
    const total = state.diagnostics.items.length;
    const status = state.diagnostics.status;

    setText(sel.problemsCount, String(errorCount));
    setText(sel.statusProblemsCount, String(errorCount));

    const badge = $(sel.problemsBadge);
    if (badge) {
      if (errorCount > 0) {
        badge.hidden = false;
        badge.textContent = String(errorCount > 99 ? "99+" : errorCount);
      } else {
        badge.hidden = true;
      }
    }

    const statusBtn = $(sel.statusProblems);
    if (statusBtn) {
      statusBtn.classList.toggle("has-errors", errorCount > 0);
    }

    const summary = $(sel.problemsSummary);
    const summaryText = $(sel.problemsSummaryText);
    if (summary) summary.setAttribute("data-state", status);
    if (summaryText) {
      if (status === "running") {
        summaryText.textContent = "Running…";
      } else if (errorCount > 0) {
        summaryText.textContent =
          errorCount === 1 ? "1 problem found" : `${errorCount} problems found`;
      } else if (status === "success") {
        summaryText.textContent = "No problems — last run succeeded";
      } else {
)VIXNOTE");

    value.append(R"VIXNOTE(        summaryText.textContent = "No problems detected";
      }
    }

    const listEl = $(sel.problemsList);
    if (!listEl) return;

    if (status === "running" && !total) {
      listEl.innerHTML = `
        <div class="vn-Problems__loading">
          <span class="vn-spinner" aria-hidden="true"></span>
          Running cells…
        </div>`;
      return;
    }

    if (!total) {
      listEl.innerHTML = `
        <p class="vn-Tree__empty">
          No problems detected. Run a C++ or Reply cell to see compiler
          and runtime diagnostics here.
        </p>`;
      return;
    }

    const groups = new Map();
    for (const d of state.diagnostics.items) {
      if (!groups.has(d.cellId)) groups.set(d.cellId, []);
      groups.get(d.cellId).push(d);
    }

    const runningClass = status === "running" ? " is-stale" : "";

    const rows = [];
    for (const [cellId, items] of groups) {
      items.sort((a, b) => {
        if (a.severity !== b.severity) return a.severity === "error" ? -1 : 1;
        return a.ts - b.ts;
      });
      const label = cellLabelFor(cellId);
      rows.push(
        `<div class="vn-Problems__group${runningClass}">
           <button class="vn-Problems__groupHead" type="button" data-problem-goto="${escapeHtml(cellId)}" title="Go to ${escapeHtml(label)}">
             <svg viewBox="0 0 24 24" class="vn-Problems__cellIcon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3v6h6"/></svg>
             <span class="vn-Problems__cellName">${escapeHtml(label)}</span>
             <span class="vn-Problems__cellCount">${items.length}</span>
           </button>
           ${items
             .map(
               (d) => `
             <button
               class="vn-Problem vn-Problem--${escapeHtml(d.severity)}"
               type="button"
               data-problem-goto="${escapeHtml(d.cellId)}"
               title="${escapeHtml(firstMessageLine(d.message))}"
             >
               ${PROBLEM_ICONS[d.severity] || PROBLEM_ICONS.error}
               <span class="vn-Problem__body">
                 <span class="vn-Problem__message">${escapeHtml(firstMessageLine(d.message))}</span>
                 <span class="vn-Problem__kind">${escapeHtml(d.kind.replace(/_/g, " "))}</span>
               </span>
             </button>`,
             )
             .join("")}
         </div>`,
      );
    }

    listEl.innerHTML = rows.join("");
  }

  function refreshCellProblemBadges() {
    for (const el of $all(".vn-Cell")) {
      const id = el.dataset.cellId;
      const hasError = state.diagnostics.byCell.get(id) > 0;
      el.classList.toggle("has-problem", !!hasError);
    }
  }

  function gotoCellFromDiagnostic(cellId) {
    const id = String(cellId);

    if (!findCell(id)) {
      setMessage("That cell no longer exists.", "warning");
      return;
    }

    selectCell(id, { edit: false });

    const el = cellElById(id);

    if (el && typeof el.scrollIntoView === "function") {
      el.scrollIntoView({ block: "center", behavior: "smooth" });
    }
  }

  function renderOpenTabs() {
    renderTabsBar();
  }

  /* ==========================================================
   * Activity bar (Explorer / Problems)
   * ======================================================== */
  function renderActivityBar() {
    for (const b of $all("[data-activity]")) {
      const activity = b.dataset.activity;
      b.classList.toggle(
        "is-active",
        !state.sidebarCollapsed && state.activePanel === activity,
      );
    }
  }

  function setPanel(panel) {
    state.activePanel = panel;
    state.sidebarCollapsed = false;
    for (const p of $all("[data-panel]")) {
      p.hidden = p.dataset.panel !== panel;
    }
    app.classList.remove("is-sidebar-collapsed");
    renderActivityBar();
  }

  /* ==========================================================
   * Sidebar: collapse + resize
   * ======================================================== */
  function applySidebarWidth(w) {
    state.sidebarWidth = Math.max(
      MIN_SIDEBAR_WIDTH,
      Math.min(MAX_SIDEBAR_WIDTH, w),
    );
    document.documentElement.style.setProperty(
      "--vn-sidebar-w",
      `${state.sidebarWidth}px`,
    );
  }
  function toggleExplorerSidebar() {
    if (state.sidebarCollapsed) {
      setPanel("explorer");
      return;
    }
    if (state.activePanel === "explorer") {
      state.sidebarCollapsed = true;
      app.classList.add("is-sidebar-collapsed");
      renderActivityBar();
      return;
    }
    setPanel("explorer");
  }
  function toggleSidebar(forceCollapsed) {
    if (forceCollapsed === true) {
      state.sidebarCollapsed = true;
      app.classList.add("is-sidebar-collapsed");
      renderActivityBar();
      return;
    }
    toggleExplorerSidebar();
  }
  function toggleFocus() {
    state.focusMode = !state.focusMode;
    app.classList.toggle("is-focus", state.focusMode);
    setMessage(state.focusMode ? "Focus mode on." : "Focus mode off.", "info");
  }

  function bindSidebarResize() {
    const resizer = $(sel.sidebarResizer);
    if (!resizer) return;
    let startX = 0,
      startW = 0,
      dragging = false;

    const onMove = (e) => {
      if (!dragging) return;
      const x = e.touches ? e.touches[0].clientX : e.clientX;
      applySidebarWidth(startW + (x - startX));
    };
    const onUp = () => {
      if (!dragging) return;
      dragging = false;
      app.classList.remove("is-resizing");
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
      window.removeEventListener("touchmove", onMove);
      window.removeEventListener("touchend", onUp);
    };
    const onDown = (e) => {
      dragging = true;
      startX = e.touches ? e.touches[0].clientX : e.clientX;
      startW = state.sidebarWidth;
      app.classList.add("is-resizing");
      window.addEventListener("mousemove", onMove);
      window.addEventListener("mouseup", onUp);
      window.addEventListener("touchmove", onMove, { passive: false });
      window.addEventListener("touchend", onUp);
      e.preventDefault();
    };

    resizer.addEventListener("mousedown", onDown);
    resizer.addEventListener("touchstart", onDown, { passive: false });
    resizer.addEventListener("dblclick", () =>
      applySidebarWidth(DEFAULT_SIDEBAR_WIDTH),
    );
    resizer.addEventListener("keydown", (e) => {
      if (e.key === "ArrowLeft") {
        applySidebarWidth(state.sidebarWidth - 16);
        e.preventDefault();
      }
      if (e.key === "ArrowRight") {
        applySidebarWidth(state.sidebarWidth + 16);
        e.preventDefault();
      }
    });
  }

  /* ==========================================================
   * Header dropdown menus
   * ======================================================== */
  function closeAllMenus() {
    for (const m of $all(".vn-Menu")) {
      m.classList.remove("is-open");
      const d = $(".vn-Menu__dropdown", m);
      if (d) d.hidden = true;
    }
  }

  function bindMenus() {
    const bar = $("[data-menubar]");
    if (!bar) return;

    bar.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-menu-button]");
      if (btn) {
        const menu = btn.closest(".vn-Menu");
        const isOpen = menu.classList.contains("is-open");
        closeAllMenus();
        if (!isOpen) {
          menu.classList.add("is-open");
          const d = $(".vn-Menu__dropdown", menu);
          if (d) d.hidden = false;
        }
        e.stopPropagation();
        return;
      }
      const item = e.target.closest("[data-command]");
      if (item) {
        closeAllMenus();
        runCommand(item.getAttribute("data-command"));
      }
    });

    bar.addEventListener("mouseover", (e) => {
      const anyOpen = bar.querySelector(".vn-Menu.is-open");
      if (!anyOpen) return;
      const btn = e.target.closest("[data-menu-button]");
      if (!btn) return;
      const menu = btn.closest(".vn-Menu");
      if (menu.classList.contains("is-open")) return;
      closeAllMenus();
      menu.classList.add("is-open");
      const d = $(".vn-Menu__dropdown", menu);
      if (d) d.hidden = false;
    });

    document.addEventListener("click", () => closeAllMenus());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeAllMenus();
    });
  }

  /* ==========================================================
   * Command registry
   * ======================================================== */
  function currentToolbarKind() {
    const button = $(sel.toolbarKind);
    const value = button ? button.dataset.kind : "cpp";
    const kind = normalizeKind(value);

    return ["cpp", "reply", "markdown", "html"].includes(kind) ? kind : "cpp";
  }
  function targetId() {
    return state.selectedId;
  }

  const COMMANDS = {
    "new-note": { label: "New note", run: () => newNote() },
    "open-note": { label: "Open note", run: () => openNote() },
    "new-folder": { label: "New folder", run: () => newFolder() },
    save: { label: "Save note", hint: "⌘S", run: () => saveNote() },
    reload: { label: "Reload from disk", run: () => loadDocument() },
    "add-cpp": { label: "Add C++ cell", run: () => addCell("cpp") },
    "add-reply": { label: "Add Reply cell", run: () => addCell("reply") },
    "add-markdown": {
      label: "Add Markdown cell",
      run: () => addCell("markdown"),
    },
    "add-html": { label: "Add HTML cell", run: () => addCell("html") },
    "insert-below": {
      label: "Insert cell below",
      hint: "B",
      run: () => addCell(currentToolbarKind(), { afterId: targetId() }),
    },
    "run-cell": {
      label: "Run selected cell",
      hint: "⌘↵",
      run: () => targetId() && runCellById(targetId()),
    },
    "run-advance": {
      label: "Run and advance",
      hint: "⇧↵",
      run: async () => {
        if (targetId()) {
          await runCellById(targetId());
          selectAdjacent(1);
        }
      },
    },
    "run-all": { label: "Run all cells", run: () => runAll() },
    "cut-cell": {
      label: "Delete selected cell",
      hint: "D D",
      run: () => targetId() && deleteCellById(targetId()),
    },
    duplicate: {
      label: "Duplicate selected cell",
      run: () => targetId() && duplicateCell(targetId()),
    },
    "move-up": {
      label: "Move cell up",
      run: () => targetId() && moveCellById(targetId(), "up"),
    },
    "move-down": {
      label: "Move cell down",
      run: () => targetId() && moveCellById(targetId(), "down"),
    },
    "to-cpp": {
      label: "Change cell to C++",
      hint: "Y",
      run: () => targetId() && changeKind(targetId(), "cpp"),
    },
    "to-markdown": {
      label: "Change cell to Markdown",
      hint: "M",
      run: () => targetId() && changeKind(targetId(), "markdown"),
    },
    "to-reply": {
      label: "Change cell to Reply",
      hint: "R",
      run: () => targetId() && changeKind(targetId(), "reply"),
    },
    "to-html": {
      label: "Change cell to HTML",
      run: () => targetId() && changeKind(targetId(), "html"),
    },
    "clear-cell": {
      label: "Clear selected output",
      run: () => targetId() && clearCellOutput(targetId()),
    },
    "clear-all": { label: "Clear all outputs", run: () => clearAllOutputs() },
    restart: { label: "Restart kernel", run: () => restartKernel(false) },
    "restart-run": {
      label: "Restart kernel and run all",
      run: () => restartKernel(true),
    },
    "toggle-sidebar": {
      label: "Toggle Explorer",
      hint: "⌘B",
      run: () => toggleExplorerSidebar(),
    },
    "toggle-focus": { label: "Toggle focus mode", run: () => toggleFocus() },
    "show-explorer": {
      label: "Show Explorer",
      run: () => setPanel("explorer"),
)VIXNOTE");

    value.append(R"VIXNOTE(    },
    "show-problems": {
      label: "Show Problems",
      run: () => setPanel("problems"),
    },
    refresh: { label: "Refresh explorer", run: () => refreshExplorer() },
    shortcuts: {
      label: "Keyboard shortcuts",
      hint: "?",
      run: () => showShortcuts(),
    },
    about: { label: "About Vix Note", run: () => showAbout() },
  };

  function runCommand(name) {
    const cmd = COMMANDS[name];
    if (cmd && typeof cmd.run === "function") cmd.run();
  }

  /* ==========================================================
   * Modal system (forms + confirm + info)
   * ======================================================== */
  function modalEls() {
    return {
      root: $("[data-modal]"),
      title: $("[data-modal-title]"),
      body: $("[data-modal-body]"),
      foot: $("[data-modal-foot]"),
    };
  }

  function openModalShell(title) {
    const m = modalEls();
    if (!m.root) return null;
    if (m.title) m.title.textContent = title;
    m.root.hidden = false;
    return m;
  }
  function closeModal() {
    const m = modalEls();
    if (m.root) m.root.hidden = true;
    if (m.body) m.body.innerHTML = "";
    if (m.foot) m.foot.innerHTML = "";
  }

  function showModalForm({ title, fields, confirm = "OK" }) {
    return new Promise((resolve) => {
      const m = openModalShell(title);
      if (!m) {
        resolve(null);
        return;
      }
      m.body.innerHTML = `<form class="vn-Form" data-modal-form>
  ${fields
    .map((f) => {
      const hint = f.hint
        ? `<span class="vn-Form__hint">${escapeHtml(f.hint)}</span>`
        : "";
      if (f.type === "select") {
        return `<label class="vn-Form__field">
          <span class="vn-Form__label">${escapeHtml(f.label)}</span>
          <select class="vn-Form__input" name="${escapeHtml(f.name)}">
            ${(f.options || [])
              .map((option) => {
                const value = String(option.value ?? option);
                const label = String(option.label ?? option);
                const selected =
                  value === String(f.value || "") ? " selected" : "";
                return `<option value="${escapeHtml(value)}"${selected}>${escapeHtml(label)}</option>`;
              })
              .join("")}
          </select>
          ${hint}
        </label>`;
      }
      return `<label class="vn-Form__field">
        <span class="vn-Form__label">${escapeHtml(f.label)}</span>
        <input class="vn-Form__input" name="${escapeHtml(f.name)}" value="${escapeHtml(f.value || "")}" placeholder="${escapeHtml(f.placeholder || "")}" autocomplete="off" spellcheck="false" />
        ${hint}
      </label>`;
    })
    .join("")}
</form>`;
      m.foot.innerHTML = `
        <button type="button" class="vn-Btn vn-Btn--ghost" data-modal-cancel>Cancel</button>
        <button type="button" class="vn-Btn vn-Btn--primary" data-modal-ok>${escapeHtml(confirm)}</button>`;

      const form = $("[data-modal-form]", m.body);
      const firstInput =
        $('input[name="path"]', form) || $("input, textarea, select", form);
      if (firstInput) {
        firstInput.focus();
        if (typeof firstInput.select === "function") firstInput.select();
      }

      const done = (val) => {
        cleanup();
        closeModal();
        resolve(val);
      };
      const collect = () => {
        const data = {};
        for (const input of $all("input, select", form)) {
          data[input.name] = input.value;
        }
        return data;
      };
      const onOk = () => done(collect());
      const onCancel = () => done(null);
      const onKey = (e) => {
        if (e.key === "Enter") {
          e.preventDefault();
          onOk();
        } else if (e.key === "Escape") {
          e.preventDefault();
          onCancel();
        }
      };
      function cleanup() {
        $("[data-modal-ok]", m.foot)?.removeEventListener("click", onOk);
        for (const c of $all("[data-modal-cancel], [data-modal-close]"))
          c.removeEventListener("click", onCancel);
        form.removeEventListener("keydown", onKey);
      }
      $("[data-modal-ok]", m.foot).addEventListener("click", onOk);
      for (const c of $all("[data-modal-cancel], [data-modal-close]"))
        c.addEventListener("click", onCancel);
      form.addEventListener("keydown", onKey);
    });
  }

  function showModalConfirm({ title, body, confirm = "OK", danger = false }) {
    return new Promise((resolve) => {
      const m = openModalShell(title);
      if (!m) {
        resolve(false);
        return;
      }
      m.body.innerHTML = `<p class="vn-Modal__text">${body}</p>`;
      m.foot.innerHTML = `
        <button type="button" class="vn-Btn vn-Btn--ghost" data-modal-cancel>Cancel</button>
        <button type="button" class="vn-Btn ${danger ? "vn-Btn--danger" : "vn-Btn--primary"}" data-modal-ok>${escapeHtml(confirm)}</button>`;
      const okBtn = $("[data-modal-ok]", m.foot);
      okBtn.focus();
      const done = (val) => {
        cleanup();
        closeModal();
        resolve(val);
      };
      const onOk = () => done(true);
      const onCancel = () => done(false);
      function cleanup() {
        okBtn.removeEventListener("click", onOk);
        for (const c of $all("[data-modal-cancel], [data-modal-close]"))
          c.removeEventListener("click", onCancel);
      }
      okBtn.addEventListener("click", onOk);
      for (const c of $all("[data-modal-cancel], [data-modal-close]"))
        c.addEventListener("click", onCancel);
    });
  }

  function showModalInfo(title, html) {
    const m = openModalShell(title);
    if (!m) return;
    m.body.innerHTML = html;
    m.foot.innerHTML = `<button type="button" class="vn-Btn vn-Btn--primary" data-modal-cancel>Close</button>`;
    const close = () => closeModal();
    for (const c of $all("[data-modal-cancel], [data-modal-close]"))
      c.addEventListener("click", close, { once: true });
  }

  function showShortcuts() {
    const rows = [
      ["Command mode", ""],
      ["Run cell", "Ctrl/⌘ + Enter"],
      ["Run and advance", "Shift + Enter"],
      ["Enter edit mode", "Enter"],
      ["Select cell above / below", "↑ / ↓ · K / J"],
      ["Insert cell above / below", "A / B"],
      ["Delete cell", "D D"],
      ["Change to Markdown / C++ / Reply", "M / Y / R"],
      ["Edit mode", ""],
      ["Leave edit mode", "Esc"],
      ["Indent / Outdent", "Tab · Shift + Tab"],
      ["Toggle comment", "Ctrl/⌘ + /"],
      ["Move line", "Alt + ↑ · Alt + ↓"],
      ["Run cell", "Ctrl/⌘ + Enter"],
      ["Run and advance", "Shift + Enter"],
      ["Explorer", ""],
      ["New note (inline)", "Ctrl/⌘ + N"],
      ["New folder (inline)", "Ctrl/⌘ + Shift + N"],
      ["Rename selected", "F2"],
      ["Commit / cancel inline name", "Enter · Esc"],
      ["Global", ""],
      ["Save note", "Ctrl/⌘ + S"],
      ["Toggle sidebar", "Ctrl/⌘ + B"],
      ["Show this dialog", "?"],
    ];
    const html = `<div class="vn-Shortcuts">${rows
      .map(([label, keys]) =>
        keys === ""
          ? `<div class="vn-Shortcuts__group">${escapeHtml(label)}</div>`
          : `<div>${escapeHtml(label)}</div><div>${keys
              .split(" · ")
              .map((k) =>
                k
                  .split(" + ")
                  .map((p) => `<kbd>${escapeHtml(p)}</kbd>`)
                  .join(" + "),
              )
              .join(" · ")}</div>`,
      )
      .join("")}</div>`;
    showModalInfo("Keyboard shortcuts", html);
  }
  function showAbout() {
    showModalInfo(
      "About Vix Note",
      `<div class="vn-About">
      <div class="vn-About__hero">
        <div class="vn-About__logo" aria-hidden="true">V</div>
        <div>
          <h2>Vix Note</h2>
          <p>Visual executable notes for learning C++ and Vix.cpp faster.</p>
        </div>
      </div>

      <p>
        Vix Note is a local notebook workspace for writing notes, running C++
        examples, testing Vix.cpp code, and keeping the result in the same
        document.
      </p>

      <div class="vn-About__section">
        <h3>What you can do</h3>
        <ul>
          <li>Write explanations with Markdown cells.</li>
          <li>Run C++ cells through <code>vix run</code>.</li>
          <li>Use Reply cells for small scripts and quick tests.</li>
          <li>Add HTML cells when you need rendered content.</li>
          <li>See outputs, errors, and diagnostics directly in the note.</li>
        </ul>
      </div>

      <div class="vn-About__section">
        <h3>Workspace</h3>
        <ul>
          <li>Create notes and folders from the explorer.</li>
          <li>Open multiple notes with tabs.</li>
          <li>Use the Problems panel to find failed cells faster.</li>
          <li>Run notes in project context when local headers or dependencies are needed.</li>
        </ul>
      </div>

      <p class="vn-About__meta">
        Vix Note is part of the Vix.cpp ecosystem.
        Built for local C++ learning and development.
        MIT License. Copyright 2026, Gaspard Kirira.
      </p>
    </div>`,
    );
  }

  /* ==========================================================
   * Context menu (custom, not the browser's)
   * ======================================================== */
  let contextMenuEl = null;
  function closeContextMenu() {
    if (contextMenuEl) {
      contextMenuEl.remove();
      contextMenuEl = null;
    }
  }
  function showContextMenu(x, y, items) {
    closeContextMenu();
    const menu = document.createElement("div");
    menu.className = "vn-Context";
    menu.innerHTML = items
      .map((it) =>
        it.sep
          ? `<div class="vn-Context__sep"></div>`
          : `<button type="button" class="vn-Context__item${it.danger ? " is-danger" : ""}${it.disabled ? " is-disabled" : ""}" data-ctx="${escapeHtml(it.id)}">${escapeHtml(it.label)}</button>`,
      )
      .join("");
    document.body.appendChild(menu);
    contextMenuEl = menu;

    const rect = menu.getBoundingClientRect();
    const px = Math.min(x, window.innerWidth - rect.width - 8);
    const py = Math.min(y, window.innerHeight - rect.height - 8);
    menu.style.left = `${Math.max(8, px)}px`;
    menu.style.top = `${Math.max(8, py)}px`;

    menu.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-ctx]");
      if (!btn || btn.classList.contains("is-disabled")) return;
      const id = btn.getAttribute("data-ctx");
      closeContextMenu();
      const found = items.find((i) => i.id === id);
      if (found && found.run) found.run();
    });
  }

  function fileContextItems(path) {
    return [
      {
        id: "open",
        label: "Open",
        run: () => openNotePath(path, { silent: true }),
      },
      { sep: true },
      {
        id: "rename",
        label: "Rename…",
        run: () => startInlineRename(path, "file"),
      },
      {
        id: "delete",
        label: "Delete",
        danger: true,
        run: async () => {
          if (await confirmDelete(baseName(path), "file")) {
            await deletePath(path);
          }
        },
      },
    ];
  }
  function dirContextItems(path) {
    return [
      { id: "new-note", label: "New note", run: () => newNote(path) },
      { id: "new-folder", label: "New folder", run: () => newFolder(path) },
      { sep: true },
      {
        id: "rename",
        label: "Rename…",
        run: () => startInlineRename(path, "dir"),
      },
      {
        id: "delete",
        label: "Delete",
        danger: true,
        run: async () => {
          const ok = await showModalConfirm({
            title: "Delete folder",
            body: `Delete folder “${escapeHtml(baseName(path))}” and everything inside it from disk? This cannot be undone from Vix Note.`,
            confirm: "Delete",
            danger: true,
          });

          if (ok) {
            await deletePath(path, { recursive: true });
          }
        },
      },
    ];
  }

)VIXNOTE");

    value.append(R"VIXNOTE(  /* ==========================================================
   * Wiring: header toolbar actions
   * ======================================================== */
  function bindActions() {
    document.addEventListener("click", (event) => {
      const target = event.target instanceof Element ? event.target : null;

      const kindOption = target ? target.closest("[data-kind-option]") : null;
      if (kindOption) {
        event.preventDefault();
        event.stopPropagation();

        setToolbarKind(kindOption.dataset.kindOption || "cpp", {
          applyToCell: true,
        });

        closeToolbarKindMenu();
        return;
      }

      const kindButton = target
        ? target.closest('[data-action="toolbar-kind"]')
        : null;

      if (kindButton) {
        event.preventDefault();
        event.stopPropagation();
        toggleToolbarKindMenu();
        return;
      }

      if (target && !target.closest("[data-cell-type-select]")) {
        closeToolbarKindMenu();
      }

      const t =
        event.target instanceof Element
          ? event.target.closest("[data-action]")
          : null;
      if (!t) return;
      const action = t.getAttribute("data-action");

      switch (action) {
        case "toggle-sidebar":
          toggleExplorerSidebar();
          break;
        case "save":
          saveNote();
          break;
        case "run-cell":
          if (targetId()) runCellById(targetId());
          break;
        case "run-all":
          runAll();
          break;
        case "restart":
          restartKernel(false);
          break;
        case "insert-below":
          addCell(currentToolbarKind(), { afterId: targetId() });
          break;
        case "cut-cell":
          if (targetId()) deleteCellById(targetId());
          break;
        case "duplicate":
          if (targetId()) duplicateCell(targetId());
          break;
        case "move-up":
          if (targetId()) moveCellById(targetId(), "up");
          break;
        case "move-down":
          if (targetId()) moveCellById(targetId(), "down");
          break;
        case "new-note":
          newNote(state.explorer.selectedDirPath || ".");
          break;
        case "open-note":
          openNote();
          break;
        case "new-folder":
          newFolder(state.explorer.selectedDirPath || ".");
          break;
        case "refresh":
          refreshExplorer();
          break;
        case "clear-problems":
          clearDiagnostics();
          setMessage("Problems cleared.", "info");
          break;
        case "shortcuts":
          showShortcuts();
          break;
        default:
          break;
      }
    });
  }

  /* ==========================================================
   * Wiring: activity bar
   * ======================================================== */
  function bindActivityBar() {
    for (const b of $all("[data-activity]")) {
      b.addEventListener("click", () => {
        const activity = b.dataset.activity;
        if (activity === "explorer") {
          toggleExplorerSidebar();
          return;
        }
        setPanel(activity);
      });
    }
  }

  /* ==========================================================
   * Wiring: explorer + tabs bar
   * ======================================================== */
  function bindExplorer() {
    const listEl = $(sel.explorerList);
    if (listEl) {
      listEl.addEventListener("keydown", (e) => {
        const input = e.target.closest("[data-tree-input]");
        if (input) {
          if (e.key === "Enter") {
            e.preventDefault();
            commitInlineCreate(input.value);
          } else if (e.key === "Escape") {
            e.preventDefault();
            cancelInlineCreate();
          }
          return;
        }

        const renameInput = e.target.closest("[data-tree-rename-input]");
        if (renameInput) {
          if (e.key === "Enter") {
            e.preventDefault();
            commitInlineRename(renameInput.value);
          } else if (e.key === "Escape") {
            e.preventDefault();
            cancelInlineRename();
          }
          return;
        }

        const row = e.target.closest("[data-tree-path]");

        if (!row) {
          state.explorer.selectedDirPath = ".";
          state.explorer.currentPath = ".";
          renderExplorer();
          return;
        }

        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");

        if (e.key === "F2") {
          e.preventDefault();
          startInlineRename(path, type === "dir" ? "dir" : "file");
          return;
        }
        if (e.key === "Enter") {
          if (type === "dir") {
            toggleDirectory(path);
          } else if (type === "file") {
            openFileRowIfAllowed(path);
          }
        }
      });

      listEl.addEventListener(
        "focusout",
        (e) => {
          const input = e.target.closest("[data-tree-input]");
          if (input && state.explorer.draft) {
            setTimeout(() => {
              if (state.explorer.draft) commitInlineCreate(input.value);
            }, 0);
            return;
          }
          const renameInput = e.target.closest("[data-tree-rename-input]");
          if (renameInput && state.explorer.rename) {
            setTimeout(() => {
              if (state.explorer.rename) commitInlineRename(renameInput.value);
            }, 0);
          }
        },
        true,
      );

      listEl.addEventListener("click", (e) => {
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          return;
        }

        const menuBtn = e.target.closest("[data-tree-menu]");
        if (menuBtn) {
          e.stopPropagation();
          const path = menuBtn.getAttribute("data-tree-menu");
          const entry = state.explorer.entries.get(path);
          const rect = menuBtn.getBoundingClientRect();
          showContextMenu(
            rect.left,
            rect.bottom,
            entry && entry.type === "dir"
              ? dirContextItems(path)
              : fileContextItems(path),
          );
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");

        if (type === "dir") {
          state.explorer.selectedDirPath = normalizeExplorerPath(path);
          toggleDirectory(path);
          return;
        }
        if (type === "file") {
          state.explorer.selectedDirPath = parentPath(path);
          openFileRowIfAllowed(path);
        }
      });

      listEl.addEventListener("contextmenu", (e) => {
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        e.preventDefault();
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");
        showContextMenu(
          e.clientX,
          e.clientY,
          type === "dir" ? dirContextItems(path) : fileContextItems(path),
        );
      });

      // ---- Explorer drag and drop ----
      listEl.addEventListener("dragstart", (e) => {
        // Never start a drag from an inline input.
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          e.preventDefault();
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");
        if (!path || path === ".") {
          e.preventDefault();
          return;
        }
        state.drag.tab = null;
        state.drag.explorer = { path: normalizeExplorerPath(path), type };
        row.classList.add("is-dragging");
        if (e.dataTransfer) {
          e.dataTransfer.effectAllowed = "move";
          // Some browsers require data to be set for a drag to begin.
          try {
            e.dataTransfer.setData("text/plain", path);
          } catch (_) {}
        }
      });

      listEl.addEventListener("dragover", (e) => {
        const drag = state.drag.explorer;
        if (!drag) return;

        const row = e.target.closest("[data-tree-path]");
        const targetDir = row ? explorerDropDirForRow(row) : "."; // empty space => root

        clearExplorerDropTargets();

        if (targetDir == null) {
          return; // not a valid target (e.g. a file row)
        }

        if (!canMoveExplorerPath(drag.path, drag.type, targetDir)) {
          return;
        }

        e.preventDefault();
        if (e.dataTransfer) e.dataTransfer.dropEffect = "move";

        if (row) {
          row.classList.add("is-drop-target");
        } else {
          listEl.classList.add("is-root-drop-target");
        }
      });

      listEl.addEventListener("dragleave", (e) => {
        const row = e.target.closest("[data-tree-path]");
        if (row) row.classList.remove("is-drop-target");
      });

      listEl.addEventListener("drop", (e) => {
        const drag = state.drag.explorer;
        if (!drag) return;

        const row = e.target.closest("[data-tree-path]");
        const targetDir = row ? explorerDropDirForRow(row) : ".";

        clearExplorerDropTargets();

        if (targetDir == null) return;
        if (!canMoveExplorerPath(drag.path, drag.type, targetDir)) return;

        e.preventDefault();
        const source = drag.path;
        state.drag.explorer = null;
        moveExplorerPath(source, targetDir);
      });

      listEl.addEventListener("dragend", () => {
        state.drag.explorer = null;
        for (const el of $all(".vn-Tree__row.is-dragging")) {
          el.classList.remove("is-dragging");
        }
        clearExplorerDropTargets();
      });
    }

    const search = $(sel.explorerSearch);
    if (search) search.addEventListener("input", renderExplorer);

    const problemsList = $(sel.problemsList);
    if (problemsList) {
      problemsList.addEventListener("click", (e) => {
        const goto = e.target.closest("[data-problem-goto]");
        if (goto) {
          gotoCellFromDiagnostic(goto.getAttribute("data-problem-goto"));
        }
      });
    }

    bindTabsBar();

    document.addEventListener("click", () => closeContextMenu());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeContextMenu();
    });
    window.addEventListener("blur", closeContextMenu);
    window.addEventListener("resize", closeContextMenu);
  }

  /* ==========================================================
   * Wiring: tabs bar (click + reorder drag and drop)
   * ======================================================== */
  function clearTabDropMarkers() {
    for (const el of $all(".vn-Tab.is-drop-before, .vn-Tab.is-drop-after")) {
      el.classList.remove("is-drop-before", "is-drop-after");
    }
  }

  function bindTabsBar() {
    const tabsBar = $(sel.tabsBar);
    if (!tabsBar) return;

    tabsBar.addEventListener("click", (e) => {
      const close = e.target.closest("[data-tab-close]");
      if (close) {
        e.stopPropagation();
        closeTab(close.getAttribute("data-tab-close"));
        return;
      }
      const tab = e.target.closest("[data-tab-path]");
      if (tab) switchTab(tab.getAttribute("data-tab-path"));
    });

    tabsBar.addEventListener("dragstart", (e) => {
      const tab = e.target.closest("[data-tab-path]");
      if (!tab) return;
      const path = normalizeExplorerPath(tab.getAttribute("data-tab-path"));
      const fromIndex = state.tabs.findIndex(
        (t) => normalizeExplorerPath(t.path) === path,
      );
      state.drag.explorer = null;
      state.drag.tab = { path, fromIndex };
      tab.classList.add("is-dragging");
)VIXNOTE");

    value.append(R"VIXNOTE(      if (e.dataTransfer) {
        e.dataTransfer.effectAllowed = "move";
        try {
          e.dataTransfer.setData("text/plain", path);
        } catch (_) {}
      }
    });

    tabsBar.addEventListener("dragover", (e) => {
      const drag = state.drag.tab;
      if (!drag) return;
      const tab = e.target.closest("[data-tab-path]");
      if (!tab) return;
      const path = normalizeExplorerPath(tab.getAttribute("data-tab-path"));
      if (path === drag.path) {
        clearTabDropMarkers();
        return;
      }
      e.preventDefault();
      if (e.dataTransfer) e.dataTransfer.dropEffect = "move";

      // Decide before/after from cursor position within the tab.
      const rect = tab.getBoundingClientRect();
      const after = e.clientX > rect.left + rect.width / 2;
      clearTabDropMarkers();
      tab.classList.add(after ? "is-drop-after" : "is-drop-before");
    });

    tabsBar.addEventListener("dragleave", (e) => {
      const tab = e.target.closest("[data-tab-path]");
      if (tab) tab.classList.remove("is-drop-before", "is-drop-after");
    });

    tabsBar.addEventListener("drop", (e) => {
      const drag = state.drag.tab;
      if (!drag) return;
      const tab = e.target.closest("[data-tab-path]");
      if (!tab) {
        clearTabDropMarkers();
        return;
      }
      const targetPath = normalizeExplorerPath(
        tab.getAttribute("data-tab-path"),
      );
      if (targetPath === drag.path) {
        clearTabDropMarkers();
        return;
      }
      e.preventDefault();
      const rect = tab.getBoundingClientRect();
      const after = e.clientX > rect.left + rect.width / 2;
      const source = drag.path;
      state.drag.tab = null;
      clearTabDropMarkers();
      reorderTabs(source, targetPath, after ? "after" : "before");
    });

    tabsBar.addEventListener("dragend", () => {
      state.drag.tab = null;
      for (const el of $all(".vn-Tab.is-dragging")) {
        el.classList.remove("is-dragging");
      }
      clearTabDropMarkers();
    });
  }

  function openFileRowIfAllowed(path) {
    const entry = state.explorer.entries.get(path);
    if (entry && entry.openable === false && !path.endsWith(".vixnote")) {
      setMessage("Only .vixnote files can be opened in Vix Note.", "warning");
      return;
    }
    openNotePath(path, { silent: true });
  }

  /* ==========================================================
   * Wiring: cell interactions
   * ======================================================== */
  function bindCellInteractions() {
    const container = $(sel.cells);
    if (!container) return;

    container.addEventListener("click", (event) => {
      const target = event.target;
      if (!(target instanceof Element)) return;

      const insertBtn = target.closest("[data-insert-after]");
      if (insertBtn) {
        addCell(currentToolbarKind(), {
          afterId: insertBtn.getAttribute("data-insert-after"),
        });
        return;
      }
      const actionBtn = target.closest("[data-cell-action]");
      const cellEl = target.closest(".vn-Cell");
      if (!cellEl) return;
      const id = cellEl.dataset.cellId;

      if (actionBtn) {
        const a = actionBtn.getAttribute("data-cell-action");
        if (a === "run") return void runCellById(id);
        if (a === "edit") return void toggleCellEdit(id);
        if (a === "duplicate") return void duplicateCell(id);
        if (a === "up") return void moveCellById(id, "up");
        if (a === "down") return void moveCellById(id, "down");
        if (a === "delete") return void deleteCellById(id);
        if (a === "select") return void selectCell(id, { edit: false });
      }
      const inEditor = target.closest(".vn-Editor, textarea");
      selectCell(id, { edit: !!inEditor, focus: !!inEditor });
    });

    container.addEventListener("dblclick", (event) => {
      const target = event.target;
      if (!(target instanceof Element)) return;
      if (!target.closest("[data-rendered]")) return;
      const cellEl = target.closest(".vn-Cell");
      if (cellEl) selectCell(cellEl.dataset.cellId, { edit: true });
    });

    container.addEventListener("input", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      markTextareaChanged(ta);
    });

    container.addEventListener("click", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener("keyup", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener("select", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener(
      "scroll",
      (event) => {
        if (event.target instanceof HTMLTextAreaElement)
          syncScroll(event.target);
      },
      true,
    );

    container.addEventListener(
      "focusout",
      async (event) => {
        const ta = event.target;
        if (!(ta instanceof HTMLTextAreaElement)) return;
        if (ta.getAttribute("data-action") !== "edit-source") return;
        const cellEl = ta.closest(".vn-Cell");
        if (!cellEl) return;
        try {
          await pushCell(cellEl);
        } catch (_) {}
      },
      true,
    );
  }

  /* ==========================================================
   * Keyboard
   * ======================================================== */
  let lastDTime = 0;
  function handleDoubleD() {
    const now = Date.now();
    if (now - lastDTime < 500) {
      lastDTime = 0;
      if (state.selectedId) deleteCellById(state.selectedId);
    } else lastDTime = now;
  }
  async function insertAbove(id) {
    const idx = cellIndex(id);
    if (idx < 0) return;
    await addCell(currentToolbarKind(), { atIndex: idx });
  }

  function inlineInputActive() {
    return !!(state.explorer.draft || state.explorer.rename);
  }

  function bindKeyboard() {
    document.addEventListener("keydown", async (event) => {
      const inField =
        event.target instanceof HTMLTextAreaElement ||
        event.target instanceof HTMLInputElement;
      const inTextarea = event.target instanceof HTMLTextAreaElement;
      const meta = event.ctrlKey || event.metaKey;

      if (meta && event.key.toLowerCase() === "n") {
        if (!inTextarea) {
          event.preventDefault();
          if (event.shiftKey) newFolder();
          else newNote();
          return;
        }
      }

      if (inlineInputActive() && inField) {
        return;
      }

      if (meta && event.key === "Enter") {
        event.preventDefault();
        if (state.selectedId) await runCellById(state.selectedId);
        if (inTextarea) enterCommandMode();
        return;
      }
      if (event.shiftKey && event.key === "Enter") {
        if (inField && !inTextarea) return;
        event.preventDefault();
        if (state.selectedId) {
          await runCellById(state.selectedId);
          selectAdjacent(1);
        }
        return;
      }
      if (meta && event.key.toLowerCase() === "s") {
        event.preventDefault();
        await saveNote();
        return;
      }
      if (meta && event.key.toLowerCase() === "b") {
        event.preventDefault();
        toggleSidebar();
        return;
      }

      if (state.editing && inTextarea) {
        const ta = event.target;

        if (event.key === "Escape") {
          event.preventDefault();
          if (state.selectedId) {
            const cellEl = cellElById(state.selectedId);
            if (cellEl) localUpdateFromDom(cellEl);
          }
          exitEditMode();
          return;
        }
        if (event.key === "Tab") {
          event.preventDefault();
          if (event.shiftKey) outdentTextarea(ta);
          else indentTextarea(ta);
          return;
        }
        if (meta && event.key === "/") {
          event.preventDefault();
          toggleCommentTextarea(ta);
          return;
        }
        if (event.altKey && event.key === "ArrowUp") {
          event.preventDefault();
          moveCurrentLine(ta, -1);
          return;
        }
        if (event.altKey && event.key === "ArrowDown") {
          event.preventDefault();
          moveCurrentLine(ta, 1);
          return;
        }
        if (meta && event.key.toLowerCase() === "s") {
          event.preventDefault();
          await saveNote();
          return;
        }
        updateLineFocus(ta);
        updateCursorStatus(ta);
        return;
      }

      if (inField) return;

      if (event.key === "?") {
        event.preventDefault();
        showShortcuts();
        return;
      }

      if (!state.selectedId) return;

      switch (event.key) {
        case "Enter":
          event.preventDefault();
          enterEditMode();
          break;
        case "ArrowUp":
        case "k":
          event.preventDefault();
          selectAdjacent(-1);
          break;
        case "ArrowDown":
        case "j":
          event.preventDefault();
          selectAdjacent(1);
          break;
        case "a":
          event.preventDefault();
          insertAbove(state.selectedId);
          break;
        case "b":
          event.preventDefault();
          addCell(currentToolbarKind(), { afterId: state.selectedId });
          break;
        case "d":
          handleDoubleD();
          break;
        case "m":
          event.preventDefault();
          changeKind(state.selectedId, "markdown");
          break;
        case "y":
          event.preventDefault();
          changeKind(state.selectedId, "cpp");
          break;
        case "r":
          event.preventDefault();
          changeKind(state.selectedId, "reply");
          break;
        default:
          break;
      }
    });
  }

  /* ==========================================================
   * Init
   * ======================================================== */
  function init() {
    applySidebarWidth(DEFAULT_SIDEBAR_WIDTH);
    if (window.matchMedia("(max-width: 900px)").matches) toggleSidebar(true);

    bindActions();
    bindActivityBar();
    bindMenus();
    bindSidebarResize();
    bindCellInteractions();
    bindExplorer();
    bindKeyboard();

    const statusProblems = $(sel.statusProblems);
    if (statusProblems) {
      statusProblems.addEventListener("click", () => setPanel("problems"));
    }

    for (const c of $all("[data-modal-close]"))
      c.addEventListener("click", () => closeModal());

    setPanel("explorer");
    setKernel("idle");
    renderExplorer();
    renderOpenTabs();
    renderTabsBar();
    renderProblems();
    loadDocument();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
)VIXNOTE");

    return value;
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
