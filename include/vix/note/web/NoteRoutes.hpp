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
#include <vix/note/storage/NoteStore.hpp>
#include <vix/note/web/NoteAssets.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
     * @brief Additional HTTP headers returned by the route.
     */
    std::vector<std::pair<std::string, std::string>> headers;

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

    /**
     * @brief Enables document editing API routes.
     */
    bool enableEditing = true;

    /**
     * @brief Enables saving the current document back to disk.
     *
     * Saving only works when the document has a non-empty path.
     */
    bool enableSave = true;

    /**
     * @brief Enables local note file actions such as creating/opening notes.
     */
    bool enableFileActions = true;

    /**
     * @brief Optional UI asset directory.
     *
     * When empty, Vix Note tries the environment, build-tree, executable-relative,
     * installed, and global asset directories.
     */
    std::filesystem::path assetDirectory;

    /**
     * @brief Loads installed UI assets when available.
     */
    bool loadInstalledAssets = true;

    /**
     * @brief Keeps a minimal error page as fallback.
     */
    bool keepEmbeddedAssetFallback = false;

    /**
     * @brief Kernel options used by route execution.
     */
    NoteKernelOptions kernelOptions;
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
     * @brief Returns the note store used by save routes.
     *
     * @return Read-only store.
     */
    const NoteStore &store() const noexcept;

    /**
     * @brief Returns the mutable note store used by save routes.
     *
     * @return Mutable store.
     */
    NoteStore &store() noexcept;

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

    /**
     * @brief Handles a PUT request.
     *
     * @param path Request path.
     * @param body Request body.
     * @return Route response.
     */
    NoteRouteResponse put(std::string_view path, std::string body = {});

    /**
     * @brief Handles a DELETE request.
     *
     * @param path Request path.
     * @return Route response.
     */
    NoteRouteResponse delete_request(std::string_view path);

  private:
    /**
     * @brief Synchronizes the asset registry from route options.
     */
    void sync_assets();

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
     * @brief Creates a new .vixnote file and makes it the active document.
     */
    NoteRouteResponse handle_document_new(std::string_view body);

    /**
     * @brief Opens an existing .vixnote file and makes it the active document.
     */
    NoteRouteResponse handle_document_open(std::string_view body);

    /**
     * @brief Updates the active document metadata (e.g. its title) in memory.
     *
     * When the body contains `"save": true`, the document is also persisted
     * to disk if it already has a path.
     */
    NoteRouteResponse handle_document_update(std::string_view body);

    /**
     * @brief Saves the active document to a new .vixnote path.
     *
     * On success the active document path is updated to the new path.
     */
    NoteRouteResponse handle_document_save_as(std::string_view body);

    /**
     * @brief Creates a directory on disk.
     */
    NoteRouteResponse handle_directory_create(std::string_view body);

    /**
     * @brief Lists files and directories from a local workspace path.
     */
    NoteRouteResponse handle_directory_list(std::string_view body);

    /**
     * @brief Deletes a local file or directory from disk.
     */
    NoteRouteResponse handle_path_delete(std::string_view body);

    /**
     * @brief Renames a local file or directory on disk.
     *
     * Accepts either `"newName"` (rename in place) or `"newPath"` (full
     * destination). Renaming a `.vixnote` file must keep the `.vixnote`
     * extension.
     */
    NoteRouteResponse handle_path_rename(std::string_view body);

    /**
     * @brief Moves a local file or directory into a new location on disk.
     *
     * Accepts either `"directory"` (target folder, keeps the file name) or
     * `"newPath"` (full destination).
     */
    NoteRouteResponse handle_path_move(std::string_view body);

    /**
     * @brief Copies a local file or directory on disk.
     *
     * Directories are copied recursively when `"recursive": true`.
     */
    NoteRouteResponse handle_path_copy(std::string_view body);

    /**
     * @brief Serializes the current document into a JSON object.
     *
     * The returned document includes metadata, cells, execution counts, and
     * cell outputs so the browser UI can render the full note state.
     *
     * @return JSON document response.
     */
    std::string document_json() const;

    /**
     * @brief Serializes the current project context into JSON.
     *
     * @return JSON project context object.
     */
    std::string project_context_json() const;

    std::string extensions_json() const;

    /**
     * @brief Serializes one note cell into JSON.
     *
     * @param cell  Cell to serialize.
     * @param index Zero-based cell index.
     * @return JSON cell object.
     */
    std::string cell_json(
        const NoteCell &cell,
        std::size_t index) const;

    /**
     * @brief Serializes one note output into JSON.
     *
     * @param output Output to serialize.
     * @return JSON output object.
     */
    std::string output_json(const NoteOutput &output) const;

    /**
     * @brief Serializes a note output list into JSON.
     *
     * @param outputs Outputs to serialize.
     * @return JSON output array.
     */
    std::string outputs_json(const std::vector<NoteOutput> &outputs) const;

    /**
     * @brief Serializes a NoteResult into JSON.
     *
     * @param result Result to serialize.
     * @return JSON result object.
     */
    std::string result_json(const NoteResult &result) const;

    /**
     * @brief Serializes the response returned after running one cell.
     *
     * The response includes the execution result and the updated cell state.
     *
     * @param index  Cell index that was requested.
     * @param result Execution result.
     * @return JSON response.
     */
    std::string cell_run_json(
        std::size_t index,
        const NoteResult &result) const;

    /**
     * @brief Serializes a cell mutation response.
     *
     * The response includes a success flag, message, changed cell when
     * available, and the updated document.
     *
     * @param ok      Whether the mutation succeeded.
     * @param message Human-readable message.
     * @param cellId  Changed cell id when available.
     * @return JSON response.
     */
    std::string cell_mutation_json(
        bool ok,
        std::string_view message,
        std::string_view cellId = {}) const;

    /**
     * @brief Serializes a document save response.
     *
     * @param result Storage save result.
     * @return JSON response.
     */
    std::string save_result_json(const NoteResult &result) const;

    /**
     * @brief Serializes a multi-cell kernel run result.
     *
     * The response includes the run summary, individual results, and the
     * updated document state.
     *
     * @param result Run result to serialize.
     * @return JSON response.
     */
    std::string run_result_json(const NoteKernelRunResult &result) const;

    /**
     * @brief Returns true when the given normalized path is the active
     *        document path.
     *
     * @param normalized A path already passed through lexically_normal().
     * @return True when it matches the active document path.
     */
    bool is_current_document_path(
        const std::filesystem::path &normalized) const;

    /**
     * @brief Updates the active document path when its file was renamed or
     *        moved on disk.
     *
     * @param oldPath Previous normalized path.
     * @param newPath New normalized path.
     */
    void update_current_document_path_if_needed(
        const std::filesystem::path &oldPath,
        const std::filesystem::path &newPath);

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

    /**
     * @brief Storage helper used by document save routes.
     */
    NoteStore store_;
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
