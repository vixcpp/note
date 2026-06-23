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

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace vix::note
{
  NoteServer::NoteServer()
      : routes_(options_.routeOptions)
  {
  }

  NoteServer::NoteServer(NoteDocument document)
      : routes_(std::move(document), options_.routeOptions)
  {
  }

  NoteServer::NoteServer(NoteServerOptions options)
      : options_(std::move(options)),
        routes_(options_.routeOptions)
  {
  }

  NoteServer::NoteServer(NoteDocument document, NoteServerOptions options)
      : options_(std::move(options)),
        routes_(std::move(document), options_.routeOptions)
  {
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

    state_ = NoteServerState::Running;

    return NoteResult::success("note server started")
        .add_text(url());
  }

  NoteResult NoteServer::stop()
  {
    if (stopped())
    {
      return NoteResult::success("note server already stopped");
    }

    state_ = NoteServerState::Stopped;

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
    routes_.set_document(std::move(document));
  }

  NoteRouteResponse NoteServer::handle(const NoteRouteRequest &request)
  {
    return routes_.handle(request);
  }

  NoteRouteResponse NoteServer::get(std::string_view path)
  {
    return routes_.get(path);
  }

  NoteRouteResponse NoteServer::post(std::string_view path, std::string body)
  {
    return routes_.post(path, std::move(body));
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
