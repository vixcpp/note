/**
 *
 *  @file NoteRoutes.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Route resolution helpers for the Vix Note local UI.
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

#ifndef VIX_NOTE_WEB_NOTE_ROUTES_HPP
#define VIX_NOTE_WEB_NOTE_ROUTES_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/runtime/NoteKernel.hpp>
#include <vix/note/web/NoteAssets.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vix::note
{
  /**
   * @brief HTTP-like method used by the local note UI router.
   *
   * NoteRoutes does not depend on a concrete HTTP server. It uses this small
   * enum so the same route layer can later be adapted to Vix web routes,
   * another embedded server, or tests.
   */
  enum class NoteRouteMethod
  {
    /**
     * @brief Unknown or unsupported method.
     */
    Unknown,

    /**
     * @brief GET request.
     */
    Get,

    /**
     * @brief POST request.
     */
    Post,

    /**
     * @brief PUT request.
     */
    Put,

    /**
     * @brief DELETE request.
     */
    Delete
  };

  /**
   * @brief Request object consumed by NoteRoutes.
   */
  struct NoteRouteRequest
  {
    /**
     * @brief Request method.
     */
    NoteRouteMethod method{NoteRouteMethod::Unknown};

    /**
     * @brief Request path.
     */
    std::string path;

    /**
     * @brief Optional request body.
     */
    std::string body;
  };

  /**
   * @brief Response object returned by NoteRoutes.
   */
  struct NoteRouteResponse
  {
    /**
     * @brief HTTP-like response status code.
     */
    int status = 200;

    /**
     * @brief Response content type.
     */
    std::string contentType = "text/plain; charset=utf-8";

    /**
     * @brief Response body.
     */
    std::string body;

    /**
     * @brief Creates a plain text response.
     *
     * @param status Response status code.
     * @param body   Response body.
     * @return Created response.
     */
    static NoteRouteResponse text(int status, std::string body);

    /**
     * @brief Creates a JSON response.
     *
     * @param status Response status code.
     * @param body   JSON response body.
     * @return Created response.
     */
    static NoteRouteResponse json(int status, std::string body);

    /**
     * @brief Creates an asset response.
     *
     * @param asset Asset to return.
     * @return Created response.
     */
    static NoteRouteResponse asset(const NoteAsset &asset);

    /**
     * @brief Checks whether the response is successful.
     *
     * @return True when the status code is between 200 and 299.
     */
    bool ok() const noexcept;
  };

  /**
   * @brief Options controlling NoteRoutes behavior.
   */
  struct NoteRoutesOptions
  {
    /**
     * @brief Enables API routes.
     */
    bool enableApi = true;

    /**
     * @brief Enables static asset routes.
     */
    bool enableAssets = true;
  };

  /**
   * @brief Route resolver for the Vix Note local UI.
   *
   * NoteRoutes maps UI paths to embedded assets and API paths to NoteKernel
   * operations. It intentionally remains independent of any concrete HTTP
   * server so NoteServer can adapt it to the Vix server layer later.
   */
  class NoteRoutes
  {
  public:
    /**
     * @brief Creates routes with a default document and default assets.
     */
    NoteRoutes();

    /**
     * @brief Creates routes for an existing document.
     *
     * @param document Document used by the route kernel.
     */
    explicit NoteRoutes(NoteDocument document);

    /**
     * @brief Creates routes with custom options.
     *
     * @param options Route options.
     */
    explicit NoteRoutes(NoteRoutesOptions options);

    /**
     * @brief Creates routes for an existing document with custom options.
     *
     * @param document Document used by the route kernel.
     * @param options  Route options.
     */
    NoteRoutes(NoteDocument document, NoteRoutesOptions options);

    /**
     * @brief Returns the current route options.
     *
     * @return Route options.
     */
    const NoteRoutesOptions &options() const noexcept;

    /**
     * @brief Replaces the current route options.
     *
     * @param options New route options.
     */
    void set_options(NoteRoutesOptions options) noexcept;

    /**
     * @brief Returns the asset registry.
     *
     * @return Read-only assets.
     */
    const NoteAssets &assets() const noexcept;

    /**
     * @brief Returns the mutable asset registry.
     *
     * @return Mutable assets.
     */
    NoteAssets &assets() noexcept;

    /**
     * @brief Returns the note kernel.
     *
     * @return Read-only kernel.
     */
    const NoteKernel &kernel() const noexcept;

    /**
     * @brief Returns the mutable note kernel.
     *
     * @return Mutable kernel.
     */
    NoteKernel &kernel() noexcept;

    /**
     * @brief Returns the kernel document.
     *
     * @return Read-only document.
     */
    const NoteDocument &document() const noexcept;

    /**
     * @brief Replaces the kernel document.
     *
     * @param document New document.
     */
    void set_document(NoteDocument document);

    /**
     * @brief Handles a route request.
     *
     * @param request Route request.
     * @return Route response.
     */
    NoteRouteResponse handle(const NoteRouteRequest &request);

    /**
     * @brief Handles a GET request.
     *
     * @param path Request path.
     * @return Route response.
     */
    NoteRouteResponse get(std::string_view path);

    /**
     * @brief Handles a POST request.
     *
     * @param path Request path.
     * @param body Request body.
     * @return Route response.
     */
    NoteRouteResponse post(std::string_view path, std::string body = {});

  private:
    /**
     * @brief Handles static asset requests.
     *
     * @param path Request path.
     * @return Route response when an asset is found.
     */
    std::optional<NoteRouteResponse> handle_asset(std::string_view path) const;

    /**
     * @brief Handles API requests.
     *
     * @param request Route request.
     * @return Route response when the path is an API path.
     */
    std::optional<NoteRouteResponse> handle_api(const NoteRouteRequest &request);

    /**
     * @brief Serializes the current document into a small JSON object.
     *
     * @return JSON document summary.
     */
    std::string document_json() const;

    /**
     * @brief Serializes a NoteResult into a small JSON object.
     *
     * @param result Result to serialize.
     * @return JSON response.
     */
    std::string result_json(const NoteResult &result) const;

    /**
     * @brief Serializes a multi-cell kernel run result.
     *
     * @param result Run result to serialize.
     * @return JSON response.
     */
    std::string run_result_json(const NoteKernelRunResult &result) const;

    /**
     * @brief Route options.
     */
    NoteRoutesOptions options_;

    /**
     * @brief Embedded UI assets.
     */
    NoteAssets assets_;

    /**
     * @brief Runtime kernel used by API routes.
     */
    NoteKernel kernel_;
  };

  /**
   * @brief Converts a route method to a stable string.
   *
   * @param method Route method.
   * @return String representation.
   */
  std::string_view to_string(NoteRouteMethod method) noexcept;

  /**
   * @brief Parses a route method from text.
   *
   * @param value Method text.
   * @return Parsed method.
   */
  NoteRouteMethod note_route_method_from_string(std::string_view value) noexcept;

  /**
   * @brief Checks whether a path is a note API path.
   *
   * @param path Path to inspect.
   * @return True when the path starts with `/api/`.
   */
  bool is_note_api_path(std::string_view path) noexcept;
}

#endif // VIX_NOTE_WEB_NOTE_ROUTES_HPP
