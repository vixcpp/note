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
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
  bool contains(const std::string &text, std::string_view needle)
  {
    return text.find(std::string(needle)) != std::string::npos;
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

    vix::note::NoteResult result =
        server.set_options(options);

    assert(result.ok());
    assert(result.message() == "server options updated");
    assert(server.options().host == "localhost");
    assert(server.options().port == 3000);
    assert(!server.routes().options().enableApi);
    assert(!server.routes().options().enableAssets);
    assert(server.url() == "http://localhost:3000/");
  }

  {
    vix::note::NoteServer server;

    vix::note::NoteResult started =
        server.start();

    assert(started.ok());
    assert(started.message() == "note server started");
    assert(started.has_outputs());
    assert(started.outputs()[0].kind == vix::note::NoteOutputKind::Text);
    assert(started.outputs()[0].content == "http://127.0.0.1:5179/");

    assert(server.running());
    assert(!server.stopped());
    assert(server.state() == vix::note::NoteServerState::Running);

    vix::note::NoteServerOptions options;
    options.host = "localhost";
    options.port = 9001;

    vix::note::NoteResult changed =
        server.set_options(options);

    assert(changed.failed());
    assert(changed.message() == "cannot change server options while running");
    assert(changed.has_outputs());
    assert(changed.outputs()[0].kind == vix::note::NoteOutputKind::Error);
  }

  {
    vix::note::NoteServer server;

    assert(server.start().ok());

    vix::note::NoteResult startedAgain =
        server.start();

    assert(startedAgain.ok());
    assert(startedAgain.message() == "note server already running");
    assert(startedAgain.has_outputs());
    assert(startedAgain.outputs()[0].content == "http://127.0.0.1:5179/");
    assert(server.running());
  }

  {
    vix::note::NoteServerOptions options;
    options.host.clear();

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
    vix::note::NoteServer server;

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
    assert(contains(response.body, "\"id\":\"intro\""));
    assert(contains(response.body, "\"id\":\"reply\""));
    assert(contains(response.body, "\"executable\":true"));
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

    assert(contains(response.body, "\"status\":\"skipped\""));
    assert(contains(response.body, "\"message\":\"Reply cell execution is not available yet\""));

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
    assert(contains(response.body, "\"status\":\"skipped\""));

    assert(server.document().cells()[0].execution_count() == 0);
    assert(server.document().cells()[1].execution_count() == 1);
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

  return 0;
}
