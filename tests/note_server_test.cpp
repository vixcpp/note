/**
 *
 *  @file note_server_test.cpp
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

#include <vix/note/core/NoteCell.hpp>
#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/web/NoteServer.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
#ifdef _WIN32
  using TestSocket = SOCKET;

  constexpr TestSocket invalid_test_socket = INVALID_SOCKET;

  void close_test_socket(TestSocket socket)
  {
    if (socket != invalid_test_socket)
    {
      closesocket(socket);
    }
  }
#else
  using TestSocket = int;

  constexpr TestSocket invalid_test_socket = -1;

  void close_test_socket(TestSocket socket)
  {
    if (socket != invalid_test_socket)
    {
      close(socket);
    }
  }
#endif

  bool contains(const std::string &text, std::string_view needle)
  {
    return text.find(std::string(needle)) != std::string::npos;
  }

  std::uint16_t next_test_port()
  {
    static std::uint16_t port = 55179;
    return port++;
  }

  vix::note::NoteServerOptions make_server_options()
  {
    vix::note::NoteServerOptions options;
    options.host = "127.0.0.1";
    options.port = next_test_port();
    return options;
  }

  bool send_all(TestSocket socket, const std::string &payload)
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

  std::string read_all(TestSocket socket)
  {
    std::string response;
    char buffer[4096];

    while (true)
    {
#ifdef _WIN32
      const int n =
          recv(
              socket,
              buffer,
              static_cast<int>(sizeof(buffer)),
              0);
#else
      const ssize_t n =
          recv(
              socket,
              buffer,
              sizeof(buffer),
              0);
#endif

      if (n <= 0)
      {
        break;
      }

      response.append(buffer, static_cast<std::size_t>(n));
    }

    return response;
  }

  std::string http_request(
      std::string_view host,
      std::uint16_t port,
      const std::string &request)
  {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *addresses = nullptr;

    const std::string hostString(host);
    const std::string service = std::to_string(port);

    const int rc =
        getaddrinfo(
            hostString.c_str(),
            service.c_str(),
            &hints,
            &addresses);

    assert(rc == 0);
    assert(addresses != nullptr);

    TestSocket socket = invalid_test_socket;

    for (addrinfo *address = addresses;
         address != nullptr;
         address = address->ai_next)
    {
      TestSocket candidate =
          ::socket(
              address->ai_family,
              address->ai_socktype,
              address->ai_protocol);

      if (candidate == invalid_test_socket)
      {
        continue;
      }

      if (connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
      {
        socket = candidate;
        break;
      }

      close_test_socket(candidate);
    }

    freeaddrinfo(addresses);

    assert(socket != invalid_test_socket);
    assert(send_all(socket, request));

    const std::string response = read_all(socket);

    close_test_socket(socket);

    return response;
  }

  std::string http_get(
      std::string_view host,
      std::uint16_t port,
      std::string_view path)
  {
    std::ostringstream request;

    request << "GET "
            << path
            << " HTTP/1.1\r\n";

    request << "Host: "
            << host
            << "\r\n";

    request << "Connection: close\r\n";
    request << "\r\n";

    return http_request(host, port, request.str());
  }

  std::string http_post(
      std::string_view host,
      std::uint16_t port,
      std::string_view path,
      const std::string &body = {})
  {
    std::ostringstream request;

    request << "POST "
            << path
            << " HTTP/1.1\r\n";

    request << "Host: "
            << host
            << "\r\n";

    request << "Content-Type: application/json\r\n";
    request << "Content-Length: "
            << body.size()
            << "\r\n";

    request << "Connection: close\r\n";
    request << "\r\n";
    request << body;

    return http_request(host, port, request.str());
  }

  std::string http_put(
      std::string_view host,
      std::uint16_t port,
      std::string_view path,
      const std::string &body = {})
  {
    std::ostringstream request;

    request << "PUT "
            << path
            << " HTTP/1.1\r\n";

    request << "Host: "
            << host
            << "\r\n";

    request << "Content-Type: application/json\r\n";
    request << "Content-Length: "
            << body.size()
            << "\r\n";

    request << "Connection: close\r\n";
    request << "\r\n";
    request << body;

    return http_request(host, port, request.str());
  }

  std::string http_delete(
      std::string_view host,
      std::uint16_t port,
      std::string_view path)
  {
    std::ostringstream request;

    request << "DELETE "
            << path
            << " HTTP/1.1\r\n";

    request << "Host: "
            << host
            << "\r\n";

    request << "Connection: close\r\n";
    request << "\r\n";

    return http_request(host, port, request.str());
  }
}

int main()
{
  {
    assert(vix::note::to_string(vix::note::NoteServerState::Stopped) == "stopped");
    assert(vix::note::to_string(vix::note::NoteServerState::Running) == "running");
  }

  {
    const std::string url =
        vix::note::make_note_server_url("127.0.0.1", 5179);

    assert(url == "http://127.0.0.1:5179/");
  }

  {
    vix::note::NoteServer server;

    assert(server.state() == vix::note::NoteServerState::Stopped);
    assert(server.stopped());
    assert(!server.running());

    assert(server.options().host == "127.0.0.1");
    assert(server.options().port == 5179);
    assert(!server.options().openBrowser);
    assert(server.options().routeOptions.enableApi);
    assert(server.options().routeOptions.enableAssets);
    assert(server.options().routeOptions.enableEditing);
    assert(server.options().routeOptions.enableSave);

    assert(server.url() == "http://127.0.0.1:5179/");
    assert(server.document().empty());
  }

  {
    vix::note::NoteDocument doc("Server Doc");
    doc.add_markdown("# Server Doc");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteServer server(doc);

    assert(server.document().title() == "Server Doc");
    assert(server.document().cell_count() == 2);
    assert(server.routes().document().title() == "Server Doc");
  }

  {
    vix::note::NoteServerOptions options;
    options.host = "localhost";
    options.port = 8080;
    options.openBrowser = true;
    options.routeOptions.enableApi = false;
    options.routeOptions.enableAssets = true;

    vix::note::NoteServer server(options);

    assert(server.options().host == "localhost");
    assert(server.options().port == 8080);
    assert(server.options().openBrowser);
    assert(!server.options().routeOptions.enableApi);
    assert(server.options().routeOptions.enableAssets);

    assert(server.url() == "http://localhost:8080/");
    assert(!server.routes().options().enableApi);
    assert(server.routes().options().enableAssets);
  }

  {
    vix::note::NoteDocument doc("Custom Server Doc");
    doc.add_cpp("int main() { return 0; }");

    vix::note::NoteServerOptions options;
    options.host = "0.0.0.0";
    options.port = 9000;
    options.routeOptions.enableApi = true;
    options.routeOptions.enableAssets = false;

    vix::note::NoteServer server(doc, options);

    assert(server.document().title() == "Custom Server Doc");
    assert(server.document().cell_count() == 1);
    assert(server.url() == "http://0.0.0.0:9000/");
    assert(server.routes().options().enableApi);
    assert(!server.routes().options().enableAssets);
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteServerOptions options;
    options.host = "localhost";
    options.port = 3000;
    options.routeOptions.enableApi = false;
    options.routeOptions.enableAssets = false;
    options.routeOptions.enableEditing = false;
    options.routeOptions.enableSave = false;

    vix::note::NoteResult result =
        server.set_options(options);

    assert(result.ok());
    assert(result.message() == "server options updated");
    assert(server.options().host == "localhost");
    assert(server.options().port == 3000);
    assert(!server.routes().options().enableApi);
    assert(!server.routes().options().enableAssets);
    assert(!server.routes().options().enableEditing);
    assert(!server.routes().options().enableSave);
    assert(server.url() == "http://localhost:3000/");
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteServer server(options);

    vix::note::NoteResult started =
        server.start();

    assert(started.ok());
    assert(started.message() == "note server started");
    assert(started.has_outputs());
    assert(started.outputs()[0].kind == vix::note::NoteOutputKind::Text);
    assert(started.outputs()[0].content == server.url());

    assert(server.running());
    assert(!server.stopped());
    assert(server.state() == vix::note::NoteServerState::Running);

    vix::note::NoteServerOptions changedOptions = make_server_options();

    vix::note::NoteResult changed =
        server.set_options(changedOptions);

    assert(changed.failed());
    assert(changed.message() == "cannot change server options while running");
    assert(changed.has_outputs());
    assert(changed.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteServer server(options);

    assert(server.start().ok());

    vix::note::NoteResult startedAgain =
        server.start();

    assert(startedAgain.ok());
    assert(startedAgain.message() == "note server already running");
    assert(startedAgain.has_outputs());
    assert(startedAgain.outputs()[0].content == server.url());
    assert(server.running());
  }

  {
    vix::note::NoteServerOptions options;
    options.host.clear();
    options.port = next_test_port();

    vix::note::NoteServer server(options);

    vix::note::NoteResult result =
        server.start();

    assert(result.failed());
    assert(result.message() == "note server host is empty");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
    assert(server.stopped());
  }

  {
    vix::note::NoteServerOptions options;
    options.port = 0;

    vix::note::NoteServer server(options);

    vix::note::NoteResult result =
        server.start();

    assert(result.failed());
    assert(result.message() == "note server port is invalid");
    assert(result.has_outputs());
    assert(result.outputs()[0].kind == vix::note::NoteOutputKind::Error);
    assert(server.stopped());
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteServer server(options);

    assert(server.start().ok());
    assert(server.running());

    vix::note::NoteResult stopped =
        server.stop();

    assert(stopped.ok());
    assert(stopped.message() == "note server stopped");
    assert(server.stopped());
    assert(!server.running());
  }

  {
    vix::note::NoteServer server;

    assert(server.stopped());

    vix::note::NoteResult stoppedAgain =
        server.stop();

    assert(stoppedAgain.ok());
    assert(stoppedAgain.message() == "note server already stopped");
    assert(server.stopped());
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteResult waitResult =
        server.wait();

    assert(waitResult.ok());
    assert(waitResult.message() == "note server already stopped");
    assert(server.stopped());
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteServer server(options);

    vix::note::NoteResult restarted =
        server.restart();

    assert(restarted.ok());
    assert(restarted.message() == "note server started");
    assert(server.running());

    vix::note::NoteResult restartedAgain =
        server.restart();

    assert(restartedAgain.ok());
    assert(restartedAgain.message() == "note server started");
    assert(server.running());
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteDocument doc("Replaced");
    doc.add_markdown("# Replaced");
    doc.add_cpp("int main() { return 0; }");

    server.set_document(doc);

    assert(server.document().title() == "Replaced");
    assert(server.document().cell_count() == 2);
    assert(server.routes().document().title() == "Replaced");
    assert(server.routes().kernel().can_execute_cell(1));
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteRouteResponse response =
        server.get("/");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "text/html; charset=utf-8");
    assert(contains(response.body, "Vix Note"));
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteRouteResponse css =
        server.get("/assets/note.css");

    assert(css.ok());
    assert(css.contentType == "text/css; charset=utf-8");
    assert(contains(css.body, ".note-shell"));

    vix::note::NoteRouteResponse js =
        server.get("/assets/note.js");

    assert(js.ok());
    assert(js.contentType == "application/javascript; charset=utf-8");
    assert(contains(js.body, "run-cell"));
  }

  {
    vix::note::NoteServerOptions options;
    options.routeOptions.enableAssets = false;

    vix::note::NoteServer server(options);

    vix::note::NoteRouteResponse response =
        server.get("/");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.body == "not found");
  }

  {
    vix::note::NoteDocument doc("API Server Doc");
    doc.set_path("examples/server.vixnote");

    doc.add_cell(
        vix::note::NoteCell(
            "intro",
            vix::note::NoteCellKind::Markdown,
            "# API Server Doc"));

    doc.add_cell(
        vix::note::NoteCell(
            "reply",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteServer server(doc);

    vix::note::NoteRouteResponse response =
        server.get("/api/document");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"title\":\"API Server Doc\""));
    assert(contains(response.body, "\"path\":\"examples/server.vixnote\""));
    assert(contains(response.body, "\"cellCount\":2"));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"index\":1"));
    assert(contains(response.body, "\"id\":\"intro\""));
    assert(contains(response.body, "\"id\":\"reply\""));
    assert(contains(response.body, "\"executable\":true"));
    assert(contains(response.body, "\"outputs\":[]"));
  }

  {
    vix::note::NoteServerOptions options;
    options.routeOptions.enableApi = false;

    vix::note::NoteServer server(options);

    vix::note::NoteRouteResponse response =
        server.get("/api/document");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.body == "not found");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "reply",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteServer server(doc);

    vix::note::NoteRouteResponse response =
        server.post("/api/cells/0/run");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "\"result\":{"));
    assert(contains(response.body, "\"cell\":{"));
    assert(contains(response.body, "\"status\":\"skipped\""));
    assert(contains(response.body, "\"message\":\"Reply cell execution is not available yet\""));
    assert(contains(response.body, "\"executionCount\":1"));

    assert(server.document().cells()[0].execution_count() == 1);
    assert(server.routes().kernel().session().has_records());
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Intro");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteServer server(doc);

    vix::note::NoteRouteResponse response =
        server.post("/api/run-all");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"visited\":2"));
    assert(contains(response.body, "\"executed\":1"));
    assert(contains(response.body, "\"skipped\":1"));
    assert(contains(response.body, "\"failed\":0"));
    assert(contains(response.body, "\"status\":\"skipped\""));
    assert(contains(response.body, "\"document\":{"));
    assert(contains(response.body, "\"cellCount\":2"));

    assert(server.document().cells()[0].execution_count() == 0);
    assert(server.document().cells()[1].execution_count() == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "edit-server",
            vix::note::NoteCellKind::Markdown,
            "Old server source"));

    vix::note::NoteServer server(doc);

    vix::note::NoteRouteResponse response =
        server.put(
            "/api/cells/edit-server",
            "{\"kind\":\"cpp\",\"source\":\"int main() { return 0; }\"}");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell updated\""));
    assert(contains(response.body, "\"cellId\":\"edit-server\""));

    assert(server.document().cells()[0].kind() == vix::note::NoteCellKind::Cpp);
    assert(server.document().cells()[0].source() == "int main() { return 0; }");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "delete-server",
            vix::note::NoteCellKind::Markdown,
            "Delete server"));

    doc.add_cell(
        vix::note::NoteCell(
            "keep-server",
            vix::note::NoteCellKind::Markdown,
            "Keep server"));

    vix::note::NoteServer server(doc);

    vix::note::NoteRouteResponse response =
        server.delete_request("/api/cells/delete-server");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell deleted\""));
    assert(contains(response.body, "\"cellId\":\"delete-server\""));

    assert(server.document().cell_count() == 1);
    assert(server.document().cells()[0].id() == "keep-server");
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteRouteRequest request;
    request.method = vix::note::NoteRouteMethod::Get;
    request.path = "/";

    vix::note::NoteRouteResponse response =
        server.handle(request);

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "Vix Note"));
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteRouteResponse response =
        server.get("/missing");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.body == "not found");
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteDocument doc("Network Server Doc");
    doc.set_path("examples/network.vixnote");

    doc.add_cell(
        vix::note::NoteCell(
            "intro",
            vix::note::NoteCellKind::Markdown,
            "# Network Server Doc"));

    doc.add_cell(
        vix::note::NoteCell(
            "reply",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteServer server(doc, options);

    vix::note::NoteResult started =
        server.start();

    assert(started.ok());
    assert(server.running());

    const std::string indexResponse =
        http_get(options.host, options.port, "/");

    assert(contains(indexResponse, "HTTP/1.1 200 OK"));
    assert(contains(indexResponse, "Content-Type: text/html; charset=utf-8"));
    assert(contains(indexResponse, "Vix Note"));

    const std::string cssResponse =
        http_get(options.host, options.port, "/assets/note.css");

    assert(contains(cssResponse, "HTTP/1.1 200 OK"));
    assert(contains(cssResponse, "Content-Type: text/css; charset=utf-8"));
    assert(contains(cssResponse, ".note-shell"));

    const std::string documentResponse =
        http_get(options.host, options.port, "/api/document");

    assert(contains(documentResponse, "HTTP/1.1 200 OK"));
    assert(contains(documentResponse, "Content-Type: application/json; charset=utf-8"));
    assert(contains(documentResponse, "\"ok\":true"));
    assert(contains(documentResponse, "\"title\":\"Network Server Doc\""));
    assert(contains(documentResponse, "\"path\":\"examples/network.vixnote\""));
    assert(contains(documentResponse, "\"cellCount\":2"));
    assert(contains(documentResponse, "\"id\":\"intro\""));
    assert(contains(documentResponse, "\"id\":\"reply\""));
    assert(contains(documentResponse, "\"outputs\":[]"));

    vix::note::NoteResult stopped =
        server.stop();

    assert(stopped.ok());
    assert(server.stopped());
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteDocument doc("Network Run All");

    doc.add_markdown("# Network Run All");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteServer server(doc, options);

    assert(server.start().ok());

    const std::string runAllResponse =
        http_post(options.host, options.port, "/api/run-all");

    assert(contains(runAllResponse, "HTTP/1.1 200 OK"));
    assert(contains(runAllResponse, "Content-Type: application/json; charset=utf-8"));
    assert(contains(runAllResponse, "\"ok\":true"));
    assert(contains(runAllResponse, "\"visited\":2"));
    assert(contains(runAllResponse, "\"executed\":1"));
    assert(contains(runAllResponse, "\"skipped\":1"));
    assert(contains(runAllResponse, "\"failed\":0"));
    assert(contains(runAllResponse, "\"status\":\"skipped\""));
    assert(contains(runAllResponse, "\"document\":{"));
    assert(contains(runAllResponse, "\"cellCount\":2"));
    assert(contains(runAllResponse, "\"executionCount\":1"));

    assert(server.document().cells()[0].execution_count() == 0);
    assert(server.document().cells()[1].execution_count() == 1);

    assert(server.stop().ok());
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteDocument doc("Network Edit");

    doc.add_cell(
        vix::note::NoteCell(
            "network-edit",
            vix::note::NoteCellKind::Markdown,
            "Old network source"));

    vix::note::NoteServer server(doc, options);

    assert(server.start().ok());

    const std::string putResponse =
        http_put(
            options.host,
            options.port,
            "/api/cells/network-edit",
            "{\"kind\":\"cpp\",\"source\":\"int main() { return 0; }\"}");

    assert(contains(putResponse, "HTTP/1.1 200 OK"));
    assert(contains(putResponse, "\"ok\":true"));
    assert(contains(putResponse, "\"message\":\"cell updated\""));
    assert(contains(putResponse, "\"cellId\":\"network-edit\""));

    assert(server.document().cells()[0].kind() == vix::note::NoteCellKind::Cpp);

    assert(server.stop().ok());
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteDocument doc("Network Delete");

    doc.add_cell(
        vix::note::NoteCell(
            "network-delete",
            vix::note::NoteCellKind::Markdown,
            "Delete"));

    doc.add_cell(
        vix::note::NoteCell(
            "network-keep",
            vix::note::NoteCellKind::Markdown,
            "Keep"));

    vix::note::NoteServer server(doc, options);

    assert(server.start().ok());

    const std::string deleteResponse =
        http_delete(
            options.host,
            options.port,
            "/api/cells/network-delete");

    assert(contains(deleteResponse, "HTTP/1.1 200 OK"));
    assert(contains(deleteResponse, "\"ok\":true"));
    assert(contains(deleteResponse, "\"message\":\"cell deleted\""));
    assert(contains(deleteResponse, "\"cellId\":\"network-delete\""));

    assert(server.document().cell_count() == 1);
    assert(server.document().cells()[0].id() == "network-keep");

    assert(server.stop().ok());
  }

  {
    vix::note::NoteServerOptions options = make_server_options();

    vix::note::NoteServer server(options);

    assert(server.start().ok());

    const std::string missingResponse =
        http_get(options.host, options.port, "/missing");

    assert(contains(missingResponse, "HTTP/1.1 404 Not Found"));
    assert(contains(missingResponse, "not found"));

    assert(server.stop().ok());
  }

  return 0;
}
