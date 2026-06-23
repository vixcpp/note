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
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
  std::filesystem::path make_test_root()
  {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("vix-note-assets-test-" + std::to_string(now));
  }

  void write_file(
      const std::filesystem::path &path,
      const std::string &content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }

  std::string read_file(const std::filesystem::path &path)
  {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }

  bool has_asset_path(
      const std::vector<vix::note::NoteAsset> &assets,
      const std::string &path)
  {
    for (const auto &asset : assets)
    {
      if (asset.path == path)
      {
        return true;
      }
    }

    return false;
  }
}

int main()
{
  const std::filesystem::path root = make_test_root();

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  {
    vix::note::NoteAsset asset{
        "/assets/test.txt",
        "text/plain; charset=utf-8",
        "hello"};

    assert(!asset.empty());

    asset.content.clear();

    assert(asset.empty());
  }

  {
    vix::note::NoteAssets assets;

    assert(!assets.empty());
    assert(assets.size() >= 4);

    assert(assets.contains("/"));
    assert(assets.contains("/index.html"));
    assert(assets.contains("/assets/note.css"));
    assert(assets.contains("/assets/note.js"));

    assert(assets.contains(""));
    assert(assets.contains("index.html"));
    assert(assets.contains("assets/note.css"));
    assert(assets.contains("assets/note.js"));
  }

  {
    std::vector<vix::note::NoteAsset> customAssets;

    customAssets.push_back(
        vix::note::NoteAsset{
            "/custom.txt",
            "text/plain; charset=utf-8",
            "custom"});

    vix::note::NoteAssets assets(customAssets);

    assert(assets.size() == 1);
    assert(assets.contains("/custom.txt"));
    assert(!assets.contains("/"));

    std::optional<vix::note::NoteAsset> custom =
        assets.find("/custom.txt");

    assert(custom.has_value());
    assert(custom->content == "custom");
  }

  {
    vix::note::NoteAssets assets;

    std::optional<vix::note::NoteAsset> index =
        assets.find("/");

    assert(index.has_value());
    assert(index->path == "/");
    assert(index->contentType == "text/html; charset=utf-8");
    assert(index->content.find("<!doctype html>") != std::string::npos);
    assert(index->content.find("Vix Note") != std::string::npos);

    std::optional<vix::note::NoteAsset> css =
        assets.find("/assets/note.css");

    assert(css.has_value());
    assert(css->contentType == "text/css; charset=utf-8");
    assert(css->content.find(".note-shell") != std::string::npos);

    std::optional<vix::note::NoteAsset> js =
        assets.find("/assets/note.js");

    assert(js.has_value());
    assert(js->contentType == "application/javascript; charset=utf-8");
    assert(js->content.find("run-cell") != std::string::npos);
  }

  {
    vix::note::NoteAssets assets;

    std::optional<vix::note::NoteAsset> missing =
        assets.find("/missing.txt");

    assert(!missing.has_value());
    assert(!assets.contains("/missing.txt"));
  }

  {
    vix::note::NoteAssets assets;

    assets.add_or_replace(
        vix::note::NoteAsset{
            "/custom.txt",
            "text/plain; charset=utf-8",
            "custom content"});

    assert(assets.contains("/custom.txt"));

    std::optional<vix::note::NoteAsset> custom =
        assets.find("/custom.txt");

    assert(custom.has_value());
    assert(custom->path == "/custom.txt");
    assert(custom->contentType == "text/plain; charset=utf-8");
    assert(custom->content == "custom content");
  }

  {
    vix::note::NoteAssets assets;

    assets.add_or_replace(
        vix::note::NoteAsset{
            "custom.css",
            "",
            "body { margin: 0; }"});

    std::optional<vix::note::NoteAsset> custom =
        assets.find("/custom.css");

    assert(custom.has_value());
    assert(custom->path == "/custom.css");
    assert(custom->contentType == "text/css; charset=utf-8");
    assert(custom->content == "body { margin: 0; }");
  }

  {
    vix::note::NoteAssets assets;

    const std::size_t before = assets.size();

    assets.add_or_replace(
        vix::note::NoteAsset{
            "/assets/note.css",
            "text/css; charset=utf-8",
            "/* replaced */"});

    assert(assets.size() == before);

    std::optional<vix::note::NoteAsset> css =
        assets.find("/assets/note.css");

    assert(css.has_value());
    assert(css->content == "/* replaced */");
  }

  {
    vix::note::NoteAssets assets;

    assert(assets.contains("/assets/note.js"));

    const bool removed =
        assets.remove("/assets/note.js");

    assert(removed);
    assert(!assets.contains("/assets/note.js"));

    const bool removedAgain =
        assets.remove("/assets/note.js");

    assert(!removedAgain);
  }

  {
    vix::note::NoteAssets assets;

    assert(!assets.empty());

    assets.clear();

    assert(assets.empty());
    assert(assets.size() == 0);
    assert(!assets.contains("/"));
  }

  {
    std::vector<vix::note::NoteAsset> defaults =
        vix::note::NoteAssets::defaults();

    assert(defaults.size() >= 4);

    assert(has_asset_path(defaults, "/"));
    assert(has_asset_path(defaults, "/index.html"));
    assert(has_asset_path(defaults, "/assets/note.css"));
    assert(has_asset_path(defaults, "/assets/note.js"));
  }

  {
    const std::string html =
        vix::note::NoteAssets::default_index_html();

    assert(html.find("<!doctype html>") != std::string::npos);
    assert(html.find("Vix Note") != std::string::npos);
    assert(html.find("/assets/note.css") != std::string::npos);
    assert(html.find("/assets/note.js") != std::string::npos);
  }

  {
    const std::string css =
        vix::note::NoteAssets::default_css();

    assert(css.find(":root") != std::string::npos);
    assert(css.find(".note-cell") != std::string::npos);
    assert(css.find("--note-accent") != std::string::npos);
  }

  {
    const std::string js =
        vix::note::NoteAssets::default_js();

    assert(js.find("run-cell") != std::string::npos);
    assert(js.find("run-all") != std::string::npos);
    assert(js.find("save") != std::string::npos);
  }

  {
    assert(vix::note::normalize_note_asset_path("") == "/");
    assert(vix::note::normalize_note_asset_path("/") == "/");
    assert(vix::note::normalize_note_asset_path("index.html") == "/index.html");
    assert(vix::note::normalize_note_asset_path("/index.html") == "/index.html");
    assert(vix::note::normalize_note_asset_path("assets/note.css") == "/assets/note.css");
    assert(vix::note::normalize_note_asset_path("/assets/note.css/") == "/assets/note.css");
  }

  {
    assert(vix::note::note_asset_content_type("/") == "text/html; charset=utf-8");
    assert(vix::note::note_asset_content_type("/index.html") == "text/html; charset=utf-8");
    assert(vix::note::note_asset_content_type("/assets/note.css") == "text/css; charset=utf-8");
    assert(vix::note::note_asset_content_type("/assets/note.js") == "application/javascript; charset=utf-8");
    assert(vix::note::note_asset_content_type("/manifest.json") == "application/json; charset=utf-8");
    assert(vix::note::note_asset_content_type("/logo.svg") == "image/svg+xml");
    assert(vix::note::note_asset_content_type("/logo.png") == "image/png");
    assert(vix::note::note_asset_content_type("/photo.jpg") == "image/jpeg");
    assert(vix::note::note_asset_content_type("/photo.jpeg") == "image/jpeg");
    assert(vix::note::note_asset_content_type("/readme.txt") == "text/plain; charset=utf-8");
  }

  {
    assert(vix::note::note_asset_public_path("") == "/");
    assert(vix::note::note_asset_public_path("index.html") == "/");
    assert(vix::note::note_asset_public_path(std::filesystem::path("css") / "note.css") == "/assets/note.css");
    assert(vix::note::note_asset_public_path(std::filesystem::path("js") / "note.js") == "/assets/note.js");
    assert(vix::note::note_asset_public_path("images/logo.svg") == "/assets/images/logo.svg");
    assert(vix::note::note_asset_public_path("/assets/note.css") == "/assets/note.css");
  }

  {
    const std::filesystem::path file =
        root / "read" / "asset.txt";

    write_file(file, "asset content");

    std::string out;
    std::string err;

    const bool ok =
        vix::note::read_note_asset_file(file, out, err);

    assert(ok);
    assert(err.empty());
    assert(out == "asset content");
    assert(read_file(file) == "asset content");
  }

  {
    std::string out;
    std::string err;

    const bool ok =
        vix::note::read_note_asset_file(
            root / "missing" / "asset.txt",
            out,
            err);

    assert(!ok);
    assert(out.empty());
    assert(!err.empty());
    assert(err.find("cannot open note asset file") != std::string::npos);
  }

  {
    const std::filesystem::path dir =
        root / "full-assets";

    write_file(
        dir / "index.html",
        "<!doctype html><title>Disk Note</title>");

    write_file(
        dir / "css" / "note.css",
        ".disk-note { color: red; }");

    write_file(
        dir / "js" / "note.js",
        "console.log('disk note');");

    std::string err;

    std::vector<vix::note::NoteAsset> assets =
        vix::note::NoteAssets::from_directory(dir, err);

    assert(err.empty());
    assert(assets.size() == 4);

    assert(has_asset_path(assets, "/"));
    assert(has_asset_path(assets, "/index.html"));
    assert(has_asset_path(assets, "/assets/note.css"));
    assert(has_asset_path(assets, "/assets/note.js"));

    vix::note::NoteAssets registry(assets);

    assert(registry.contains("/"));
    assert(registry.contains("/index.html"));
    assert(registry.contains("/assets/note.css"));
    assert(registry.contains("/assets/note.js"));

    assert(registry.find("/")->content.find("Disk Note") != std::string::npos);
    assert(registry.find("/index.html")->content.find("Disk Note") != std::string::npos);
    assert(registry.find("/assets/note.css")->content.find(".disk-note") != std::string::npos);
    assert(registry.find("/assets/note.js")->content.find("disk note") != std::string::npos);
  }

  {
    std::string err;

    std::vector<vix::note::NoteAsset> assets =
        vix::note::NoteAssets::from_directory({}, err);

    assert(assets.empty());
    assert(!err.empty());
    assert(err == "empty note asset directory");
  }

  {
    std::string err;

    std::vector<vix::note::NoteAsset> assets =
        vix::note::NoteAssets::from_directory(
            root / "does-not-exist",
            err);

    assert(assets.empty());
    assert(!err.empty());
    assert(err.find("note asset directory does not exist") != std::string::npos);
  }

  {
    const std::filesystem::path dir =
        root / "partial-assets";

    write_file(
        dir / "css" / "note.css",
        ".partial-note { color: blue; }");

    vix::note::NoteAssets assets;

    const std::size_t before =
        assets.size();

    std::string err;

    const bool ok =
        assets.load_from_directory(dir, err);

    assert(ok);
    assert(err.empty());

    assert(assets.size() == before);
    assert(assets.contains("/"));
    assert(assets.contains("/index.html"));
    assert(assets.contains("/assets/note.css"));
    assert(assets.contains("/assets/note.js"));

    std::optional<vix::note::NoteAsset> css =
        assets.find("/assets/note.css");

    assert(css.has_value());
    assert(css->content.find(".partial-note") != std::string::npos);

    std::optional<vix::note::NoteAsset> index =
        assets.find("/");

    assert(index.has_value());
    assert(index->content.find("Vix Note") != std::string::npos);
  }

  {
    const std::filesystem::path dir =
        root / "clear-assets";

    write_file(
        dir / "css" / "note.css",
        ".only-css { color: green; }");

    vix::note::NoteAssets assets;

    vix::note::NoteAssetDirectoryOptions options;
    options.clearBeforeLoad = true;
    options.keepEmbeddedFallback = true;

    std::string err;

    const bool ok =
        assets.load_from_directory(dir, options, err);

    assert(ok);
    assert(err.empty());

    assert(assets.size() == 1);
    assert(!assets.contains("/"));
    assert(!assets.contains("/index.html"));
    assert(assets.contains("/assets/note.css"));
    assert(!assets.contains("/assets/note.js"));

    assert(assets.find("/assets/note.css")->content.find(".only-css") != std::string::npos);
  }

  {
    const std::filesystem::path dir =
        root / "strict-missing-assets";

    write_file(
        dir / "css" / "note.css",
        ".strict-css { color: black; }");

    vix::note::NoteAssets assets;

    vix::note::NoteAssetDirectoryOptions options;
    options.clearBeforeLoad = false;
    options.keepEmbeddedFallback = false;

    std::string err;

    const bool ok =
        assets.load_from_directory(dir, options, err);

    assert(!ok);
    assert(!err.empty());
    assert(err.find("asset directory is missing required Vix Note UI assets") != std::string::npos);

    assert(assets.contains("/"));
    assert(assets.contains("/index.html"));
    assert(assets.contains("/assets/note.css"));
    assert(assets.contains("/assets/note.js"));
  }

  {
    const std::filesystem::path dir =
        root / "strict-full-assets";

    write_file(
        dir / "index.html",
        "<!doctype html><title>Strict Disk Note</title>");

    write_file(
        dir / "css" / "note.css",
        ".strict-note { color: orange; }");

    write_file(
        dir / "js" / "note.js",
        "console.log('strict note');");

    vix::note::NoteAssets assets;

    vix::note::NoteAssetDirectoryOptions options;
    options.clearBeforeLoad = false;
    options.keepEmbeddedFallback = false;

    std::string err;

    const bool ok =
        assets.load_from_directory(dir, options, err);

    assert(ok);
    assert(err.empty());

    assert(assets.size() == 4);
    assert(assets.contains("/"));
    assert(assets.contains("/index.html"));
    assert(assets.contains("/assets/note.css"));
    assert(assets.contains("/assets/note.js"));

    assert(assets.find("/")->content.find("Strict Disk Note") != std::string::npos);
    assert(assets.find("/assets/note.css")->content.find(".strict-note") != std::string::npos);
    assert(assets.find("/assets/note.js")->content.find("strict note") != std::string::npos);
  }

  std::filesystem::remove_all(root);

  return 0;
}
