/**
 *
 *  @file NoteServer.cpp
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

#include <vix/note/web/NoteServer.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#else
#include <cerrno>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace vix::note
{
  namespace
  {
#ifdef _WIN32
    using NoteSocket = SOCKET;

    constexpr NoteSocket invalid_socket_value = INVALID_SOCKET;

    bool socket_platform_start(std::string &err)
    {
      err.clear();

      WSADATA data;
      const int rc = WSAStartup(MAKEWORD(2, 2), &data);

      if (rc != 0)
      {
        err = "cannot initialize socket platform";
        return false;
      }

      return true;
    }

    void socket_platform_stop()
    {
      WSACleanup();
    }

    void close_note_socket(NoteSocket socket)
    {
      if (socket != invalid_socket_value)
      {
        closesocket(socket);
      }
    }

    int last_socket_error()
    {
      return WSAGetLastError();
    }

    std::string socket_error_message()
    {
      return "socket error " + std::to_string(last_socket_error());
    }
#else
    using NoteSocket = int;

    constexpr NoteSocket invalid_socket_value = -1;

    bool socket_platform_start(std::string &err)
    {
      err.clear();
      return true;
    }

    void socket_platform_stop()
    {
    }

    void close_note_socket(NoteSocket socket)
    {
      if (socket != invalid_socket_value)
      {
        close(socket);
      }
    }

    int last_socket_error()
    {
      return errno;
    }

    std::string socket_error_message()
    {
      return std::strerror(last_socket_error());
    }
#endif

    std::string trim_copy(std::string_view value)
    {
      std::size_t begin = 0;

      while (begin < value.size() &&
             (value[begin] == ' ' ||
              value[begin] == '\t' ||
              value[begin] == '\n' ||
              value[begin] == '\r'))
      {
        ++begin;
      }

      std::size_t end = value.size();

      while (end > begin &&
             (value[end - 1] == ' ' ||
              value[end - 1] == '\t' ||
              value[end - 1] == '\n' ||
              value[end - 1] == '\r'))
      {
        --end;
      }

      return std::string(value.substr(begin, end - begin));
    }

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

    std::string reason_phrase(int status)
    {
      switch (status)
      {
      case 200:
        return "OK";

      case 201:
        return "Created";

      case 204:
        return "No Content";

      case 400:
        return "Bad Request";

      case 403:
        return "Forbidden";

      case 404:
        return "Not Found";

      case 405:
        return "Method Not Allowed";

      case 500:
        return "Internal Server Error";

      default:
        if (status >= 200 && status < 300)
        {
          return "OK";
        }

        if (status >= 400 && status < 500)
        {
          return "Bad Request";
        }

        if (status >= 500)
        {
          return "Internal Server Error";
        }

        return "OK";
      }
    }

    std::string normalize_http_path(std::string path)
    {
      if (path.empty())
      {
        return "/";
      }

      const std::size_t query = path.find('?');

      if (query != std::string::npos)
      {
        path.erase(query);
      }

      if (path.empty())
      {
        return "/";
      }

      return path;
    }

    std::size_t parse_content_length(const std::string &headers)
    {
      std::istringstream in(headers);
      std::string line;

      while (std::getline(in, line))
      {
        const std::size_t colon = line.find(':');

        if (colon == std::string::npos)
        {
          continue;
        }

        std::string key =
            lower_copy(trim_copy(std::string_view(line).substr(0, colon)));

        if (key != "content-length")
        {
          continue;
        }

        const std::string raw =
            trim_copy(std::string_view(line).substr(colon + 1));

        try
        {
          return static_cast<std::size_t>(std::stoull(raw));
        }
        catch (...)
        {
          return 0;
        }
      }

      return 0;
    }

    bool recv_more(NoteSocket socket, std::string &buffer)
    {
      char chunk[4096];

#ifdef _WIN32
      const int n = recv(socket, chunk, static_cast<int>(sizeof(chunk)), 0);
#else
      const ssize_t n = recv(socket, chunk, sizeof(chunk), 0);
#endif

      if (n <= 0)
      {
        return false;
      }

      buffer.append(chunk, static_cast<std::size_t>(n));
      return true;
    }

    bool read_http_request(
        NoteSocket socket,
        NoteRouteRequest &request)
    {
      request = NoteRouteRequest{};

      std::string raw;
      constexpr std::size_t maxRequestSize = 1024 * 1024;

      std::size_t headerEnd = std::string::npos;

      while (raw.size() < maxRequestSize)
      {
        headerEnd = raw.find("\r\n\r\n");

        if (headerEnd != std::string::npos)
        {
          break;
        }

        if (!recv_more(socket, raw))
        {
          return false;
        }
      }

      if (headerEnd == std::string::npos)
      {
        return false;
      }

      const std::string headerText = raw.substr(0, headerEnd);
      const std::size_t bodyStart = headerEnd + 4;
      const std::size_t contentLength = parse_content_length(headerText);

      while (raw.size() < bodyStart + contentLength &&
             raw.size() < maxRequestSize)
      {
        if (!recv_more(socket, raw))
        {
          return false;
        }
      }

      std::istringstream headers(headerText);

      std::string requestLine;

      if (!std::getline(headers, requestLine))
      {
        return false;
      }

      std::istringstream firstLine(requestLine);

      std::string method;
      std::string path;
      std::string version;

      firstLine >> method >> path >> version;

      if (method.empty() || path.empty())
      {
        return false;
      }

      request.method = note_route_method_from_string(method);
      request.path = normalize_http_path(std::move(path));

      if (contentLength > 0 &&
          raw.size() >= bodyStart)
      {
        const std::size_t available =
            raw.size() - bodyStart;

        request.body =
            raw.substr(
                bodyStart,
                available < contentLength ? available : contentLength);
      }

      return true;
    }

    std::string make_http_response(const NoteRouteResponse &response)
    {
      std::ostringstream out;

      out << "HTTP/1.1 "
          << response.status
          << " "
          << reason_phrase(response.status)
          << "\r\n";

      out << "Content-Type: "
          << response.contentType
          << "\r\n";

      out << "Content-Length: "
          << response.body.size()
          << "\r\n";

      out << "Connection: close\r\n";
      out << "Cache-Control: no-store\r\n";
      out << "\r\n";
      out << response.body;

      return out.str();
    }

    bool send_all(NoteSocket socket, const std::string &payload)
    {
      std::size_t sent = 0;

      while (sent < payload.size())
      {
        const char *data = payload.data() + sent;
        const std::size_t remaining = payload.size() - sent;

#ifdef _WIN32
        const int n =
            send(
                socket,
                data,
                static_cast<int>(remaining),
                0);
#else
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif

        const ssize_t n =
            send(
                socket,
                data,
                remaining,
                flags);
#endif

        if (n <= 0)
        {
          return false;
        }

        sent += static_cast<std::size_t>(n);
      }

      return true;
    }

    NoteSocket create_listening_socket(
        std::string_view host,
        std::uint16_t port,
        std::string &err)
    {
      err.clear();

      addrinfo hints{};
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;

      addrinfo *addresses = nullptr;

      const std::string service = std::to_string(port);
      const std::string hostString(host);

      const int rc =
          getaddrinfo(
              hostString.c_str(),
              service.c_str(),
              &hints,
              &addresses);

      if (rc != 0)
      {
#ifdef _WIN32
        err = "cannot resolve note server address";
#else
        err = std::string("cannot resolve note server address: ") +
              gai_strerror(rc);
#endif
        return invalid_socket_value;
      }

      NoteSocket serverSocket = invalid_socket_value;

      for (addrinfo *address = addresses;
           address != nullptr;
           address = address->ai_next)
      {
        NoteSocket candidate =
            socket(
                address->ai_family,
                address->ai_socktype,
                address->ai_protocol);

        if (candidate == invalid_socket_value)
        {
          continue;
        }

        int yes = 1;

        setsockopt(
            candidate,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char *>(&yes),
            sizeof(yes));

        if (bind(candidate, address->ai_addr, address->ai_addrlen) == 0 &&
            listen(candidate, 16) == 0)
        {
          serverSocket = candidate;
          break;
        }

        close_note_socket(candidate);
      }

      freeaddrinfo(addresses);

      if (serverSocket == invalid_socket_value)
      {
        err = "cannot bind note server on " +
              std::string(host) +
              ":" +
              std::to_string(port) +
              ": " +
              socket_error_message();
      }

      return serverSocket;
    }

    void shutdown_note_socket(NoteSocket socket)
    {
      if (socket == invalid_socket_value)
      {
        return;
      }

#ifdef _WIN32
      shutdown(socket, SD_BOTH);
#else
      shutdown(socket, SHUT_RDWR);
#endif
    }

    void configure_client_socket(NoteSocket socket)
    {
      if (socket == invalid_socket_value)
      {
        return;
      }

#ifdef _WIN32
      DWORD timeoutMs = 5000;

      setsockopt(
          socket,
          SOL_SOCKET,
          SO_RCVTIMEO,
          reinterpret_cast<const char *>(&timeoutMs),
          sizeof(timeoutMs));

      setsockopt(
          socket,
          SOL_SOCKET,
          SO_SNDTIMEO,
          reinterpret_cast<const char *>(&timeoutMs),
          sizeof(timeoutMs));
#else
      timeval timeout{};
      timeout.tv_sec = 5;

      setsockopt(
          socket,
          SOL_SOCKET,
          SO_RCVTIMEO,
          &timeout,
          sizeof(timeout));

      setsockopt(
          socket,
          SOL_SOCKET,
          SO_SNDTIMEO,
          &timeout,
          sizeof(timeout));
#endif
    }

    [[nodiscard]] std::string note_env_or_empty(const char *name)
    {
      if (name == nullptr)
      {
        return {};
      }

#if defined(_WIN32)
      char *buffer = nullptr;
      std::size_t length = 0;

      if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr)
      {
        return {};
      }

      std::string value(buffer);
      std::free(buffer);

      return value;
#else
      const char *value = std::getenv(name);

      if (value == nullptr)
      {
        return {};
      }

      return std::string(value);
#endif
    }

    bool note_runtime_color_enabled()
    {
      const std::string noColor = note_env_or_empty("NO_COLOR");

      if (!noColor.empty())
      {
        return false;
      }

      const std::string rawColor = note_env_or_empty("VIX_COLOR");

      if (!rawColor.empty())
      {
        std::string value = lower_copy(rawColor);

        if (value == "never" ||
            value == "0" ||
            value == "false" ||
            value == "off")
        {
          return false;
        }

        if (value == "always" ||
            value == "1" ||
            value == "true" ||
            value == "on")
        {
          return true;
        }
      }

      return true;
    }

    std::string_view note_reset()
    {
      return note_runtime_color_enabled() ? "\x1b[0m" : "";
    }

    std::string_view note_dim()
    {
      return note_runtime_color_enabled() ? "\x1b[2m" : "";
    }

    std::string_view note_gray()
    {
      return note_runtime_color_enabled() ? "\x1b[90m" : "";
    }


    std::string_view note_cyan()
    {
      return note_runtime_color_enabled() ? "\x1b[36m" : "";
    }

    std::string_view note_green()
    {
      return note_runtime_color_enabled() ? "\x1b[32m" : "";
    }

    std::string_view note_yellow()
    {
      return note_runtime_color_enabled() ? "\x1b[33m" : "";
    }

    std::string_view note_red()
    {
      return note_runtime_color_enabled() ? "\x1b[31m" : "";
    }

    std::string_view note_level_color(int status)
    {
      if (status >= 500)
      {
        return note_red();
      }

      if (status >= 400)
      {
        return note_yellow();
      }

      return note_green();
    }

    std::string_view note_status_color(int status)
    {
      if (status >= 200 && status < 300)
      {
        return note_green();
      }

      if (status >= 300 && status < 400)
      {
        return note_cyan();
      }

      if (status >= 400 && status < 500)
      {
        return note_yellow();
      }

      return note_red();
    }

    std::string note_log_level_for_status(int status)
    {
      if (status >= 500)
      {
        return "error";
      }

      if (status >= 400)
      {
        return "warn";
      }

      return "info";
    }

    std::string note_format_local_time_24h()
    {
      using clock = std::chrono::system_clock;

      const auto now = clock::now();
      const std::time_t raw = clock::to_time_t(now);

      std::tm tm{};

#if defined(_WIN32)
      localtime_s(&tm, &raw);
#else
      localtime_r(&raw, &tm);
#endif

      std::ostringstream out;

      out << std::setw(2) << std::setfill('0') << tm.tm_hour
          << ":"
          << std::setw(2) << std::setfill('0') << tm.tm_min
          << ":"
          << std::setw(2) << std::setfill('0') << tm.tm_sec;

      return out.str();
    }

    void note_print_request_log(
        const NoteRouteRequest &request,
        int status,
        long long elapsedMs)
    {
      const std::string method = std::string(to_string(request.method));
      const std::string path =
          request.path.empty() ? "<bad-request>" : request.path;

      std::string methodLabel = method;

      if (methodLabel.size() < 4)
      {
        methodLabel.append(4 - methodLabel.size(), ' ');
      }

      const std::string level = note_log_level_for_status(status);

      std::cerr
          << note_gray()
          << note_format_local_time_24h()
          << note_reset()
          << " "
          << note_gray()
          << "[note]"
          << note_reset()
          << " "
          << note_level_color(status)
          << "["
          << level
          << "]"
          << note_reset()
          << " "
          << note_cyan()
          << methodLabel
          << note_reset()
          << " "
          << path
          << " "
          << note_status_color(status)
          << status
          << note_reset()
          << " "
          << note_dim()
          << elapsedMs
          << "ms"
          << note_reset()
          << '\n';
    }
  }

  class NoteServerRuntime
  {
  public:
    NoteServerRuntime() = default;

    ~NoteServerRuntime()
    {
      (void)stop();
    }

    NoteServerRuntime(const NoteServerRuntime &) = delete;
    NoteServerRuntime &operator=(const NoteServerRuntime &) = delete;

    void set_routes(NoteRoutes *routes, std::mutex *routeMutex) noexcept
    {
      routes_ = routes;
      routeMutex_ = routeMutex;
    }

    bool running() const noexcept
    {
      return running_.load();
    }

    NoteResult start(
        NoteRoutes &routes,
        std::mutex &routeMutex,
        std::string_view host,
        std::uint16_t port,
        bool logRequests)
    {
      if (running())
      {
        return NoteResult::success("note server already running");
      }

      std::string err;

      if (!socket_platform_start(err))
      {
        return NoteResult::failure(err, 1).add_error(err);
      }

      NoteSocket socket =
          create_listening_socket(host, port, err);

      if (socket == invalid_socket_value)
      {
        socket_platform_stop();
        return NoteResult::failure(err, 1).add_error(err);
      }

      routes_ = &routes;
      routeMutex_ = &routeMutex;
      serverSocket_ = socket;
      running_.store(true);
      logRequests_ = logRequests;

      const std::size_t workerCount = client_worker_count();

      workers_.reserve(workerCount);

      for (std::size_t i = 0; i < workerCount; ++i)
      {
        workers_.emplace_back(
            [this]()
            {
              worker_loop();
            });
      }

      acceptThread_ =
          std::thread(
              [this]()
              {
                accept_loop();
              });

      return NoteResult::success("note server runtime started");
    }

    NoteResult wait()
    {
      if (!running() && !acceptThread_.joinable())
      {
        return NoteResult::success("note server already stopped");
      }

      join_thread(acceptThread_);
      join_workers();

      running_.store(false);
      socket_platform_stop();

      return NoteResult::success("note server stopped");
    }

    NoteResult stop()
    {
      if (!running() && !acceptThread_.joinable() && workers_.empty())
      {
        return NoteResult::success("note server already stopped");
      }

      running_.store(false);

      const NoteSocket socket = serverSocket_;

      if (socket != invalid_socket_value)
      {
        shutdown_note_socket(socket);
        close_note_socket(socket);
        serverSocket_ = invalid_socket_value;
      }

      clientQueueCv_.notify_all();
      clientQueueSpaceCv_.notify_all();

      join_thread(acceptThread_);
      join_workers();
      close_queued_clients();

      socket_platform_stop();

      return NoteResult::success("note server stopped");
    }

  private:
    static std::size_t client_worker_count()
    {
      const unsigned int hardware = std::thread::hardware_concurrency();
      const std::size_t available = hardware == 0 ? 4 : hardware;

      return std::clamp<std::size_t>(available, 2, 4);
    }

    static bool can_handle_without_route_lock(const NoteRouteRequest &request)
    {
      return request.method == NoteRouteMethod::Get &&
             !is_note_api_path(request.path);
    }

    void accept_loop()
    {
      const NoteSocket listenSocket = serverSocket_;

      while (running())
      {
        sockaddr_storage clientAddress{};
        socklen_t clientAddressLength =
            static_cast<socklen_t>(sizeof(clientAddress));

        NoteSocket client =
            accept(
                listenSocket,
                reinterpret_cast<sockaddr *>(&clientAddress),
                &clientAddressLength);

        if (client == invalid_socket_value)
        {
          if (running())
          {
            continue;
          }

          break;
        }

        configure_client_socket(client);
        enqueue_client(client);
      }

      running_.store(false);
      clientQueueCv_.notify_all();
      clientQueueSpaceCv_.notify_all();
    }

    void enqueue_client(NoteSocket client)
    {
      std::unique_lock<std::mutex> lock(clientQueueMutex_);

      clientQueueSpaceCv_.wait(
          lock,
          [this]()
          {
            return !running() || clientQueue_.size() < maxQueuedClients;
          });

      if (!running())
      {
        lock.unlock();
        close_note_socket(client);
        return;
      }

      clientQueue_.push_back(client);
      clientQueueCv_.notify_one();
    }

    void worker_loop()
    {
      while (true)
      {
        NoteSocket client = invalid_socket_value;

        {
          std::unique_lock<std::mutex> lock(clientQueueMutex_);

          clientQueueCv_.wait(
              lock,
              [this]()
              {
                return !clientQueue_.empty() || !running();
              });

          if (clientQueue_.empty())
          {
            if (!running())
            {
              break;
            }

            continue;
          }

          client = clientQueue_.front();
          clientQueue_.pop_front();
          clientQueueSpaceCv_.notify_one();
        }

        if (!running())
        {
          close_note_socket(client);
          continue;
        }

        handle_client(client);
        close_note_socket(client);
      }
    }

    void handle_client(NoteSocket client)
    {
      const auto requestStart = std::chrono::steady_clock::now();

      NoteRouteRequest request;

      NoteRouteResponse response =
          NoteRouteResponse::text(400, "bad request");

      if (read_http_request(client, request))
      {
        if (routes_ == nullptr)
        {
          response =
              NoteRouteResponse::text(500, "note routes are not available");
        }
        else if (request.method == NoteRouteMethod::Unknown)
        {
          response =
              NoteRouteResponse::text(405, "method not allowed");
        }
        else
        {
          response = handle_route_request(request);
        }
      }

      if (logRequests_)
      {
        const auto requestEnd = std::chrono::steady_clock::now();

        const long long elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                requestEnd - requestStart)
                .count();

        note_print_request_log(request, response.status, elapsedMs);
      }

      const std::string payload =
          make_http_response(response);

      (void)send_all(client, payload);
    }

    NoteRouteResponse handle_route_request(const NoteRouteRequest &request)
    {
      // Static UI assets are read-only after start(), so they can be served
      // without blocking behind a long-running API request. API routes share
      // NoteRoutes, NoteKernel, NoteDocument, and NoteStore state and must be
      // serialized until Vix Note grows a dedicated NoteJobManager for
      // non-blocking cell execution. That future job manager should be local to
      // Vix Note rather than wired directly to vix::async from this server.
      if (can_handle_without_route_lock(request))
      {
        return routes_->handle(request);
      }

      if (routeMutex_ == nullptr)
      {
        return routes_->handle(request);
      }

      std::lock_guard<std::mutex> lock(*routeMutex_);
      return routes_->handle(request);
    }

    static void join_thread(std::thread &thread)
    {
      if (thread.joinable() && thread.get_id() != std::this_thread::get_id())
      {
        thread.join();
      }
    }

    void join_workers()
    {
      for (std::thread &worker : workers_)
      {
        join_thread(worker);
      }

      workers_.clear();
    }

    void close_queued_clients()
    {
      std::lock_guard<std::mutex> lock(clientQueueMutex_);

      while (!clientQueue_.empty())
      {
        close_note_socket(clientQueue_.front());
        clientQueue_.pop_front();
      }
    }

    static constexpr std::size_t maxQueuedClients = 32;

    std::atomic<bool> running_{false};
    NoteSocket serverSocket_{invalid_socket_value};
    NoteRoutes *routes_{nullptr};
    std::mutex *routeMutex_{nullptr};
    bool logRequests_ = false;
    std::thread acceptThread_;
    std::vector<std::thread> workers_;
    std::mutex clientQueueMutex_;
    std::condition_variable clientQueueCv_;
    std::condition_variable clientQueueSpaceCv_;
    std::deque<NoteSocket> clientQueue_;
  };

  NoteServer::NoteServer()
      : routes_(options_.routeOptions),
        runtime_(std::make_unique<NoteServerRuntime>())
  {
  }

  NoteServer::NoteServer(NoteDocument document)
      : routes_(std::move(document), options_.routeOptions),
        runtime_(std::make_unique<NoteServerRuntime>())
  {
  }

  NoteServer::NoteServer(NoteServerOptions options)
      : options_(std::move(options)),
        routes_(options_.routeOptions),
        runtime_(std::make_unique<NoteServerRuntime>())
  {
  }

  NoteServer::NoteServer(NoteDocument document, NoteServerOptions options)
      : options_(std::move(options)),
        routes_(std::move(document), options_.routeOptions),
        runtime_(std::make_unique<NoteServerRuntime>())
  {
  }

  NoteServer::~NoteServer()
  {
    (void)stop();
  }

  NoteServer::NoteServer(NoteServer &&other) noexcept
      : options_(std::move(other.options_)),
        routes_(std::move(other.routes_)),
        routeMutex_(std::move(other.routeMutex_)),
        state_(other.state_),
        runtime_(std::move(other.runtime_))
  {
    other.state_ = NoteServerState::Stopped;

    if (!other.routeMutex_)
    {
      other.routeMutex_ = std::make_shared<std::mutex>();
    }

    if (runtime_)
    {
      runtime_->set_routes(&routes_, routeMutex_.get());
    }
  }

  NoteServer &NoteServer::operator=(NoteServer &&other) noexcept
  {
    if (this == &other)
    {
      return *this;
    }

    (void)stop();

    options_ = std::move(other.options_);
    routes_ = std::move(other.routes_);
    routeMutex_ = std::move(other.routeMutex_);
    state_ = other.state_;
    runtime_ = std::move(other.runtime_);

    other.state_ = NoteServerState::Stopped;

    if (!other.routeMutex_)
    {
      other.routeMutex_ = std::make_shared<std::mutex>();
    }

    if (runtime_)
    {
      runtime_->set_routes(&routes_, routeMutex_.get());
    }

    return *this;
  }

  const NoteServerOptions &NoteServer::options() const noexcept
  {
    return options_;
  }

  NoteResult NoteServer::set_options(NoteServerOptions options)
  {
    if (running())
    {
      return NoteResult::failure("cannot change server options while running", 1)
          .add_error("cannot change server options while running");
    }

    std::lock_guard<std::mutex> lock(*routeMutex_);

    options_ = std::move(options);
    routes_.set_options(options_.routeOptions);

    return NoteResult::success("server options updated");
  }

  NoteServerState NoteServer::state() const noexcept
  {
    return state_;
  }

  bool NoteServer::running() const noexcept
  {
    return state_ == NoteServerState::Running;
  }

  bool NoteServer::stopped() const noexcept
  {
    return state_ == NoteServerState::Stopped;
  }

  NoteResult NoteServer::start()
  {
    if (running())
    {
      return NoteResult::success("note server already running")
          .add_text(url());
    }

    if (options_.host.empty())
    {
      return NoteResult::failure("note server host is empty", 1)
          .add_error("note server host is empty");
    }

    if (options_.port == 0)
    {
      return NoteResult::failure("note server port is invalid", 1)
          .add_error("note server port is invalid");
    }

    if (!runtime_)
    {
      runtime_ = std::make_unique<NoteServerRuntime>();
    }

    NoteResult runtimeResult =
        runtime_->start(
            routes_,
            *routeMutex_,
            options_.host,
            options_.port,
            options_.logRequests);

    if (!runtimeResult.ok())
    {
      state_ = NoteServerState::Stopped;
      return runtimeResult;
    }

    state_ = NoteServerState::Running;

    return NoteResult::success("note server started")
        .add_text(url());
  }

  NoteResult NoteServer::wait()
  {
    if (stopped())
    {
      return NoteResult::success("note server already stopped");
    }

    if (!runtime_)
    {
      state_ = NoteServerState::Stopped;

      return NoteResult::failure("note server runtime is not available", 1)
          .add_error("note server runtime is not available");
    }

    NoteResult result = runtime_->wait();

    state_ = NoteServerState::Stopped;

    return result;
  }

  NoteResult NoteServer::stop()
  {
    if (stopped())
    {
      return NoteResult::success("note server already stopped");
    }

    NoteResult result =
        runtime_ ? runtime_->stop()
                 : NoteResult::success("note server stopped");

    state_ = NoteServerState::Stopped;

    if (!result.ok())
    {
      return result;
    }

    return NoteResult::success("note server stopped");
  }

  NoteResult NoteServer::restart()
  {
    if (running())
    {
      NoteResult stoppedResult = stop();

      if (!stoppedResult.ok())
      {
        return stoppedResult;
      }
    }

    return start();
  }

  std::string NoteServer::url() const
  {
    return make_note_server_url(options_.host, options_.port);
  }

  const NoteRoutes &NoteServer::routes() const noexcept
  {
    return routes_;
  }

  NoteRoutes &NoteServer::routes() noexcept
  {
    return routes_;
  }

  const NoteDocument &NoteServer::document() const noexcept
  {
    return routes_.document();
  }

  void NoteServer::set_document(NoteDocument document)
  {
    std::lock_guard<std::mutex> lock(*routeMutex_);
    routes_.set_document(std::move(document));
  }

  NoteRouteResponse NoteServer::handle(const NoteRouteRequest &request)
  {
    std::lock_guard<std::mutex> lock(*routeMutex_);
    return routes_.handle(request);
  }

  NoteRouteResponse NoteServer::get(std::string_view path)
  {
    std::lock_guard<std::mutex> lock(*routeMutex_);
    return routes_.get(path);
  }

  NoteRouteResponse NoteServer::post(std::string_view path, std::string body)
  {
    std::lock_guard<std::mutex> lock(*routeMutex_);
    return routes_.post(path, std::move(body));
  }

  NoteRouteResponse NoteServer::put(std::string_view path, std::string body)
  {
    std::lock_guard<std::mutex> lock(*routeMutex_);
    return routes_.put(path, std::move(body));
  }

  NoteRouteResponse NoteServer::delete_request(std::string_view path)
  {
    std::lock_guard<std::mutex> lock(*routeMutex_);
    return routes_.delete_request(path);
  }

  std::string_view to_string(NoteServerState state) noexcept
  {
    switch (state)
    {
    case NoteServerState::Stopped:
      return "stopped";

    case NoteServerState::Running:
      return "running";
    }

    return "stopped";
  }

  std::string make_note_server_url(
      std::string_view host,
      std::uint16_t port)
  {
    std::ostringstream out;

    out << "http://"
        << host
        << ":"
        << port
        << "/";

    return out.str();
  }
}
