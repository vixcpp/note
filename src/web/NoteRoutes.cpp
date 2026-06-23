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

    std::string json_escape(const std::string &value)
    {
      std::string out;
      out.reserve(value.size() + 8);

      for (char c : value)
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

        default:
          out.push_back(c);
          break;
        }
      }

      return out;
    }

    std::optional<std::size_t> parse_cell_index_path(std::string_view path)
    {
      constexpr std::string_view prefix = "/api/cells/";
      constexpr std::string_view suffix = "/run";

      if (!starts_with(path, prefix))
      {
        return std::nullopt;
      }

      if (path.size() <= prefix.size() + suffix.size())
      {
        return std::nullopt;
      }

      if (path.substr(path.size() - suffix.size()) != suffix)
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

      std::size_t value = 0;

      for (char c : raw)
      {
        if (c < '0' || c > '9')
        {
          return std::nullopt;
        }

        value = (value * 10) + static_cast<std::size_t>(c - '0');
      }

      return value;
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

  NoteRoutes::NoteRoutes() = default;

  NoteRoutes::NoteRoutes(NoteDocument document)
      : kernel_(std::move(document))
  {
  }

  NoteRoutes::NoteRoutes(NoteRoutesOptions options)
      : options_(options)
  {
  }

  NoteRoutes::NoteRoutes(NoteDocument document, NoteRoutesOptions options)
      : options_(options),
        kernel_(std::move(document))
  {
  }

  const NoteRoutesOptions &NoteRoutes::options() const noexcept
  {
    return options_;
  }

  void NoteRoutes::set_options(NoteRoutesOptions options) noexcept
  {
    options_ = options;
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
          result.ok ? 200 : 500,
          run_result_json(result));
    }

    if (request.method == NoteRouteMethod::Post)
    {
      const std::optional<std::size_t> index =
          parse_cell_index_path(request.path);

      if (index)
      {
        NoteResult result =
            kernel_.run_cell(*index);

        return NoteRouteResponse::json(
            result.ok() || result.was_skipped() ? 200 : 500,
            cell_run_json(*index, result));
      }
    }

    return NoteRouteResponse::json(
        404,
        "{\"ok\":false,\"error\":\"api route not found\"}");
  }

  std::string NoteRoutes::document_json() const
  {
    const NoteDocument &doc = kernel_.document();

    std::ostringstream out;

    out << "{";
    out << "\"ok\":true,";
    out << "\"title\":\"" << json_escape(doc.title()) << "\",";
    out << "\"path\":\"" << json_escape(doc.path()) << "\",";
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
    out << "\"ok\":" << (result.ok() ? "true" : "false") << ",";
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
