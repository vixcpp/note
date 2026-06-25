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

      for (unsigned char c : value)
      {
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

    if (request.method == NoteRouteMethod::Get &&
        request.path == "/api/document")
    {
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
        return NoteRouteResponse::json(
            403,
            "{\"ok\":false,\"error\":\"save disabled\"}");
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
        return NoteRouteResponse::json(
            403,
            "{\"ok\":false,\"error\":\"file actions disabled\"}");
      }

      return handle_document_new(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/document/open")
    {
      if (!options_.enableFileActions)
      {
        return NoteRouteResponse::json(
            403,
            "{\"ok\":false,\"error\":\"file actions disabled\"}");
      }

      return handle_document_open(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/directory/create")
    {
      if (!options_.enableFileActions)
      {
        return NoteRouteResponse::json(
            403,
            "{\"ok\":false,\"error\":\"file actions disabled\"}");
      }

      return handle_directory_create(request.body);
    }

    if (request.method == NoteRouteMethod::Post &&
        request.path == "/api/cells")
    {
      if (!options_.enableEditing)
      {
        return NoteRouteResponse::json(
            403,
            "{\"ok\":false,\"error\":\"editing disabled\"}");
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
        return NoteRouteResponse::json(
            409,
            "{\"ok\":false,\"error\":\"cell id already exists\"}");
      }

      const NoteCellKind kind =
          json_cell_kind(request.body, NoteCellKind::Markdown);

      const std::string source =
          json_string_field(request.body, "source").value_or(std::string{});

      NoteCell cell(id, kind, source);

      const std::optional<std::size_t> index =
          json_size_field(request.body, "index");

      const bool inserted =
          index
              ? doc.insert_cell(*index, std::move(cell))
              : (doc.add_cell(std::move(cell)), true);

      if (!inserted)
      {
        return NoteRouteResponse::json(
            400,
            "{\"ok\":false,\"error\":\"invalid cell index\"}");
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
          return NoteRouteResponse::json(
              403,
              "{\"ok\":false,\"error\":\"editing disabled\"}");
        }

        NoteDocument &doc =
            kernel_.document();

        NoteCell *cell =
            doc.find_cell(*id);

        if (cell == nullptr)
        {
          return NoteRouteResponse::json(
              404,
              "{\"ok\":false,\"error\":\"cell not found\"}");
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
          return NoteRouteResponse::json(
              403,
              "{\"ok\":false,\"error\":\"editing disabled\"}");
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
          return NoteRouteResponse::json(
              403,
              "{\"ok\":false,\"error\":\"editing disabled\"}");
        }

        const std::optional<std::size_t> index =
            json_size_field(request.body, "index");

        if (!index)
        {
          return NoteRouteResponse::json(
              400,
              "{\"ok\":false,\"error\":\"missing target index\"}");
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

    return NoteRouteResponse::json(
        404,
        "{\"ok\":false,\"error\":\"api route not found\"}");
  }

  NoteRouteResponse NoteRoutes::handle_document_new(std::string_view body)
  {
    const std::string pathText =
        json_string_field(body, "path").value_or(std::string{});

    std::string title =
        json_string_field(body, "title").value_or(std::string{});

    if (pathText.empty())
    {
      return NoteRouteResponse::json(
          400,
          "{\"ok\":false,\"error\":\"missing note path\"}");
    }

    const std::filesystem::path path(pathText);

    if (!is_vixnote_path(path))
    {
      return NoteRouteResponse::json(
          400,
          "{\"ok\":false,\"error\":\"note path must end with .vixnote\"}");
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
      return NoteRouteResponse::json(
          500,
          "{\"ok\":false,\"error\":\"" + json_escape(saved.message()) + "\"}");
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
      return NoteRouteResponse::json(
          400,
          "{\"ok\":false,\"error\":\"missing note path\"}");
    }

    const std::filesystem::path path(pathText);

    if (!is_vixnote_path(path))
    {
      return NoteRouteResponse::json(
          400,
          "{\"ok\":false,\"error\":\"note path must end with .vixnote\"}");
    }

    NoteLoadResult loaded =
        store_.load(path);

    if (!loaded.ok)
    {
      return NoteRouteResponse::json(
          404,
          "{\"ok\":false,\"error\":\"" + json_escape(loaded.error) + "\"}");
    }

    set_document(std::move(loaded.document));

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
      return NoteRouteResponse::json(
          400,
          "{\"ok\":false,\"error\":\"missing directory path\"}");
    }

    const std::filesystem::path path(pathText);

    std::error_code ec;
    std::filesystem::create_directories(path, ec);

    if (ec)
    {
      return NoteRouteResponse::json(
          500,
          "{\"ok\":false,\"error\":\"" + json_escape(ec.message()) + "\"}");
    }

    return NoteRouteResponse::json(
        200,
        "{\"ok\":true,\"path\":\"" + json_escape(path.string()) + "\"}");
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
