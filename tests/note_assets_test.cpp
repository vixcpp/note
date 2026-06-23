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
#include <optional>
#include <string>
#include <vector>

int main()
{
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

    bool hasIndex = false;
    bool hasCss = false;
    bool hasJs = false;

    for (const auto &asset : defaults)
    {
      if (asset.path == "/")
      {
        hasIndex = true;
      }

      if (asset.path == "/assets/note.css")
      {
        hasCss = true;
      }

      if (asset.path == "/assets/note.js")
      {
        hasJs = true;
      }
    }

    assert(hasIndex);
    assert(hasCss);
    assert(hasJs);
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

  return 0;
}
