/**
 *
 *  @file NoteServer.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Local server facade for the Vix Note UI.
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

#ifndef VIX_NOTE_WEB_NOTE_SERVER_HPP
#define VIX_NOTE_WEB_NOTE_SERVER_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/web/NoteRoutes.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace vix::note
{
  /**
   * @brief Internal runtime used by the concrete local HTTP server.
   *
   * The implementation is hidden from the public header so platform sockets,
   * threads, and low-level server details stay inside NoteServer.cpp.
   */
  class NoteServerRuntime;

  /**
   * @brief Runtime state of the Vix Note local server.
   */
  enum class NoteServerState
  {
    /**
     * @brief The server is not running.
     */
    Stopped,

    /**
     * @brief The server has been started.
     */
    Running
  };

  /**
   * @brief Options used by the Vix Note local server.
   *
   * The server listens on a local HTTP address, serves embedded note assets,
   * and forwards API requests to NoteRoutes. The low-level networking backend
   * is kept private to NoteServer.cpp.
   */
  struct NoteServerOptions
  {
    /**
     * @brief Hostname used to build local URLs.
     */
    std::string host = "127.0.0.1";

    /**
     * @brief Port used to build local URLs.
     */
    std::uint16_t port = 5179;

    /**
     * @brief Opens the browser after server startup when supported.
     */
    bool openBrowser = false;

    /**
     * @brief Prints simple local server request logs to stderr.
     */
    bool logRequests = false;

    /**
     * @brief Route behavior used by the server.
     */
    NoteRoutesOptions routeOptions;
  };

  /**
   * @brief Local UI server for Vix Note.
   *
   * NoteServer owns the route resolver and starts a small local HTTP server
   * for the browser UI. It keeps the public API independent from the concrete
   * socket implementation so tests can still call handle(), get(), post(),
   * put(), and delete_request() directly without opening a network port.
   */
  class NoteServer
  {
  public:
    /**
     * @brief Creates a server with default options.
     */
    NoteServer();

    /**
     * @brief Creates a server for an existing document.
     *
     * @param document Document served by the local UI.
     */
    explicit NoteServer(NoteDocument document);

    /**
     * @brief Creates a server with custom options.
     *
     * @param options Server options.
     */
    explicit NoteServer(NoteServerOptions options);

    /**
     * @brief Creates a server for an existing document with custom options.
     *
     * @param document Document served by the local UI.
     * @param options  Server options.
     */
    NoteServer(NoteDocument document, NoteServerOptions options);

    /**
     * @brief Stops the server and releases internal runtime resources.
     */
    ~NoteServer();

    /**
     * @brief NoteServer owns a runtime and cannot be copied.
     */
    NoteServer(const NoteServer &) = delete;

    /**
     * @brief NoteServer owns a runtime and cannot be copied.
     */
    NoteServer &operator=(const NoteServer &) = delete;

    /**
     * @brief Moves a note server.
     */
    NoteServer(NoteServer &&other) noexcept;

    /**
     * @brief Moves a note server.
     */
    NoteServer &operator=(NoteServer &&other) noexcept;

    /**
     * @brief Returns the current server options.
     *
     * @return Server options.
     */
    const NoteServerOptions &options() const noexcept;

    /**
     * @brief Replaces the current server options.
     *
     * This is only allowed while the server is stopped.
     *
     * @param options New server options.
     * @return Result describing whether the update succeeded.
     */
    NoteResult set_options(NoteServerOptions options);

    /**
     * @brief Returns the current server state.
     *
     * @return Server state.
     */
    NoteServerState state() const noexcept;

    /**
     * @brief Checks whether the server is running.
     *
     * @return True when the state is Running.
     */
    bool running() const noexcept;

    /**
     * @brief Checks whether the server is stopped.
     *
     * @return True when the state is Stopped.
     */
    bool stopped() const noexcept;

    /**
     * @brief Starts the local HTTP server.
     *
     * The server begins listening on options().host and options().port.
     * Use wait() when the caller wants to keep the process alive until the
     * server is stopped.
     *
     * @return Start result.
     */
    NoteResult start();

    /**
     * @brief Blocks until the local server stops.
     *
     * This is mainly used by the CLI command after start() succeeds.
     *
     * @return Wait result.
     */
    NoteResult wait();

    /**
     * @brief Stops the local HTTP server.
     *
     * @return Stop result.
     */
    NoteResult stop();

    /**
     * @brief Restarts the local HTTP server.
     *
     * @return Restart result.
     */
    NoteResult restart();

    /**
     * @brief Returns the local UI URL.
     *
     * @return URL string.
     */
    std::string url() const;

    /**
     * @brief Returns the route resolver.
     *
     * @return Read-only routes.
     */
    const NoteRoutes &routes() const noexcept;

    /**
     * @brief Returns the mutable route resolver.
     *
     * @return Mutable routes.
     */
    NoteRoutes &routes() noexcept;

    /**
     * @brief Returns the served document.
     *
     * @return Read-only document.
     */
    const NoteDocument &document() const noexcept;

    /**
     * @brief Replaces the served document.
     *
     * @param document New document.
     */
    void set_document(NoteDocument document);

    /**
     * @brief Handles a route request through the server routes.
     *
     * This helper is mainly used by tests and by future server adapters.
     *
     * @param request Route request.
     * @return Route response.
     */
    NoteRouteResponse handle(const NoteRouteRequest &request);

    /**
     * @brief Handles a GET request through the server routes.
     *
     * @param path Request path.
     * @return Route response.
     */
    NoteRouteResponse get(std::string_view path);

    /**
     * @brief Handles a POST request through the server routes.
     *
     * @param path Request path.
     * @param body Request body.
     * @return Route response.
     */
    NoteRouteResponse post(std::string_view path, std::string body = {});

    /**
     * @brief Handles a PUT request through the server routes.
     *
     * @param path Request path.
     * @param body Request body.
     * @return Route response.
     */
    NoteRouteResponse put(std::string_view path, std::string body = {});

    /**
     * @brief Handles a DELETE request through the server routes.
     *
     * @param path Request path.
     * @return Route response.
     */
    NoteRouteResponse delete_request(std::string_view path);

  private:
    /**
     * @brief Server options.
     */
    NoteServerOptions options_;

    /**
     * @brief Route resolver used by the server.
     */
    NoteRoutes routes_;

    /**
     * @brief Serializes API access to routes_, kernel_, and the active document.
     */
    std::shared_ptr<std::mutex> routeMutex_{std::make_shared<std::mutex>()};

    /**
     * @brief Current server state.
     */
    NoteServerState state_{NoteServerState::Stopped};

    /**
     * @brief Concrete local HTTP runtime.
     */
    std::unique_ptr<NoteServerRuntime> runtime_;
  };

  /**
   * @brief Converts a NoteServerState to a stable string name.
   *
   * @param state Server state.
   * @return String representation.
   */
  std::string_view to_string(NoteServerState state) noexcept;

  /**
   * @brief Builds a local note server URL.
   *
   * @param host Hostname.
   * @param port Port.
   * @return URL string.
   */
  std::string make_note_server_url(
      std::string_view host,
      std::uint16_t port);
}

#endif // VIX_NOTE_WEB_NOTE_SERVER_HPP
