/**
 *
 *  @file note_assets_test.cpp
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

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace
{
  std::filesystem::path make_test_root(const std::string &name)
  {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("vix-note-assets-test-" + name + "-" + std::to_string(now));
  }

  void write_file(const std::filesystem::path &path, const std::string &content)
  {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }

  std::string read_file(const std::filesystem::path &path)
  {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
  }

  void write_asset_tree(const std::filesystem::path &root)
  {
    write_file(root / "index.html", "<!doctype html><html><body>Disk UI</body></html>");
    write_file(root / "assets" / "note.css", ".disk-ui { color: red; }");
    write_file(root / "assets" / "note.js", "console.log('disk ui');");
  }

  void set_env(const char *key, const std::string &value)
  {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
  }

  void unset_env(const char *key)
  {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    ::unsetenv(key);
#endif
  }
}

int main()
{
  {
    vix::note::NoteAsset asset{"/assets/test.txt", "text/plain", "hello"};
    assert(!asset.empty());
    asset.content.clear();
    assert(asset.empty());
  }

  {
    const auto sourceAssets = std::filesystem::path(__FILE__).parent_path().parent_path() / "assets";
    const auto html = read_file(sourceAssets / "index.html");
    const auto css = read_file(sourceAssets / "note.css");
    const auto js = read_file(sourceAssets / "note.js");

    assert(html.find("/assets/note.css") != std::string::npos);
    assert(html.find("/assets/note.js") != std::string::npos);
    assert(html.find("data-command=\"note.save\"") != std::string::npos);
    assert(html.find("data-command=\"cell.run\"") != std::string::npos);
    assert(html.find("data-command=\"view.showProblems\"") != std::string::npos);

    assert(css.find("vn-PickerOverlay") != std::string::npos);
    assert(css.find("vn-Picker__item") != std::string::npos);
    assert(css.find("vn-Notification") != std::string::npos);

    assert(js.find("const commands = new Map") != std::string::npos);
    assert(js.find("function registerCommand") != std::string::npos);
    assert(js.find("async function executeCommand") != std::string::npos);
    assert(js.find("openCommandPalette") != std::string::npos);
    assert(js.find("openQuickOpen") != std::string::npos);
    assert(js.find("/api/extensions") != std::string::npos);
    assert(js.find("normalizedCellTypes") != std::string::npos);
    assert(js.find("data-change-cell-type") != std::string::npos);
    assert(js.find("UI_STATE_KEY") != std::string::npos);
    assert(js.find("window.confirm") == std::string::npos);
    assert(js.find("confirm(") == std::string::npos);
    assert(js.find("recommended: false") != std::string::npos);
    assert(js.find("builtin: false") != std::string::npos);
    assert(js.find("toggleActivityPanel") != std::string::npos);
    assert(js.find("VIX_LOGO_SVG") != std::string::npos);
  }

  const auto validRoot = make_test_root("valid");
  std::filesystem::remove_all(validRoot);
  write_asset_tree(validRoot);

  {
    std::string err;
    assert(vix::note::note_asset_directory_is_valid(validRoot, &err));
    assert(err.empty());

    vix::note::NoteAssets assets(validRoot);
    assert(assets.valid());
    assert(assets.root() == validRoot);
    assert(assets.contains("/"));
    assert(assets.contains("/index.html"));
    assert(assets.contains("/assets/note.css"));
    assert(assets.contains("/assets/note.js"));

    auto html = assets.find("/");
    assert(html.has_value());
    assert(html->contentType == "text/html; charset=utf-8");
    assert(html->content.find("Disk UI") != std::string::npos);

    auto css = assets.find("/assets/note.css");
    assert(css.has_value());
    assert(css->contentType == "text/css; charset=utf-8");
    assert(css->content.find(".disk-ui") != std::string::npos);

    auto js = assets.find("/assets/note.js");
    assert(js.has_value());
    assert(js->contentType == "application/javascript; charset=utf-8");
    assert(js->content.find("disk ui") != std::string::npos);
  }

  {
    vix::note::NoteAssets assets(validRoot);
    assert(!assets.contains("/assets/../../../../etc/passwd"));
    assert(!assets.contains("/assets/%2e%2e/%2e%2e/etc/passwd"));
    assert(!assets.contains("/assets/..\\..\\windows.ini"));
    assert(!assets.contains("/etc/passwd"));
  }

  {
    assert(vix::note::note_asset_content_type("index.html") == "text/html; charset=utf-8");
    assert(vix::note::note_asset_content_type("note.css") == "text/css; charset=utf-8");
    assert(vix::note::note_asset_content_type("note.js") == "application/javascript; charset=utf-8");
    assert(vix::note::note_asset_content_type("data.json") == "application/json; charset=utf-8");
    assert(vix::note::note_asset_content_type("icon.svg") == "image/svg+xml");
    assert(vix::note::note_asset_content_type("image.png") == "image/png");
    assert(vix::note::note_asset_content_type("font.woff2") == "font/woff2");
  }

  {
    assert(vix::note::normalize_note_asset_path("") == "/");
    assert(vix::note::normalize_note_asset_path("/") == "/");
    assert(vix::note::normalize_note_asset_path("index.html") == "/");
    assert(vix::note::normalize_note_asset_path("assets/note.css") == "/assets/note.css");
    assert(vix::note::normalize_note_asset_path("/assets/note.js") == "/assets/note.js");
    assert(vix::note::normalize_note_asset_path("/assets/%2e%2e/passwd").empty());
  }

  {
    const auto missing = make_test_root("missing");
    std::filesystem::remove_all(missing);
    std::filesystem::create_directories(missing / "assets");
    write_file(missing / "index.html", "<!doctype html>");
    write_file(missing / "assets" / "note.css", "body {}");

    std::string err;
    assert(!vix::note::note_asset_directory_is_valid(missing, &err));
    assert(err.find("assets/note.js") != std::string::npos);
  }

  {
    vix::note::NoteAssetResolveOptions options;
    options.customDirectory = validRoot;
    options.useEnvironmentDirectory = false;
    options.useBuildDirectory = false;
    options.useExecutableRelativeDirectory = false;
    options.useInstalledDirectory = false;
    options.useGlobalDirectory = false;
    options.useSourceDirectory = false;

    const auto resolved = vix::note::resolve_note_asset_directory(options);
    assert(resolved.found());
    assert(resolved.directory.filename() == validRoot.filename());
  }

  {
    const auto envRoot = make_test_root("env");
    std::filesystem::remove_all(envRoot);
    write_asset_tree(envRoot);
    set_env("VIX_NOTE_ASSETS_DIR", envRoot.string());

    vix::note::NoteAssetResolveOptions options;
    options.useBuildDirectory = false;
    options.useExecutableRelativeDirectory = false;
    options.useInstalledDirectory = false;
    options.useGlobalDirectory = false;
    options.useSourceDirectory = false;

    const auto resolved = vix::note::resolve_note_asset_directory(options);
    assert(resolved.found());
    assert(resolved.directory.filename() == envRoot.filename());
    unset_env("VIX_NOTE_ASSETS_DIR");
  }

  std::filesystem::remove_all(validRoot);
  return 0;
}
