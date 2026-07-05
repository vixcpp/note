/**
 *
 *  @file NoteRoutes.cpp
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

#include <vix/note/web/NoteRoutes.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <chrono>
#include <cstdint>

namespace vix::note
{
  namespace
  {
    std::string lower_copy(std::string value)
    {
      for (char &c : value)
      {
        if (c >= 'A' && c <= 'Z')
        {
          c = static_cast<char>(c - 'A' + 'a');
        }
      }

      return value;
    }

    bool starts_with(std::string_view value, std::string_view prefix)
    {
      return value.size() >= prefix.size() &&
             value.substr(0, prefix.size()) == prefix;
    }

    bool ends_with(std::string_view value, std::string_view suffix)
    {
      return value.size() >= suffix.size() &&
             value.substr(value.size() - suffix.size()) == suffix;
    }

    bool is_digits(std::string_view value)
    {
      if (value.empty())
      {
        return false;
      }

      for (char c : value)
      {
        if (c < '0' || c > '9')
        {
          return false;
        }
      }

      return true;
    }

    std::optional<std::size_t> parse_size_value(std::string_view value)
    {
      if (!is_digits(value))
      {
        return std::nullopt;
      }

      std::size_t out = 0;

      for (char c : value)
      {
        out = (out * 10) + static_cast<std::size_t>(c - '0');
      }

      return out;
    }

    std::string json_escape(const std::string &value)
    {
      std::string out;
      out.reserve(value.size() + 8);

      const char hex[] = "0123456789abcdef";

      for (char raw_c : value)
      {
        const auto c = static_cast<unsigned char>(raw_c);
        switch (c)
        {
        case '\\':
          out += "\\\\";
          break;

        case '"':
          out += "\\\"";
          break;

        case '\n':
          out += "\\n";
          break;

        case '\r':
          out += "\\r";
          break;

        case '\t':
          out += "\\t";
          break;

        case '\b':
          out += "\\b";
          break;

        case '\f':
          out += "\\f";
          break;

        default:
          if (c < 0x20)
          {
            out += "\\u00";
            out.push_back(hex[(c >> 4) & 0x0F]);
            out.push_back(hex[c & 0x0F]);
          }
          else
          {
            out.push_back(static_cast<char>(c));
          }

          break;
        }
      }

      return out;
    }

    std::string json_unescape(std::string_view value)
    {
      std::string out;
      out.reserve(value.size());

      bool escaping = false;

      for (char c : value)
      {
        if (escaping)
        {
          switch (c)
          {
          case 'n':
            out += '\n';
            break;

          case 'r':
            out += '\r';
            break;

          case 't':
            out += '\t';
            break;

          case '\\':
            out += '\\';
            break;

          case '"':
            out += '"';
            break;

          default:
            out += c;
            break;
          }

          escaping = false;
          continue;
        }

        if (c == '\\')
        {
          escaping = true;
          continue;
        }

        out += c;
      }

      if (escaping)
      {
        out += '\\';
      }

      return out;
    }

    std::optional<std::string> json_string_field(
        std::string_view json,
        std::string_view key)
    {
      const std::string pattern =
          "\"" + std::string(key) + "\"";

      std::size_t pos =
          json.find(pattern);

      if (pos == std::string_view::npos)
      {
        return std::nullopt;
      }

      pos = json.find(':', pos + pattern.size());

      if (pos == std::string_view::npos)
      {
        return std::nullopt;
      }

      ++pos;

      while (pos < json.size() &&
             (json[pos] == ' ' ||
              json[pos] == '\t' ||
              json[pos] == '\n' ||
              json[pos] == '\r'))
      {
        ++pos;
      }

      if (pos >= json.size() || json[pos] != '"')
      {
        return std::nullopt;
      }

      ++pos;

      std::string raw;
      bool escaping = false;

      while (pos < json.size())
      {
        const char c = json[pos++];

        if (escaping)
        {
          raw += '\\';
          raw += c;
          escaping = false;
          continue;
        }

        if (c == '\\')
        {
          escaping = true;
          continue;
        }

        if (c == '"')
        {
          return json_unescape(raw);
        }

        raw += c;
      }

      return std::nullopt;
    }

    std::optional<std::size_t> json_size_field(
        std::string_view json,
        std::string_view key)
    {
      const std::string pattern =
          "\"" + std::string(key) + "\"";

      std::size_t pos =
          json.find(pattern);

      if (pos == std::string_view::npos)
      {
        return std::nullopt;
      }

      pos = json.find(':', pos + pattern.size());

      if (pos == std::string_view::npos)
      {
        return std::nullopt;
      }

      ++pos;

      while (pos < json.size() &&
             (json[pos] == ' ' ||
              json[pos] == '\t' ||
              json[pos] == '\n' ||
              json[pos] == '\r'))
      {
        ++pos;
      }

      const std::size_t begin = pos;

      while (pos < json.size() &&
             json[pos] >= '0' &&
             json[pos] <= '9')
      {
        ++pos;
      }

      if (begin == pos)
      {
        return std::nullopt;
      }

      return parse_size_value(json.substr(begin, pos - begin));
    }

    std::optional<bool> json_bool_field(
        std::string_view json,
        std::string_view key)
    {
      const std::string pattern =
          "\"" + std::string(key) + "\"";

      std::size_t pos =
          json.find(pattern);

      if (pos == std::string_view::npos)
      {
        return std::nullopt;
      }

      pos = json.find(':', pos + pattern.size());

      if (pos == std::string_view::npos)
      {
        return std::nullopt;
      }

      ++pos;

      while (pos < json.size() &&
             (json[pos] == ' ' ||
              json[pos] == '\t' ||
              json[pos] == '\n' ||
              json[pos] == '\r'))
      {
        ++pos;
      }

      if (json.substr(pos, 4) == "true")
      {
        return true;
      }

      if (json.substr(pos, 5) == "false")
      {
        return false;
      }

      return std::nullopt;
    }

    std::optional<std::string> parse_cell_run_path(std::string_view path)
    {
      constexpr std::string_view prefix = "/api/cells/";
      constexpr std::string_view suffix = "/run";

      if (!starts_with(path, prefix) || !ends_with(path, suffix))
      {
        return std::nullopt;
      }

      if (path.size() <= prefix.size() + suffix.size())
      {
        return std::nullopt;
      }

      const std::string_view raw =
          path.substr(
              prefix.size(),
              path.size() - prefix.size() - suffix.size());

      if (raw.empty())
      {
        return std::nullopt;
      }

      return std::string(raw);
    }

    std::optional<std::string> parse_cell_move_path(std::string_view path)
    {
      constexpr std::string_view prefix = "/api/cells/";
      constexpr std::string_view suffix = "/move";

      if (!starts_with(path, prefix) || !ends_with(path, suffix))
      {
        return std::nullopt;
      }

      if (path.size() <= prefix.size() + suffix.size())
      {
        return std::nullopt;
      }

      const std::string_view raw =
          path.substr(
              prefix.size(),
              path.size() - prefix.size() - suffix.size());

      if (raw.empty())
      {
        return std::nullopt;
      }

      return std::string(raw);
    }

    std::optional<std::string> parse_cell_id_path(std::string_view path)
    {
      constexpr std::string_view prefix = "/api/cells/";

      if (!starts_with(path, prefix))
      {
        return std::nullopt;
      }

      if (path.size() <= prefix.size())
      {
        return std::nullopt;
      }

      const std::string_view raw =
          path.substr(prefix.size());

      if (raw.empty() ||
          raw.find('/') != std::string_view::npos)
      {
        return std::nullopt;
      }

      return std::string(raw);
    }

    NoteCellKind json_cell_kind(
        std::string_view body,
        NoteCellKind fallback = NoteCellKind::Markdown)
    {
      const std::optional<std::string> kind =
          json_string_field(body, "kind");

      if (!kind)
      {
        return fallback;
      }

      const NoteCellKind parsed =
          note_cell_kind_from_string(*kind);

      if (parsed == NoteCellKind::Unknown)
      {
        return fallback;
      }

      return parsed;
    }

    std::string make_unique_cell_id(const NoteDocument &doc)
    {
      std::size_t index = doc.cell_count() + 1;

      while (true)
      {
        const std::string id =
            "cell-" + std::to_string(index);

        if (doc.find_cell(id) == nullptr)
        {
          return id;
        }

        ++index;
      }
    }

    bool document_has_cell_id(
        const NoteDocument &doc,
        const std::string &id)
    {
      return doc.find_cell(id) != nullptr;
    }

    bool is_vixnote_path(const std::filesystem::path &path)
    {
      return path.extension() == ".vixnote";
    }

    std::string default_title_from_path(const std::filesystem::path &path)
    {
      const std::string stem = path.stem().string();

      if (!stem.empty())
      {
        return stem;
      }

      return "Untitled Note";
    }

    bool is_safe_relative_path(const std::filesystem::path &path)
    {
      if (path.empty())
      {
        return true;
      }

      if (path.is_absolute())
      {
        return false;
      }

      for (const auto &part : path)
      {
        const std::string value = part.string();

        if (value == "..")
        {
          return false;
        }
      }

      return true;
    }

    std::filesystem::path normalize_workspace_path(const std::string &pathText)
    {
      if (pathText.empty())
      {
        return std::filesystem::path(".");
      }

      return std::filesystem::path(pathText).lexically_normal();
    }

    std::int64_t file_time_to_epoch_ms(
        const std::filesystem::file_time_type &time)
    {
      const auto systemTime =
          std::chrono::time_point_cast<std::chrono::system_clock::duration>(
              time - std::filesystem::file_time_type::clock::now() +
              std::chrono::system_clock::now());

      return std::chrono::duration_cast<std::chrono::milliseconds>(
                 systemTime.time_since_epoch())
          .count();
    }

    std::int64_t last_write_time_ms(const std::filesystem::path &path)
    {
      std::error_code ec;

      const auto time =
          std::filesystem::last_write_time(path, ec);

      if (ec)
      {
        return 0;
      }

      return file_time_to_epoch_ms(time);
    }

    std::uintmax_t safe_file_size(const std::filesystem::path &path)
    {
      std::error_code ec;

      const std::uintmax_t size =
          std::filesystem::file_size(path, ec);

      if (ec)
      {
        return 0;
      }

      return size;
    }

    /**
     * @brief Builds a `{ "ok": false, "error": "..." }` response.
     */
    NoteRouteResponse json_error(int status, std::string_view message)
    {
      std::ostringstream out;

      out << "{\"ok\":false,\"error\":\""
          << json_escape(std::string(message))
          << "\"}";

      return NoteRouteResponse::json(status, out.str());
    }

    /**
     * @brief Builds a success response for a path action.
     *
     * Shape: `{ "ok": true, "path": "...", "newPath": "...",
     *           "type": "file|dir", "message": "..." }`.
     */
    NoteRouteResponse json_path_action(
        const std::filesystem::path &oldPath,
        const std::filesystem::path &newPath,
        std::string_view type,
        std::string_view message)
    {
      std::ostringstream out;

      out << "{";
      out << "\"ok\":true,";
      out << "\"path\":\"" << json_escape(oldPath.generic_string()) << "\",";
      out << "\"newPath\":\"" << json_escape(newPath.generic_string()) << "\",";
      out << "\"type\":\"" << json_escape(std::string(type)) << "\",";
      out << "\"message\":\"" << json_escape(std::string(message)) << "\"";
      out << "}";

      return NoteRouteResponse::json(200, out.str());
    }


    std::string directory_entry_json(
        const std::filesystem::directory_entry &entry)
    {
      std::error_code ec;

      const std::filesystem::path path =
          entry.path().lexically_normal();

      const std::string name =
          path.filename().string();

      const bool directory =
          entry.is_directory(ec);

      ec.clear();

      const bool regularFile =
          entry.is_regular_file(ec);

      const std::string extension =
          regularFile ? path.extension().string() : std::string{};

      const bool noteFile =
          regularFile && extension == ".vixnote";

      std::ostringstream out;

      out << "{";
      out << "\"name\":\"" << json_escape(name) << "\",";
      out << "\"path\":\"" << json_escape(path.generic_string()) << "\",";
      out << "\"type\":\"" << (directory ? "dir" : "file") << "\",";
      out << "\"extension\":\"" << json_escape(extension) << "\",";
      out << "\"size\":" << (regularFile ? safe_file_size(path) : 0) << ",";
      out << "\"modified\":" << last_write_time_ms(path) << ",";
      out << "\"openable\":" << (noteFile ? "true" : "false");
      out << "}";

      return out.str();
    }

    bool should_hide_directory_entry(
        const std::filesystem::directory_entry &entry)
    {
      const std::string name =
          entry.path().filename().string();

      return name == ".vix" ||
             name == ".git" ||
             name == ".vix-scripts" ||
             name == "build" ||
             name == "cmake-build-debug" ||
             name == "cmake-build-release";
    }

    bool is_startup_scratch_document(const NoteDocument &doc)
    {
      const std::string title =
          lower_copy(doc.title());

      const std::filesystem::path normalizedPath =
          normalize_workspace_path(doc.path());

      const std::string path =
          lower_copy(normalizedPath.generic_string());

      if (!(title == "tmp" ||
            title == "untitled" ||
            title == "untitled note"))
      {
        return false;
      }

      if (!(path.empty() ||
            path == "." ||
            path == "untitled.vixnote" ||
            path == "untitled"))
      {
        return false;
      }

      if (doc.cell_count() > 1)
      {
        return false;
      }

      if (doc.cell_count() == 0)
      {
        return true;
      }

      const NoteCell *cell = doc.cell_at(0);

      if (cell == nullptr)
      {
        return true;
      }

      const std::string source =
          lower_copy(cell->source());

      return source.find("start writing your note here") != std::string::npos ||
             source.find("start writing your lesson here") != std::string::npos ||
             source.find("# tmp") != std::string::npos;
    }

    std::string no_open_document_json()
    {
      return "{\"ok\":true,\"hasDocument\":false,\"document\":null}";
    }
  }

  NoteRouteResponse NoteRouteResponse::text(int status, std::string body)
  {
    NoteRouteResponse response;
    response.status = status;
    response.contentType = "text/plain; charset=utf-8";
    response.body = std::move(body);
    return response;
  }

  NoteRouteResponse NoteRouteResponse::json(int status, std::string body)
  {
    NoteRouteResponse response;
    response.status = status;
    response.contentType = "application/json; charset=utf-8";
    response.body = std::move(body);
    return response;
  }

  NoteRouteResponse NoteRouteResponse::asset(const NoteAsset &asset)
  {
    NoteRouteResponse response;
    response.status = 200;
    response.contentType = asset.contentType;
    response.body = asset.content;
    return response;
  }

  bool NoteRouteResponse::ok() const noexcept
  {
    return status >= 200 && status < 300;
  }

  NoteRoutes::NoteRoutes()
  {
    sync_assets();
  }

  NoteRoutes::NoteRoutes(NoteDocument document)
      : kernel_(std::move(document))
  {
    sync_assets();
  }

  NoteRoutes::NoteRoutes(NoteRoutesOptions options)
      : options_(std::move(options)),
        kernel_(options_.kernelOptions)
  {
    sync_assets();
  }

  NoteRoutes::NoteRoutes(NoteDocument document, NoteRoutesOptions options)
      : options_(std::move(options)),
        kernel_(std::move(document), options_.kernelOptions)
  {
    sync_assets();
  }

  const NoteRoutesOptions &NoteRoutes::options() const noexcept
  {
    return options_;
  }

  void NoteRoutes::set_options(NoteRoutesOptions options) noexcept
  {
    options_ = std::move(options);
    kernel_.set_options(options_.kernelOptions);
    sync_assets();
  }

  void NoteRoutes::sync_assets()
  {
    NoteAssetResolveOptions assetOptions;
    assetOptions.customDirectory = options_.assetDirectory;
    assetOptions.useInstalledDirectory = options_.loadInstalledAssets;
    assetOptions.keepEmbeddedFallback = options_.keepEmbeddedAssetFallback;

    std::string error;
    (void)load_best_available_note_assets(
        assets_,
        assetOptions,
        error);
  }

  const NoteAssets &NoteRoutes::assets() const noexcept
  {
    return assets_;
  }

  NoteAssets &NoteRoutes::assets() noexcept
  {
    return assets_;
  }

  const NoteKernel &NoteRoutes::kernel() const noexcept
  {
    return kernel_;
  }

  NoteKernel &NoteRoutes::kernel() noexcept
  {
    return kernel_;
  }

  const NoteStore &NoteRoutes::store() const noexcept
  {
    return store_;
  }

  NoteStore &NoteRoutes::store() noexcept
  {
    return store_;
  }

  const NoteDocument &NoteRoutes::document() const noexcept
  {
    return kernel_.document();
  }

  void NoteRoutes::set_document(NoteDocument document)
  {
    kernel_.set_document(std::move(document));
  }

  bool NoteRoutes::is_current_document_path(
      const std::filesystem::path &normalized) const
  {
    const std::string &current = kernel_.document().path();

    if (current.empty())
    {
      return false;
    }

    return normalize_workspace_path(current) == normalized;
  }

  void NoteRoutes::update_current_document_path_if_needed(
      const std::filesystem::path &oldPath,
      const std::filesystem::path &newPath)
  {
    if (is_current_document_path(oldPath))
    {
      kernel_.document().set_path(newPath.string());
    }
  }

  NoteRouteResponse NoteRoutes::handle(const NoteRouteRequest &request)
  {
    if (options_.enableApi)
    {
      if (auto response = handle_api(request))
      {
        return *response;
      }
    }

    if (request.method == NoteRouteMethod::Get && options_.enableAssets)
    {
      if (auto response = handle_asset(request.path))
      {
        return *response;
      }
    }

    return NoteRouteResponse::text(404, "not found");
  }

  NoteRouteResponse NoteRoutes::get(std::string_view path)
  {
    return handle(
        NoteRouteRequest{
            NoteRouteMethod::Get,
            std::string(path),
            {}});
  }

  NoteRouteResponse NoteRoutes::post(std::string_view path, std::string body)
  {
    return handle(
        NoteRouteRequest{
            NoteRouteMethod::Post,
            std::string(path),
            std::move(body)});
  }

  NoteRouteResponse NoteRoutes::put(std::string_view path, std::string body)
  {
    return handle(
        NoteRouteRequest{
            NoteRouteMethod::Put,
            std::string(path),
            std::move(body)});
  }

  NoteRouteResponse NoteRoutes::delete_request(std::string_view path)
  {
    return handle(
        NoteRouteRequest{
            NoteRouteMethod::Delete,
            std::string(path),
            {}});
  }

  std::optional<NoteRouteResponse> NoteRoutes::handle_asset(std::string_view path) const
  {
    std::optional<NoteAsset> asset =
        assets_.find(path);

    if (!asset)
    {
      return std::nullopt;
    }

    return NoteRouteResponse::asset(*asset);
  }

  std::optional<NoteRouteResponse> NoteRoutes::handle_api(const NoteRouteRequest &request)
  {
    if (!is_note_api_path(request.path))
    {
      return std::nullopt;
    }

    // ----------------------------------------------------------------
    // Document routes
    // ----------------------------------------------------------------
    if (request.method == NoteRouteMethod::Get &&
        request.path == "/api/document")
    {
      if (is_startup_scratch_document(kernel_.document()))
      {
        return NoteRouteResponse::json(
            200,
            no_open_document_json());
      }

      return NoteRouteResponse::json(200, document_json());
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/run-all")
    {
      NoteKernelRunResult result =
          kernel_.run_all();

      return NoteRouteResponse::json(
          200,
          run_result_json(result));
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/document/save")
    {
      if (!options_.enableSave)
      {
        return json_error(403, "save disabled");
      }

      if (is_startup_scratch_document(kernel_.document()))
      {
        return json_error(409, "no open note to save");
      }

      NoteResult result =
          store_.save(kernel_.document());

      return NoteRouteResponse::json(
          result.ok() ? 200 : 500,
          save_result_json(result));
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/document/new")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_document_new(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/document/open")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_document_open(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/document/update")
    {
      if (!options_.enableEditing)
      {
        return json_error(403, "editing disabled");
      }

      return handle_document_update(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/document/save-as")
    {
      if (!options_.enableSave || !options_.enableFileActions)
      {
        return json_error(403, "save-as disabled");
      }

      return handle_document_save_as(request.body);
    }

    // ----------------------------------------------------------------
    // Directory + path routes
    // ----------------------------------------------------------------
    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/directory/create")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_directory_create(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/directory/list")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_directory_list(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/path/delete")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_path_delete(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/path/rename")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_path_rename(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/path/move")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_path_move(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/path/copy")
    {
      if (!options_.enableFileActions)
      {
        return json_error(403, "file actions disabled");
      }

      return handle_path_copy(request.body);
    }

    // ----------------------------------------------------------------
    // Cell routes (unchanged — these feed the notebook editor)
    // ----------------------------------------------------------------
    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/cells")
    {
      if (!options_.enableEditing)
      {
        return json_error(403, "editing disabled");
      }

      if (is_startup_scratch_document(kernel_.document()))
      {
        return json_error(409, "no open note; create or open a note first");
      }

      NoteDocument &doc =
          kernel_.document();

      std::string id =
          json_string_field(request.body, "id").value_or(std::string{});

      if (id.empty())
      {
        id = make_unique_cell_id(doc);
      }

      if (document_has_cell_id(doc, id))
      {
        return json_error(409, "cell id already exists");
      }

      const NoteCellKind kind =
          json_cell_kind(request.body, NoteCellKind::Cpp);

      const std::string source =
          json_string_field(request.body, "source").value_or(std::string{});

      NoteCell cell(id, kind, source);

      const std::optional<std::size_t> index =
          json_size_field(request.body, "index");

      bool inserted = false;

      if (index)
      {
        inserted = doc.insert_cell(*index, std::move(cell));
      }
      else
      {
        doc.add_cell(std::move(cell));
        inserted = true;
      }

      if (!inserted)
      {
        return json_error(400, "invalid cell index");
      }

      return NoteRouteResponse::json(
          200,
          cell_mutation_json(true, "cell added", id));
    }

    if (request.method == NoteRouteMethod::Put)
    {
      const std::optional<std::string> id =
          parse_cell_id_path(request.path);

      if (id)
      {
        if (!options_.enableEditing)
        {
          return json_error(403, "editing disabled");
        }

        NoteDocument &doc =
            kernel_.document();

        NoteCell *cell =
            doc.find_cell(*id);

        if (cell == nullptr)
        {
          return json_error(404, "cell not found");
        }

        const NoteCellKind kind =
            json_cell_kind(request.body, cell->kind());

        const std::string source =
            json_string_field(request.body, "source").value_or(cell->source());

        const bool updated =
            doc.update_cell(*id, kind, source);

        return NoteRouteResponse::json(
            updated ? 200 : 404,
            cell_mutation_json(updated, updated ? "cell updated" : "cell not found", *id));
      }
    }

    if (request.method == NoteRouteMethod::Delete)
    {
      const std::optional<std::string> id =
          parse_cell_id_path(request.path);

      if (id)
      {
        if (!options_.enableEditing)
        {
          return json_error(403, "editing disabled");
        }

        const bool removed =
            kernel_.document().remove_cell_by_id(*id);

        return NoteRouteResponse::json(
            removed ? 200 : 404,
            cell_mutation_json(removed, removed ? "cell deleted" : "cell not found", *id));
      }
    }

    if (request.method == NoteRouteMethod::Post)
    {
      const std::optional<std::string> moveId =
          parse_cell_move_path(request.path);

      if (moveId)
      {
        if (!options_.enableEditing)
        {
          return json_error(403, "editing disabled");
        }

        const std::optional<std::size_t> index =
            json_size_field(request.body, "index");

        if (!index)
        {
          return json_error(400, "missing target index");
        }

        const bool moved =
            kernel_.document().move_cell(*moveId, *index);

        return NoteRouteResponse::json(
            moved ? 200 : 404,
            cell_mutation_json(moved, moved ? "cell moved" : "cell not found or invalid index", *moveId));
      }

      const std::optional<std::string> runId =
          parse_cell_run_path(request.path);

      if (runId)
      {
        NoteResult result;

        std::optional<std::size_t> index;

        if (is_digits(*runId))
        {
          index = parse_size_value(*runId);
          result = kernel_.run_cell(*index);
        }
        else
        {
          index = kernel_.cell_index(*runId);
          result = kernel_.run_cell(*runId);
        }

        const std::size_t responseIndex =
            index.value_or(kernel_.cell_count());

        return NoteRouteResponse::json(
            200,
            cell_run_json(responseIndex, result));
      }
    }

    return json_error(404, "api route not found");
  }

  NoteRouteResponse NoteRoutes::handle_document_new(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    std::string title =
        json_string_field(body, "title").value_or(std::string{});

    if (pathText.empty())
    {
      return json_error(400, "missing note path");
    }

    const std::filesystem::path path =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(path))
    {
      return json_error(400, "unsafe path");
    }

    if (!is_vixnote_path(path))
    {
      return json_error(400, "note path must end with .vixnote");
    }

    std::error_code existsError;

    if (std::filesystem::exists(path, existsError))
    {
      return json_error(409, "note file already exists");
    }

    if (existsError)
    {
      return json_error(500, existsError.message());
    }

    if (title.empty())
    {
      title = default_title_from_path(path);
    }

    NoteDocument document(title);
    document.set_path(path.string());

    document.add_cell(
        NoteCell(
            "intro",
            NoteCellKind::Markdown,
            "# " + title + "\n\nStart writing your lesson here."));

    const NoteResult saved =
        store_.save(document, path);

    if (!saved.ok())
    {
      return json_error(500, saved.message());
    }

    set_document(std::move(document));

    return NoteRouteResponse::json(
        200,
        document_json());
  }

  NoteRouteResponse NoteRoutes::handle_document_open(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    if (pathText.empty())
    {
      return json_error(400, "missing note path");
    }

    const std::filesystem::path path =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(path))
    {
      return json_error(400, "unsafe path");
    }

    if (!is_vixnote_path(path))
    {
      return json_error(400, "note path must end with .vixnote");
    }

    NoteLoadResult loaded =
        store_.load(path);

    if (!loaded.ok)
    {
      return json_error(404, loaded.error);
    }

    set_document(std::move(loaded.document));

    return NoteRouteResponse::json(
        200,
        document_json());
  }

  NoteRouteResponse NoteRoutes::handle_document_update(std::string_view body)
  {
    const std::optional<std::string> title =
        json_string_field(body, "title");

    if (title)
    {
      kernel_.document().set_title(*title);
    }

    const bool save =
        json_bool_field(body, "save").value_or(false);

    if (save && !kernel_.document().path().empty())
    {
      const NoteResult result =
          store_.save(kernel_.document());

      if (!result.ok())
      {
        return json_error(500, result.message());
      }
    }

    return NoteRouteResponse::json(
        200,
        document_json());
  }

  NoteRouteResponse NoteRoutes::handle_document_save_as(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    const std::optional<std::string> title =
        json_string_field(body, "title");

    if (pathText.empty())
    {
      return json_error(400, "missing note path");
    }

    const std::filesystem::path path =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(path))
    {
      return json_error(400, "unsafe path");
    }

    if (!is_vixnote_path(path))
    {
      return json_error(400, "note path must end with .vixnote");
    }

    if (title)
    {
      kernel_.document().set_title(*title);
    }

    const NoteResult saved =
        store_.save(kernel_.document(), path);

    if (!saved.ok())
    {
      return json_error(500, saved.message());
    }

    kernel_.document().set_path(path.string());

    return NoteRouteResponse::json(
        200,
        document_json());
  }

  NoteRouteResponse NoteRoutes::handle_directory_create(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    if (pathText.empty())
    {
      return json_error(400, "missing directory path");
    }

    const std::filesystem::path path =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(path))
    {
      return json_error(400, "unsafe directory path");
    }

    std::error_code ec;
    std::filesystem::create_directories(path, ec);

    if (ec)
    {
      return json_error(500, ec.message());
    }

    std::ostringstream out;

    out << "{\"ok\":true,\"path\":\""
        << json_escape(path.generic_string())
        << "\"}";

    return NoteRouteResponse::json(200, out.str());
  }

  NoteRouteResponse NoteRoutes::handle_path_delete(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    const bool recursive =
        json_bool_field(body, "recursive").value_or(false);

    if (pathText.empty())
    {
      return json_error(400, "empty path");
    }

    const std::filesystem::path path =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(path))
    {
      return json_error(400, "unsafe path");
    }

    std::error_code ec;

    if (!std::filesystem::exists(path, ec))
    {
      return json_error(404, "path not found");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    const bool deletingCurrentDocument = is_current_document_path(path);

    std::uintmax_t removed = 0;

    if (std::filesystem::is_directory(path, ec))
    {
      if (ec)
      {
        return json_error(500, ec.message());
      }

      if (recursive)
      {
        removed = std::filesystem::remove_all(path, ec);
      }
      else
      {
        const bool ok =
            std::filesystem::remove(path, ec);

        removed = ok ? 1 : 0;
      }
    }
    else
    {
      const bool ok =
          std::filesystem::remove(path, ec);

      removed = ok ? 1 : 0;
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    if (deletingCurrentDocument)
    {
      NoteDocument empty =
          NoteDocument::create("Untitled Note");

      empty.set_path({});
      kernel_.set_document(std::move(empty));
    }

    std::ostringstream out;

    out << "{";
    out << "\"ok\":true,";
    out << "\"path\":\"" << json_escape(path.generic_string()) << "\",";
    out << "\"removed\":" << removed << ",";
    out << "\"currentDeleted\":" << (deletingCurrentDocument ? "true" : "false");
    out << "}";

    return NoteRouteResponse::json(200, out.str());
  }

  NoteRouteResponse NoteRoutes::handle_path_rename(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    if (pathText.empty())
    {
      return json_error(400, "empty path");
    }

    const std::filesystem::path source =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(source))
    {
      return json_error(400, "unsafe path");
    }

    // Resolve the destination from either "newName" or "newPath".
    std::filesystem::path destination;

    const std::optional<std::string> newName =
        json_string_field(body, "newName");

    const std::optional<std::string> newPath =
        json_string_field(body, "newPath");

    if (newName && !newName->empty())
    {
      const std::filesystem::path nameOnly(*newName);

      // A rename keeps the file inside its current parent directory, so the
      // provided name must be a bare file/folder name, not a nested path.
      if (nameOnly.has_parent_path())
      {
        return json_error(400, "newName must be a bare name, not a path");
      }

      destination = source.parent_path() / nameOnly;
    }
    else if (newPath && !newPath->empty())
    {
      destination = normalize_workspace_path(*newPath);
    }
    else
    {
      return json_error(400, "missing newName or newPath");
    }

    destination = destination.lexically_normal();

    if (!is_safe_relative_path(destination))
    {
      return json_error(400, "unsafe destination path");
    }

    std::error_code ec;

    if (!std::filesystem::exists(source, ec))
    {
      return json_error(404, "path not found");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    const bool sourceIsDir =
        std::filesystem::is_directory(source, ec);

    if (ec)
    {
      return json_error(500, ec.message());
    }

    // A .vixnote file must stay a .vixnote file after rename.
    if (!sourceIsDir &&
        is_vixnote_path(source) &&
        !is_vixnote_path(destination))
    {
      return json_error(400, "renamed note must keep the .vixnote extension");
    }

    if (std::filesystem::exists(destination, ec))
    {
      return json_error(409, "destination already exists");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    std::filesystem::rename(source, destination, ec);

    if (ec)
    {
      return json_error(500, ec.message());
    }

    update_current_document_path_if_needed(source, destination);

    return json_path_action(
        source,
        destination,
        sourceIsDir ? "dir" : "file",
        "path renamed");
  }

  NoteRouteResponse NoteRoutes::handle_path_move(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    if (pathText.empty())
    {
      return json_error(400, "empty path");
    }

    const std::filesystem::path source =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(source))
    {
      return json_error(400, "unsafe path");
    }

    // Resolve the destination from either "directory" or "newPath".
    std::filesystem::path destination;

    const std::optional<std::string> directory =
        json_string_field(body, "directory");

    const std::optional<std::string> newPath =
        json_string_field(body, "newPath");

    if (directory)
    {
      const std::filesystem::path targetDir =
          normalize_workspace_path(*directory);

      if (!is_safe_relative_path(targetDir))
      {
        return json_error(400, "unsafe destination directory");
      }

      destination = (targetDir / source.filename()).lexically_normal();
    }
    else if (newPath && !newPath->empty())
    {
      destination = normalize_workspace_path(*newPath);
    }
    else
    {
      return json_error(400, "missing directory or newPath");
    }

    if (!is_safe_relative_path(destination))
    {
      return json_error(400, "unsafe destination path");
    }

    std::error_code ec;

    if (!std::filesystem::exists(source, ec))
    {
      return json_error(404, "path not found");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    const bool sourceIsDir =
        std::filesystem::is_directory(source, ec);

    if (ec)
    {
      return json_error(500, ec.message());
    }

    if (!sourceIsDir &&
        is_vixnote_path(source) &&
        !is_vixnote_path(destination))
    {
      return json_error(400, "moved note must keep the .vixnote extension");
    }

    if (std::filesystem::exists(destination, ec))
    {
      return json_error(409, "destination already exists");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    // Make sure the destination parent directory exists.
    const std::filesystem::path destParent =
        destination.parent_path();

    if (!destParent.empty())
    {
      std::filesystem::create_directories(destParent, ec);

      if (ec)
      {
        return json_error(500, ec.message());
      }
    }

    std::filesystem::rename(source, destination, ec);

    if (ec)
    {
      // rename() can fail across devices; fall back to copy + remove.
      std::error_code copyError;

      std::filesystem::copy(
          source,
          destination,
          std::filesystem::copy_options::recursive,
          copyError);

      if (copyError)
      {
        return json_error(500, copyError.message());
      }

      std::error_code removeError;
      std::filesystem::remove_all(source, removeError);

      if (removeError)
      {
        return json_error(500, removeError.message());
      }
    }

    update_current_document_path_if_needed(source, destination);

    return json_path_action(
        source,
        destination,
        sourceIsDir ? "dir" : "file",
        "path moved");
  }

  NoteRouteResponse NoteRoutes::handle_path_copy(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    const std::string newPathText =
        json_string_field(body, "newPath").value_or(std::string{});

    const bool recursive =
        json_bool_field(body, "recursive").value_or(false);

    if (pathText.empty())
    {
      return json_error(400, "empty path");
    }

    if (newPathText.empty())
    {
      return json_error(400, "missing newPath");
    }

    const std::filesystem::path source =
        normalize_workspace_path(pathText);

    const std::filesystem::path destination =
        normalize_workspace_path(newPathText);

    if (!is_safe_relative_path(source) ||
        !is_safe_relative_path(destination))
    {
      return json_error(400, "unsafe path");
    }

    std::error_code ec;

    if (!std::filesystem::exists(source, ec))
    {
      return json_error(404, "path not found");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    const bool sourceIsDir =
        std::filesystem::is_directory(source, ec);

    if (ec)
    {
      return json_error(500, ec.message());
    }

    if (!sourceIsDir &&
        is_vixnote_path(source) &&
        !is_vixnote_path(destination))
    {
      return json_error(400, "copied note must keep the .vixnote extension");
    }

    if (std::filesystem::exists(destination, ec))
    {
      return json_error(409, "destination already exists");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    const std::filesystem::path destParent =
        destination.parent_path();

    if (!destParent.empty())
    {
      std::filesystem::create_directories(destParent, ec);

      if (ec)
      {
        return json_error(500, ec.message());
      }
    }

    if (sourceIsDir)
    {
      const std::filesystem::copy_options copyOptions =
          recursive
              ? std::filesystem::copy_options::recursive
              : std::filesystem::copy_options::none;

      std::filesystem::copy(source, destination, copyOptions, ec);
    }
    else
    {
      std::filesystem::copy_file(source, destination, ec);
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    return json_path_action(
        source,
        destination,
        sourceIsDir ? "dir" : "file",
        "path copied");
  }

  NoteRouteResponse NoteRoutes::handle_directory_list(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{"."});

    const std::filesystem::path path =
        normalize_workspace_path(pathText);

    if (!is_safe_relative_path(path))
    {
      return json_error(400, "unsafe directory path");
    }

    std::error_code ec;

    if (!std::filesystem::exists(path, ec))
    {
      return json_error(404, "directory not found");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    if (!std::filesystem::is_directory(path, ec))
    {
      return json_error(400, "path is not a directory");
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    std::vector<std::filesystem::directory_entry> entries;

    for (std::filesystem::directory_iterator it(path, ec), end;
         it != end;
         it.increment(ec))
    {
      if (ec)
      {
        break;
      }

      if (should_hide_directory_entry(*it))
      {
        continue;
      }

      entries.push_back(*it);
    }

    if (ec)
    {
      return json_error(500, ec.message());
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const std::filesystem::directory_entry &a,
           const std::filesystem::directory_entry &b)
        {
          std::error_code aec;
          std::error_code bec;

          const bool aDir = a.is_directory(aec);
          const bool bDir = b.is_directory(bec);

          if (aDir != bDir)
          {
            return aDir;
          }

          return lower_copy(a.path().filename().string()) <
                 lower_copy(b.path().filename().string());
        });

    std::ostringstream out;

    out << "{";
    out << "\"ok\":true,";
    out << "\"path\":\"" << json_escape(path.generic_string()) << "\",";
    out << "\"entries\":[";

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
      if (i > 0)
      {
        out << ",";
      }

      out << directory_entry_json(entries[i]);
    }

    out << "]";
    out << "}";

    return NoteRouteResponse::json(200, out.str());
  }

  std::string NoteRoutes::document_json() const
  {
    const NoteDocument &doc = kernel_.document();

    std::ostringstream out;

    out << "{";
    out << "\"ok\":true,";
    out << "\"title\":\"" << json_escape(doc.title()) << "\",";
    out << "\"path\":\"" << json_escape(doc.path()) << "\",";
    out << "\"project\":" << project_context_json() << ",";
    out << "\"cellCount\":" << doc.cell_count() << ",";
    out << "\"executionCount\":" << doc.execution_count() << ",";
    out << "\"cells\":[";

    for (std::size_t i = 0; i < doc.cells().size(); ++i)
    {
      if (i > 0)
      {
        out << ",";
      }

      out << cell_json(doc.cells()[i], i);
    }

    out << "]";
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::project_context_json() const
  {
    const ProjectContext &context =
        kernel_.project_context();

    std::ostringstream out;

    out << "{";
    out << "\"enabled\":" << (context.enabled ? "true" : "false") << ",";
    out << "\"projectName\":\"" << json_escape(context.projectName) << "\",";
    out << "\"notePath\":\"" << json_escape(context.notePath.string()) << "\",";
    out << "\"projectRoot\":\"" << json_escape(context.projectRoot.string()) << "\",";
    out << "\"workingDirectory\":\"" << json_escape(context.effective_working_directory().string()) << "\",";
    out << "\"manifestPath\":\"" << json_escape(context.manifestPath.string()) << "\",";
    out << "\"depsDirectory\":\"" << json_escape(context.depsDirectory.string()) << "\",";
    out << "\"includePaths\":[";

    for (std::size_t i = 0; i < context.includePaths.size(); ++i)
    {
      if (i > 0)
      {
        out << ",";
      }

      out << "\""
          << json_escape(context.includePaths[i].string())
          << "\"";
    }

    out << "]";
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::cell_json(
      const NoteCell &cell,
      std::size_t index) const
  {
    std::ostringstream out;

    out << "{";
    out << "\"index\":" << index << ",";
    out << "\"id\":\"" << json_escape(cell.id()) << "\",";
    out << "\"kind\":\"" << to_string(cell.kind()) << "\",";
    out << "\"title\":\"" << json_escape(cell.title()) << "\",";
    out << "\"source\":\"" << json_escape(cell.source()) << "\",";
    out << "\"executionCount\":" << cell.execution_count() << ",";
    out << "\"executable\":" << (cell.executable() ? "true" : "false") << ",";
    out << "\"outputCount\":" << cell.outputs().size() << ",";
    out << "\"outputs\":" << outputs_json(cell.outputs());
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::output_json(const NoteOutput &output) const
  {
    std::ostringstream out;

    out << "{";
    out << "\"kind\":\"" << to_string(output.kind) << "\",";
    out << "\"content\":\"" << json_escape(output.content) << "\"";
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::outputs_json(const std::vector<NoteOutput> &outputs) const
  {
    std::ostringstream out;

    out << "[";

    for (std::size_t i = 0; i < outputs.size(); ++i)
    {
      if (i > 0)
      {
        out << ",";
      }

      out << output_json(outputs[i]);
    }

    out << "]";

    return out.str();
  }

  std::string NoteRoutes::result_json(const NoteResult &result) const
  {
    std::ostringstream out;

    out << "{";
    out << "\"ok\":" << (result.ok() ? "true" : "false") << ",";
    out << "\"status\":\"" << to_string(result.status()) << "\",";
    out << "\"message\":\"" << json_escape(result.message()) << "\",";
    out << "\"exitCode\":" << result.exit_code() << ",";
    out << "\"outputCount\":" << result.outputs().size() << ",";
    out << "\"outputs\":" << outputs_json(result.outputs());
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::cell_run_json(
      std::size_t index,
      const NoteResult &result) const
  {
    const NoteDocument &doc = kernel_.document();
    const NoteCell *cell = doc.cell_at(index);

    std::ostringstream out;

    out << "{";
    out << "\"ok\":" << ((result.ok() || result.was_skipped()) ? "true" : "false") << ",";
    out << "\"result\":" << result_json(result) << ",";
    out << "\"cell\":";

    if (cell == nullptr)
    {
      out << "null";
    }
    else
    {
      out << cell_json(*cell, index);
    }

    out << ",";
    out << "\"document\":" << document_json();
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::cell_mutation_json(
      bool ok,
      std::string_view message,
      std::string_view cellId) const
  {
    const NoteDocument &doc = kernel_.document();
    const NoteCell *cell = cellId.empty()
                               ? nullptr
                               : doc.find_cell(std::string(cellId));

    std::ostringstream out;

    out << "{";
    out << "\"ok\":" << (ok ? "true" : "false") << ",";
    out << "\"message\":\"" << json_escape(std::string(message)) << "\",";
    out << "\"cellId\":\"" << json_escape(std::string(cellId)) << "\",";
    out << "\"cell\":";

    if (cell == nullptr)
    {
      out << "null";
    }
    else
    {
      const std::optional<std::size_t> index =
          doc.cell_index(std::string(cellId));

      out << cell_json(*cell, index.value_or(0));
    }

    out << ",";
    out << "\"document\":" << document_json();
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::save_result_json(const NoteResult &result) const
  {
    std::ostringstream out;

    out << "{";
    out << "\"ok\":" << (result.ok() ? "true" : "false") << ",";
    out << "\"result\":" << result_json(result) << ",";
    out << "\"document\":" << document_json();
    out << "}";

    return out.str();
  }

  std::string NoteRoutes::run_result_json(const NoteKernelRunResult &result) const
  {
    std::ostringstream out;

    out << "{";
    out << "\"ok\":" << (result.ok ? "true" : "false") << ",";
    out << "\"stopped\":" << (result.stopped ? "true" : "false") << ",";
    out << "\"visited\":" << result.visited << ",";
    out << "\"executed\":" << result.executed << ",";
    out << "\"skipped\":" << result.skipped << ",";
    out << "\"failed\":" << result.failed << ",";
    out << "\"results\":[";

    for (std::size_t i = 0; i < result.results.size(); ++i)
    {
      if (i > 0)
      {
        out << ",";
      }

      out << result_json(result.results[i]);
    }

    out << "],";
    out << "\"document\":" << document_json();
    out << "}";

    return out.str();
  }

  std::string_view to_string(NoteRouteMethod method) noexcept
  {
    switch (method)
    {
    case NoteRouteMethod::Unknown:
      return "unknown";

    case NoteRouteMethod::Get:
      return "GET";

    case NoteRouteMethod::Post:
      return "POST";

    case NoteRouteMethod::Put:
      return "PUT";

    case NoteRouteMethod::Delete:
      return "DELETE";
    }

    return "unknown";
  }

  NoteRouteMethod note_route_method_from_string(std::string_view value) noexcept
  {
    const std::string normalized =
        lower_copy(std::string(value));

    if (normalized == "get")
    {
      return NoteRouteMethod::Get;
    }

    if (normalized == "post")
    {
      return NoteRouteMethod::Post;
    }

    if (normalized == "put")
    {
      return NoteRouteMethod::Put;
    }

    if (normalized == "delete")
    {
      return NoteRouteMethod::Delete;
    }

    return NoteRouteMethod::Unknown;
  }

  bool is_note_api_path(std::string_view path) noexcept
  {
    return starts_with(path, "/api/");
  }
}
