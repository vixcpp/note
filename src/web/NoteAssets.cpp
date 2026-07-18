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
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace vix::note
{
  namespace
  {
    constexpr const char *kFallbackHtml =
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "  <head><meta charset=\"utf-8\"><title>Vix Note assets not found</title></head>\n"
        "  <body>\n"
        "    <h1>Vix Note assets not found</h1>\n"
        "    <p>Reinstall Vix or set VIX_NOTE_ASSETS_DIR.</p>\n"
        "  </body>\n"
        "</html>\n";

    std::string lower_extension(std::filesystem::path path)
    {
      std::string ext = path.extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
                     { return static_cast<char>(std::tolower(c)); });
      return ext;
    }

    bool is_hex(char c) noexcept
    {
      return (c >= '0' && c <= '9') ||
             (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F');
    }

    int hex_value(char c) noexcept
    {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      return c - 'A' + 10;
    }

    std::optional<std::string> url_decode_path(std::string_view path)
    {
      std::string out;
      out.reserve(path.size());

      for (std::size_t i = 0; i < path.size(); ++i)
      {
        const char c = path[i];
        if (c == '\0')
        {
          return std::nullopt;
        }
        if (c != '%')
        {
          out.push_back(c);
          continue;
        }
        if (i + 2 >= path.size() || !is_hex(path[i + 1]) || !is_hex(path[i + 2]))
        {
          return std::nullopt;
        }
        const char decoded = static_cast<char>((hex_value(path[i + 1]) << 4) | hex_value(path[i + 2]));
        if (decoded == '\0')
        {
          return std::nullopt;
        }
        out.push_back(decoded);
        i += 2;
      }

      return out;
    }

    bool is_path_inside(const std::filesystem::path &root, const std::filesystem::path &path)
    {
      auto rootIt = root.begin();
      auto pathIt = path.begin();

      for (; rootIt != root.end(); ++rootIt, ++pathIt)
      {
        if (pathIt == path.end() || *rootIt != *pathIt)
        {
          return false;
        }
      }

      return true;
    }

    std::optional<std::filesystem::path> normalized_relative_asset_path(std::string_view requested)
    {
      const auto decoded = url_decode_path(requested);
      if (!decoded)
      {
        return std::nullopt;
      }

      std::string path = *decoded;
      const std::size_t query = path.find('?');
      if (query != std::string::npos)
      {
        path.resize(query);
      }
      const std::size_t fragment = path.find('#');
      if (fragment != std::string::npos)
      {
        path.resize(fragment);
      }

      if (path.empty() || path == "/")
      {
        return std::filesystem::path("index.html");
      }

      while (!path.empty() && path.front() == '/')
      {
        path.erase(path.begin());
      }

      if (path == "index.html")
      {
        return std::filesystem::path("index.html");
      }

      if (path.rfind("assets/", 0) != 0)
      {
        return std::nullopt;
      }

      std::filesystem::path rel(path);
      if (rel.is_absolute())
      {
        return std::nullopt;
      }

      for (const auto &part : rel)
      {
        const std::string s = part.string();
        if (s.empty() || s == "." || s == ".." || s.find('\\') != std::string::npos || s.find('\0') != std::string::npos)
        {
          return std::nullopt;
        }
      }

      return rel;
    }

    void append_if_valid(std::vector<std::filesystem::path> &paths, std::filesystem::path path)
    {
      if (!path.empty())
      {
        paths.push_back(std::move(path));
      }
    }

    std::filesystem::path home_directory()
    {
      if (const char *home = std::getenv("HOME"))
      {
        if (*home != '\0')
        {
          return std::filesystem::path(home);
        }
      }
#if defined(_WIN32)
      if (const char *profile = std::getenv("USERPROFILE"))
      {
        if (*profile != '\0')
        {
          return std::filesystem::path(profile);
        }
      }
#endif
      return {};
    }
  }

  bool NoteAsset::empty() const noexcept
  {
    return content.empty();
  }

  bool NoteAssetResolveResult::found() const noexcept
  {
    return !directory.empty();
  }

  NoteAssets::NoteAssets()
  {
    const auto resolved = resolve_note_asset_directory();
    if (resolved.found())
    {
      std::string err;
      (void)set_root(resolved.directory, &err);
    }
    else
    {
      error_ = resolved.error;
    }
  }

  NoteAssets::NoteAssets(std::filesystem::path root)
  {
    std::string err;
    (void)set_root(std::move(root), &err);
  }

  NoteAssets::NoteAssets(std::vector<NoteAsset> assets)
      : overrides_(std::move(assets))
  {
  }

  const std::filesystem::path &NoteAssets::root() const noexcept
  {
    return root_;
  }

  const std::vector<NoteAsset> &NoteAssets::all() const noexcept
  {
    return overrides_;
  }

  std::size_t NoteAssets::size() const noexcept
  {
    return overrides_.size();
  }

  bool NoteAssets::empty() const noexcept
  {
    return !valid() && overrides_.empty();
  }

  bool NoteAssets::valid() const noexcept
  {
    return !canonicalRoot_.empty();
  }

  const std::string &NoteAssets::error() const noexcept
  {
    return error_;
  }

  bool NoteAssets::set_root(std::filesystem::path root, std::string *error)
  {
    error_.clear();
    root_.clear();
    canonicalRoot_.clear();

    std::string validationError;
    if (!note_asset_directory_is_valid(root, &validationError))
    {
      error_ = validationError;
      if (error)
      {
        *error = validationError;
      }
      return false;
    }

    std::error_code ec;
    canonicalRoot_ = std::filesystem::weakly_canonical(root, ec);
    if (ec || canonicalRoot_.empty())
    {
      error_ = "Unable to canonicalize Vix Note asset directory: " + root.string();
      if (error)
      {
        *error = error_;
      }
      return false;
    }

    root_ = std::move(root);
    if (error)
    {
      error->clear();
    }
    return true;
  }

  std::optional<std::filesystem::path> NoteAssets::resolve(std::string_view relativePath) const
  {
    const auto rel = normalized_relative_asset_path(relativePath);
    if (!rel || canonicalRoot_.empty())
    {
      return std::nullopt;
    }

    std::error_code ec;
    const auto candidate = std::filesystem::weakly_canonical(canonicalRoot_ / *rel, ec);
    if (ec || candidate.empty() || !is_path_inside(canonicalRoot_, candidate))
    {
      return std::nullopt;
    }

    if (!std::filesystem::is_regular_file(candidate, ec) || ec)
    {
      return std::nullopt;
    }

    return candidate;
  }

  std::optional<NoteAsset> NoteAssets::find(std::string_view path) const
  {
    const std::string normalized = normalize_note_asset_path(path);
    const auto overrideIt = std::find_if(overrides_.begin(), overrides_.end(), [&](const NoteAsset &asset)
                                         { return normalize_note_asset_path(asset.path) == normalized; });
    if (overrideIt != overrides_.end())
    {
      return *overrideIt;
    }

    const auto file = resolve(path);
    if (!file)
    {
      return std::nullopt;
    }

    std::string body;
    std::string err;
    if (!read_note_asset_file(*file, body, err))
    {
      return std::nullopt;
    }

    NoteAsset asset;
    asset.path = normalized;
    asset.contentType = content_type_for(*file);
    asset.content = std::move(body);
    return asset;
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
    const auto it = std::find_if(overrides_.begin(), overrides_.end(), [&](const NoteAsset &candidate)
                                 { return candidate.path == asset.path; });
    if (it == overrides_.end())
    {
      overrides_.push_back(std::move(asset));
    }
    else
    {
      *it = std::move(asset);
    }
  }

  bool NoteAssets::load_from_directory(const std::filesystem::path &directory, NoteAssetDirectoryOptions options, std::string &error)
  {
    if (options.clearBeforeLoad)
    {
      overrides_.clear();
    }
    return set_root(directory, &error);
  }

  bool NoteAssets::load_from_directory(const std::filesystem::path &directory, std::string &error)
  {
    return load_from_directory(directory, {}, error);
  }

  bool NoteAssets::remove(std::string_view path)
  {
    const std::string normalized = normalize_note_asset_path(path);
    const auto oldSize = overrides_.size();
    overrides_.erase(
        std::remove_if(overrides_.begin(), overrides_.end(), [&](const NoteAsset &asset)
                       { return normalize_note_asset_path(asset.path) == normalized; }),
        overrides_.end());
    return overrides_.size() != oldSize;
  }

  void NoteAssets::clear()
  {
    overrides_.clear();
    root_.clear();
    canonicalRoot_.clear();
    error_.clear();
  }

  std::vector<NoteAsset> NoteAssets::defaults()
  {
    return {
        NoteAsset{"/", "text/html; charset=utf-8", default_index_html()},
        NoteAsset{"/index.html", "text/html; charset=utf-8", default_index_html()},
        NoteAsset{"/assets/note.css", "text/css; charset=utf-8", default_css()},
        NoteAsset{"/assets/note.js", "application/javascript; charset=utf-8", default_js()},
    };
  }

  std::vector<NoteAsset> NoteAssets::from_directory(const std::filesystem::path &directory, std::string &error)
  {
    NoteAssets assets(directory);
    if (!assets.valid())
    {
      error = assets.error();
      return {};
    }

    std::vector<NoteAsset> out;
    for (std::string_view route : {std::string_view("/"), std::string_view("/index.html"), std::string_view("/assets/note.css"), std::string_view("/assets/note.js")})
    {
      if (auto asset = assets.find(route))
      {
        out.push_back(std::move(*asset));
      }
    }
    error.clear();
    return out;
  }

  std::string NoteAssets::default_index_html()
  {
    return kFallbackHtml;
  }

  std::string NoteAssets::default_css()
  {
    return {};
  }

  std::string NoteAssets::default_js()
  {
    return {};
  }

  std::string NoteAssets::content_type_for(const std::filesystem::path &path)
  {
    const std::string ext = lower_extension(path);
    if (ext == ".html" || ext == ".htm")
      return "text/html; charset=utf-8";
    if (ext == ".css")
      return "text/css; charset=utf-8";
    if (ext == ".js" || ext == ".mjs")
      return "application/javascript; charset=utf-8";
    if (ext == ".json")
      return "application/json; charset=utf-8";
    if (ext == ".svg")
      return "image/svg+xml";
    if (ext == ".png")
      return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
      return "image/jpeg";
    if (ext == ".webp")
      return "image/webp";
    if (ext == ".ico")
      return "image/x-icon";
    if (ext == ".woff")
      return "font/woff";
    if (ext == ".woff2")
      return "font/woff2";
    return "application/octet-stream";
  }

  std::filesystem::path note_installed_asset_directory()
  {
#ifdef VIX_NOTE_INSTALLED_ASSET_DIR
    return std::filesystem::path(VIX_NOTE_INSTALLED_ASSET_DIR);
#else
    return {};
#endif
  }

  std::filesystem::path note_build_asset_directory()
  {
#ifdef VIX_NOTE_BUILD_ASSET_DIR
    return std::filesystem::path(VIX_NOTE_BUILD_ASSET_DIR);
#else
    return {};
#endif
  }

  std::filesystem::path note_source_asset_directory()
  {
#ifdef VIX_NOTE_SOURCE_ASSET_DIR
    return std::filesystem::path(VIX_NOTE_SOURCE_ASSET_DIR);
#else
    return {};
#endif
  }

  std::optional<std::filesystem::path> note_current_executable_path()
  {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0 || len >= buffer.size())
    {
      return std::nullopt;
    }
    buffer.resize(len);
    return std::filesystem::path(buffer);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0)
    {
      return std::nullopt;
    }
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
      return std::nullopt;
    }
    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    std::error_code ec;
    return std::filesystem::weakly_canonical(buffer, ec);
#else
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec || path.empty())
    {
      return std::nullopt;
    }
    return path;
#endif
  }

  std::vector<std::filesystem::path> note_asset_search_paths(
      const NoteAssetResolveOptions &options)
  {
    std::vector<std::filesystem::path> paths;

    // Explicit override always has the highest priority.
    append_if_valid(paths, options.customDirectory);

    // Environment overrides are mainly useful during development.
    if (options.useEnvironmentDirectory)
    {
      if (const char *env = std::getenv("VIX_NOTE_ASSETS_DIR"))
      {
        append_if_valid(paths, env);
      }

      if (const char *legacy = std::getenv("VIX_NOTE_ASSET_DIR"))
      {
        append_if_valid(paths, legacy);
      }
    }

    // Prefer assets installed beside the running Vix executable.
    // /usr/local/bin/vix -> /usr/local/share/vix/note
    if (options.useExecutableRelativeDirectory)
    {
      if (auto exe = note_current_executable_path())
      {
        const auto bin = exe->parent_path();

        append_if_valid(
            paths,
            bin.parent_path() / "share" / "vix" / "note");

        append_if_valid(
            paths,
            bin / ".." / "share" / "vix" / "note");
      }
    }

    // CMake-configured installation path.
    if (options.useInstalledDirectory)
    {
      append_if_valid(
          paths,
          note_installed_asset_directory());
    }

    // User-global Vix installation paths.
    if (options.useGlobalDirectory)
    {
      if (const char *prefix = std::getenv("VIX_GLOBAL_PREFIX"))
      {
        append_if_valid(
            paths,
            std::filesystem::path(prefix) /
                "share" /
                "vix" /
                "note");
      }

      const auto home = home_directory();

      if (!home.empty())
      {
        append_if_valid(
            paths,
            home /
                ".vix" /
                "global" /
                "share" /
                "vix" /
                "note");
      }
    }

    // Source directory fallback for local development.
    if (options.useSourceDirectory)
    {
      append_if_valid(
          paths,
          note_source_asset_directory());
    }

    // Build directory must remain last because it may contain stale assets.
    if (options.useBuildDirectory)
    {
      append_if_valid(
          paths,
          note_build_asset_directory());
    }

    return paths;
  }

  NoteAssetResolveResult resolve_note_asset_directory(const NoteAssetResolveOptions &options)
  {
    NoteAssetResolveResult result;
    result.checked = note_asset_search_paths(options);

    std::ostringstream errors;
    for (const auto &path : result.checked)
    {
      std::string err;
      if (note_asset_directory_is_valid(path, &err))
      {
        std::error_code ec;
        result.directory = std::filesystem::weakly_canonical(path, ec);
        if (!ec && !result.directory.empty())
        {
          result.error.clear();
          return result;
        }
      }
      if (!err.empty())
      {
        errors << "  " << path.string() << ": " << err << '\n';
      }
    }

    result.error = errors.str();
    return result;
  }

  bool load_best_available_note_assets(NoteAssets &assets, const NoteAssetResolveOptions &options, std::string &error)
  {
    const auto resolved = resolve_note_asset_directory(options);
    if (resolved.found())
    {
      assets = NoteAssets(resolved.directory);
      error.clear();
      return true;
    }

    error = resolved.error;
    if (options.keepEmbeddedFallback)
    {
      assets = NoteAssets(NoteAssets::defaults());
      return true;
    }

    assets.clear();
    return false;
  }

  bool read_note_asset_file(const std::filesystem::path &path, std::string &out, std::string &err)
  {
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
      err = "Unable to open asset file: " + path.string();
      return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof())
    {
      err = "Unable to read asset file: " + path.string();
      return false;
    }

    out = buffer.str();
    err.clear();
    return true;
  }

  bool note_asset_directory_is_valid(const std::filesystem::path &directory, std::string *error)
  {
    if (directory.empty())
    {
      if (error)
        *error = "asset directory is empty";
      return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec)
    {
      if (error)
        *error = "asset directory does not exist";
      return false;
    }

    for (const auto &required : {std::filesystem::path("index.html"), std::filesystem::path("assets") / "note.css", std::filesystem::path("assets") / "note.js"})
    {
      if (!std::filesystem::is_regular_file(directory / required, ec) || ec)
      {
        if (error)
          *error = "missing required asset: " + required.generic_string();
        return false;
      }
    }

    if (error)
      error->clear();
    return true;
  }

  std::string note_asset_public_path(const std::filesystem::path &path)
  {
    auto generic = path.generic_string();
    while (!generic.empty() && generic.front() == '/')
    {
      generic.erase(generic.begin());
    }
    if (generic.empty() || generic == "index.html")
    {
      return "/";
    }
    if (generic.rfind("assets/", 0) == 0)
    {
      return "/" + generic;
    }
    if (generic == "note.css")
    {
      return "/assets/note.css";
    }
    if (generic == "note.js")
    {
      return "/assets/note.js";
    }
    return "/assets/" + std::filesystem::path(generic).filename().generic_string();
  }

  std::string note_asset_content_type(std::string_view path)
  {
    return NoteAssets::content_type_for(std::filesystem::path(std::string(path)));
  }

  std::string normalize_note_asset_path(std::string_view path)
  {
    const auto decoded = url_decode_path(path);
    if (!decoded)
    {
      return {};
    }

    std::string value = *decoded;
    const std::size_t query = value.find('?');
    if (query != std::string::npos)
    {
      value.resize(query);
    }
    const std::size_t fragment = value.find('#');
    if (fragment != std::string::npos)
    {
      value.resize(fragment);
    }

    while (!value.empty() && value.front() == '/')
    {
      value.erase(value.begin());
    }

    if (value.empty() || value == "index.html")
    {
      return "/";
    }

    std::filesystem::path rel(value);
    if (rel.is_absolute())
    {
      return {};
    }

    for (const auto &part : rel)
    {
      const std::string s = part.string();
      if (s.empty() || s == "." || s == ".." || s.find('\\') != std::string::npos || s.find('\0') != std::string::npos)
      {
        return {};
      }
    }

    return "/" + rel.generic_string();
  }
}
