/**
 *
 *  @file note_routes_test.cpp
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
#include <vix/note/runtime/NoteKernel.hpp>
#include <vix/note/extensions/NoteExtensionRegistry.hpp>
#include <vix/note/web/NoteRoutes.hpp>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace
{
  std::filesystem::path make_test_root()
  {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("vix-note-routes-test-" + std::to_string(now));
  }

  void write_file(const std::filesystem::path &path, const std::string &content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
  }

  std::string read_file(const std::filesystem::path &path)
  {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }

  std::filesystem::path make_fake_vix_command(const std::filesystem::path &root)
  {
#ifdef _WIN32
    const std::filesystem::path command = root / "fake-vix.bat";

    write_file(
        command,
        "@echo off\n"
        "if \"%3\"==\"--fail\" (\n"
        "  echo simulated routes failure\n"
        "  exit /b 7\n"
        ")\n"
        "echo fake vix run\n"
        "echo mode:%1\n"
        "type \"%2\"\n"
        "exit /b 0\n");

    return command;
#else
    const std::filesystem::path command = root / "fake-vix";

    write_file(
        command,
        "#!/bin/sh\n"
        "if [ \"$3\" = \"--fail\" ]; then\n"
        "  echo simulated routes failure\n"
        "  exit 7\n"
        "fi\n"
        "echo fake vix run\n"
        "echo mode:$1\n"
        "cat \"$2\"\n"
        "exit 0\n");

    chmod(command.string().c_str(), 0755);

    return command;
#endif
  }

  vix::note::NoteKernelOptions make_kernel_options(
      const std::filesystem::path &fakeVix,
      const std::filesystem::path &tempDir)
  {
    vix::note::NoteKernelOptions options;
    options.cppOptions.vixCommand = fakeVix.string();
    options.cppOptions.temporaryDirectory = tempDir;
    return options;
  }

  bool contains(const std::string &text, std::string_view needle)
  {
    return text.find(std::string(needle)) != std::string::npos;
  }
}

int main()
{
  const std::filesystem::path root = make_test_root();

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  const std::filesystem::path fakeVix =
      make_fake_vix_command(root);

  {
    assert(vix::note::to_string(vix::note::NoteRouteMethod::Unknown) == "unknown");
    assert(vix::note::to_string(vix::note::NoteRouteMethod::Get) == "GET");
    assert(vix::note::to_string(vix::note::NoteRouteMethod::Post) == "POST");
    assert(vix::note::to_string(vix::note::NoteRouteMethod::Put) == "PUT");
    assert(vix::note::to_string(vix::note::NoteRouteMethod::Delete) == "DELETE");

    assert(vix::note::note_route_method_from_string("GET") == vix::note::NoteRouteMethod::Get);
    assert(vix::note::note_route_method_from_string("post") == vix::note::NoteRouteMethod::Post);
    assert(vix::note::note_route_method_from_string("Put") == vix::note::NoteRouteMethod::Put);
    assert(vix::note::note_route_method_from_string("DELETE") == vix::note::NoteRouteMethod::Delete);
    assert(vix::note::note_route_method_from_string("PATCH") == vix::note::NoteRouteMethod::Unknown);

    assert(vix::note::is_note_api_path("/api/document"));
    assert(vix::note::is_note_api_path("/api/run-all"));
    assert(!vix::note::is_note_api_path("/assets/note.css"));
    assert(!vix::note::is_note_api_path("/"));
  }

  {
    vix::note::NoteRouteResponse response =
        vix::note::NoteRouteResponse::text(201, "created");

    assert(response.status == 201);
    assert(response.ok());
    assert(response.contentType == "text/plain; charset=utf-8");
    assert(response.body == "created");
  }

  {
    vix::note::NoteRouteResponse response =
        vix::note::NoteRouteResponse::json(400, "{\"ok\":false}");

    assert(response.status == 400);
    assert(!response.ok());
    assert(response.contentType == "application/json; charset=utf-8");
    assert(response.body == "{\"ok\":false}");
  }

  {
    vix::note::NoteAsset asset{
        "/custom.txt",
        "text/plain; charset=utf-8",
        "hello"};

    vix::note::NoteRouteResponse response =
        vix::note::NoteRouteResponse::asset(asset);

    assert(response.status == 200);
    assert(response.ok());
    assert(response.contentType == "text/plain; charset=utf-8");
    assert(response.body == "hello");
  }

  {
    vix::note::NoteRoutes routes;

    assert(routes.options().enableApi);
    assert(routes.options().enableAssets);
    assert(routes.options().enableEditing);
    assert(routes.options().enableSave);
    assert(routes.assets().contains("/"));
    assert(routes.document().empty());
    assert(routes.kernel().cell_count() == 0);
    assert(routes.store().options().atomicWrite);
  }

  {
    vix::note::NoteDocument doc("Routes Doc");
    doc.add_markdown("# Routes Doc");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteRoutes routes(doc);

    assert(routes.document().title() == "Routes Doc");
    assert(routes.document().cell_count() == 2);
  }

  {
    vix::note::NoteRoutesOptions options;
    options.enableApi = false;
    options.enableAssets = false;
    options.enableEditing = false;
    options.enableSave = false;

    vix::note::NoteRoutes routes(options);

    assert(!routes.options().enableApi);
    assert(!routes.options().enableAssets);
    assert(!routes.options().enableEditing);
    assert(!routes.options().enableSave);

    options.enableApi = true;
    options.enableEditing = true;
    options.enableSave = true;

    routes.set_options(options);

    assert(routes.options().enableApi);
    assert(!routes.options().enableAssets);
    assert(routes.options().enableEditing);
    assert(routes.options().enableSave);
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.get("/");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "text/html; charset=utf-8");
    assert(contains(response.body, "Vix Note"));
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.get("/index.html");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "text/html; charset=utf-8");
    assert(contains(response.body, "<!doctype html>"));
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse css =
        routes.get("/assets/note.css");

    assert(css.ok());
    assert(css.contentType == "text/css; charset=utf-8");
    assert(contains(css.body, "color-scheme: light"));

    vix::note::NoteRouteResponse js =
        routes.get("/assets/note.js");

    assert(js.ok());
    assert(js.contentType == "application/javascript; charset=utf-8");
    assert(contains(js.body, "run-cell"));
  }

  {
    const std::filesystem::path assetDir =
        root / "routes-custom-ui";

    write_file(
        assetDir / "index.html",
        "<!doctype html><html><body>Routes Custom UI</body></html>");

    write_file(
        assetDir / "assets" / "note.css",
        ".routes-custom-ui { color: red; }");

    write_file(
        assetDir / "assets" / "note.js",
        "console.log('routes custom ui');");

    vix::note::NoteRoutesOptions options;
    options.assetDirectory = assetDir;
    options.loadInstalledAssets = false;
    options.keepEmbeddedAssetFallback = true;

    vix::note::NoteRoutes routes(options);

    vix::note::NoteRouteResponse index =
        routes.get("/");

    assert(index.ok());
    assert(index.status == 200);
    assert(index.contentType == "text/html; charset=utf-8");
    assert(contains(index.body, "Routes Custom UI"));

    vix::note::NoteRouteResponse css =
        routes.get("/assets/note.css");

    assert(css.ok());
    assert(css.status == 200);
    assert(css.contentType == "text/css; charset=utf-8");
    assert(contains(css.body, ".routes-custom-ui"));

    vix::note::NoteRouteResponse js =
        routes.get("/assets/note.js");

    assert(js.ok());
    assert(js.status == 200);
    assert(js.contentType == "application/javascript; charset=utf-8");
    assert(contains(js.body, "routes custom ui"));
  }

  {
    vix::note::NoteRoutes routes;

    routes.assets().add_or_replace(
        vix::note::NoteAsset{
            "/custom.json",
            "",
            "{\"hello\":true}"});

    vix::note::NoteRouteResponse response =
        routes.get("/custom.json");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");
    assert(response.body == "{\"hello\":true}");
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.get("/missing.txt");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.body == "not found");
  }

  {
    vix::note::NoteRoutesOptions options;
    options.enableAssets = false;

    vix::note::NoteRoutes routes(options);

    vix::note::NoteRouteResponse response =
        routes.get("/");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.body == "not found");
  }

  {
    vix::note::NoteDocument doc("API Doc");
    doc.set_path("examples/api.vixnote");

    doc.add_cell(
        vix::note::NoteCell(
            "intro",
            vix::note::NoteCellKind::Markdown,
            "# API Doc"));

    doc.add_cell(
        vix::note::NoteCell(
            "run",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.get("/api/document");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"title\":\"API Doc\""));
    assert(contains(response.body, "\"path\":\"examples/api.vixnote\""));
    assert(contains(response.body, "\"cellCount\":2"));
    assert(contains(response.body, "\"id\":\"intro\""));
    assert(contains(response.body, "\"kind\":\"markdown\""));
    assert(contains(response.body, "\"id\":\"run\""));
    assert(contains(response.body, "\"kind\":\"reply\""));
    assert(contains(response.body, "\"executable\":true"));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"index\":1"));
    assert(contains(response.body, "\"executionCount\":0"));
    assert(contains(response.body, "\"outputCount\":0"));
    assert(contains(response.body, "\"outputs\":[]"));
  }

  {
    vix::note::NoteDocument doc("Escaped Doc");

    doc.add_cell(
        vix::note::NoteCell(
            "quote",
            vix::note::NoteCellKind::Markdown,
            "# Title \"quoted\"\nline two"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.get("/api/document");

    assert(response.ok());
    assert(contains(response.body, "# Title \\\"quoted\\\"\\nline two"));
  }

  {
    vix::note::NoteRoutesOptions options;
    options.enableApi = false;

    vix::note::NoteRoutes routes(options);

    vix::note::NoteRouteResponse response =
        routes.get("/api/document");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.body == "not found");
  }

  {
    vix::note::NoteDocument doc("Output Doc");

    vix::note::NoteCell cell(
        "cpp-output",
        vix::note::NoteCellKind::Cpp,
        "int main() { return 0; }");

    cell.set_execution_count(3);
    cell.add_output(
        vix::note::NoteOutput::stdout_text("hello from document\n"));

    doc.add_cell(cell);

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.get("/api/document");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"title\":\"Output Doc\""));
    assert(contains(response.body, "\"id\":\"cpp-output\""));
    assert(contains(response.body, "\"kind\":\"cpp\""));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"executionCount\":3"));
    assert(contains(response.body, "\"outputCount\":1"));
    assert(contains(response.body, "\"outputs\":["));
    assert(contains(response.body, "\"kind\":\"stdout\""));
    assert(contains(response.body, "hello from document"));
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "reply",
            vix::note::NoteCellKind::Reply,
            "println(\"hello\")"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.post("/api/cells/0/run");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"status\":\"success\""));
    assert(contains(response.body, "\"message\":\"Reply cell executed\""));
    assert(contains(response.body, "hello"));

    assert(contains(response.body, "\"result\":{"));
    assert(contains(response.body, "\"cell\":{"));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"id\":\"reply\""));
    assert(contains(response.body, "\"executionCount\":1"));
    assert(contains(response.body, "\"outputCount\":1"));
    assert(contains(response.body, "\"outputs\":["));

    assert(routes.document().cells()[0].execution_count() == 1);
    assert(routes.kernel().session().has_records());
    assert(routes.kernel().session().records().size() == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp",
            vix::note::NoteCellKind::Cpp,
            "#include <iostream>\n"
            "int main()\n"
            "{\n"
            "  std::cout << \"routes cpp ok\" << std::endl;\n"
            "  return 0;\n"
            "}\n"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-routes-cpp");
    options.cppOptions.debugMode = true;
    options.cppOptions.includeRawLog = true;

    routes.kernel().set_options(options);

    vix::note::NoteRouteResponse response =
        routes.post("/api/cells/0/run");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"status\":\"success\""));
    assert(contains(response.body, "\"message\":\"C++ cell executed\""));
    assert(contains(response.body, "routes cpp ok"));
    assert(contains(response.body, "\"result\":{"));
    assert(contains(response.body, "\"cell\":{"));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"id\":\"cpp\""));
    assert(contains(response.body, "\"executionCount\":1"));
    assert(contains(response.body, "\"outputs\":["));
    assert(contains(response.body, "\"kind\":\"stdout\""));
    assert(contains(response.body, "\"kind\":\"raw_log\""));
    assert(routes.document().cells()[0].execution_count() == 1);
    assert(routes.document().cells()[0].has_outputs());
    assert(routes.document().cells()[0].outputs()[0].content.find("routes cpp ok") != std::string::npos);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp",
            vix::note::NoteCellKind::Cpp,
            "int main() { return 0; }\n"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-routes-cpp-fail");

    options.cppOptions.runArgs.push_back("--fail");

    routes.kernel().set_options(options);

    vix::note::NoteRouteResponse response =
        routes.post("/api/cells/0/run");

    assert(response.status == 200);
    assert(response.ok());
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "\"status\":\"failure\""));
    assert(contains(response.body, "\"exitCode\":7"));
    assert(contains(response.body, "simulated routes failure"));
    assert(contains(response.body, "\"result\":{"));
    assert(contains(response.body, "\"cell\":{"));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"id\":\"cpp\""));
    assert(contains(response.body, "\"executionCount\":1"));
    assert(contains(response.body, "\"kind\":\"stdout\""));
    assert(contains(response.body, "\"kind\":\"error\""));
    assert(routes.document().cells()[0].execution_count() == 1);
    assert(routes.kernel().session().records().size() == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_markdown("# Intro");
    doc.add_reply("println(\"hello\")");

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.post("/api/run-all");

    assert(response.ok());
    assert(response.status == 200);
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"stopped\":false"));
    assert(contains(response.body, "\"visited\":2"));
    assert(contains(response.body, "\"executed\":1"));
    assert(contains(response.body, "\"skipped\":0"));
    assert(contains(response.body, "\"failed\":0"));
    assert(contains(response.body, "\"status\":\"success\""));
    assert(contains(response.body, "\"results\":["));
    assert(contains(response.body, "\"document\":{"));
    assert(contains(response.body, "\"cellCount\":2"));
    assert(contains(response.body, "\"index\":0"));
    assert(contains(response.body, "\"index\":1"));
    assert(contains(response.body, "\"executionCount\":1"));

    assert(routes.document().cells()[0].execution_count() == 0);
    assert(routes.document().cells()[1].execution_count() == 1);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cpp("int main() { return 0; }\n");
    doc.add_cpp("int main() { return 0; }\n");

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-routes-run-all-fail");

    options.stopOnFirstFailure = true;
    options.cppOptions.runArgs.push_back("--fail");

    routes.kernel().set_options(options);

    vix::note::NoteRouteResponse response =
        routes.post("/api/run-all");

    assert(response.status == 200);
    assert(response.ok());

    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "\"stopped\":true"));
    assert(contains(response.body, "\"visited\":1"));
    assert(contains(response.body, "\"executed\":1"));
    assert(contains(response.body, "\"skipped\":0"));
    assert(contains(response.body, "\"failed\":1"));
    assert(contains(response.body, "simulated routes failure"));

    assert(routes.document().cells()[0].execution_count() == 1);
    assert(routes.document().cells()[1].execution_count() == 0);
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.post("/api/cells/abc/run");

    assert(response.status == 200);
    assert(response.ok());
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "\"result\":{"));
    assert(contains(response.body, "\"status\":\"failure\""));
    assert(contains(response.body, "\"message\":\"cell not found: abc\""));
    assert(contains(response.body, "\"cell\":null"));
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.post("/api/cells/99/run");

    assert(response.status == 200);
    assert(response.ok());
    assert(response.contentType == "application/json; charset=utf-8");

    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "\"result\":{"));
    assert(contains(response.body, "\"status\":\"failure\""));
    assert(contains(response.body, "\"message\":\"cell index out of range\""));
    assert(contains(response.body, "\"cell\":null"));
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "cpp-id",
            vix::note::NoteCellKind::Cpp,
            "#include <iostream>\n"
            "int main()\n"
            "{\n"
            "  std::cout << \"run by id\" << std::endl;\n"
            "  return 0;\n"
            "}\n"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteKernelOptions options =
        make_kernel_options(fakeVix, root / "tmp-routes-run-by-id");

    routes.kernel().set_options(options);

    vix::note::NoteRouteResponse response =
        routes.post("/api/cells/cpp-id/run");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"id\":\"cpp-id\""));
    assert(contains(response.body, "run by id"));

    assert(routes.document().cells()[0].execution_count() == 1);
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.post(
            "/api/cells",
            "{\"id\":\"new-md\",\"kind\":\"markdown\",\"source\":\"# New Cell\"}");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell added\""));
    assert(contains(response.body, "\"cellId\":\"new-md\""));
    assert(contains(response.body, "\"id\":\"new-md\""));
    assert(contains(response.body, "\"kind\":\"markdown\""));
    assert(contains(response.body, "# New Cell"));

    assert(routes.document().cell_count() == 1);
    assert(routes.document().cells()[0].id() == "new-md");
    assert(routes.document().cells()[0].kind() == vix::note::NoteCellKind::Markdown);
    assert(routes.document().cells()[0].source() == "# New Cell");
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.post(
            "/api/cells",
            "{\"kind\":\"cpp\",\"source\":\"int main() { return 0; }\"}");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell added\""));
    assert(contains(response.body, "\"kind\":\"cpp\""));

    assert(routes.document().cell_count() == 1);
    assert(!routes.document().cells()[0].id().empty());
    assert(routes.document().cells()[0].kind() == vix::note::NoteCellKind::Cpp);
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "a",
            vix::note::NoteCellKind::Markdown,
            "A"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.post(
            "/api/cells",
            "{\"id\":\"a\",\"kind\":\"markdown\",\"source\":\"Duplicate\"}");

    assert(response.status == 409);
    assert(!response.ok());
    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "cell id already exists"));

    assert(routes.document().cell_count() == 1);
    assert(routes.document().cells()[0].source() == "A");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "edit",
            vix::note::NoteCellKind::Markdown,
            "Old source"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.put(
            "/api/cells/edit",
            "{\"kind\":\"cpp\",\"source\":\"int main() { return 0; }\"}");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell updated\""));
    assert(contains(response.body, "\"cellId\":\"edit\""));
    assert(contains(response.body, "\"kind\":\"cpp\""));

    assert(routes.document().cells()[0].id() == "edit");
    assert(routes.document().cells()[0].kind() == vix::note::NoteCellKind::Cpp);
    assert(routes.document().cells()[0].source() == "int main() { return 0; }");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "move-a",
            vix::note::NoteCellKind::Markdown,
            "A"));

    doc.add_cell(
        vix::note::NoteCell(
            "move-b",
            vix::note::NoteCellKind::Markdown,
            "B"));

    doc.add_cell(
        vix::note::NoteCell(
            "move-c",
            vix::note::NoteCellKind::Markdown,
            "C"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.post(
            "/api/cells/move-c/move",
            "{\"index\":0}");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell moved\""));
    assert(contains(response.body, "\"cellId\":\"move-c\""));

    assert(routes.document().cells()[0].id() == "move-c");
    assert(routes.document().cells()[1].id() == "move-a");
    assert(routes.document().cells()[2].id() == "move-b");
  }

  {
    vix::note::NoteDocument doc;

    doc.add_cell(
        vix::note::NoteCell(
            "delete-me",
            vix::note::NoteCellKind::Markdown,
            "Delete me"));

    doc.add_cell(
        vix::note::NoteCell(
            "keep-me",
            vix::note::NoteCellKind::Markdown,
            "Keep me"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.delete_request("/api/cells/delete-me");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"message\":\"cell deleted\""));
    assert(contains(response.body, "\"cellId\":\"delete-me\""));

    assert(routes.document().cell_count() == 1);
    assert(routes.document().cells()[0].id() == "keep-me");
  }

  {
    vix::note::NoteRoutesOptions options;
    options.enableEditing = false;

    vix::note::NoteRoutes routes(options);

    vix::note::NoteRouteResponse response =
        routes.post(
            "/api/cells",
            "{\"id\":\"blocked\",\"kind\":\"markdown\",\"source\":\"Blocked\"}");

    assert(response.status == 403);
    assert(!response.ok());
    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "editing disabled"));
  }

  {
    vix::note::NoteDocument doc;

    const std::filesystem::path file =
        root / "save" / "routes-save.vixnote";

    doc.set_path(file.string());

    doc.add_cell(
        vix::note::NoteCell(
            "save-md",
            vix::note::NoteCellKind::Markdown,
            "# Saved From Routes"));

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.post("/api/document/save");

    assert(response.ok());
    assert(response.status == 200);
    assert(contains(response.body, "\"ok\":true"));
    assert(contains(response.body, "\"status\":\"success\""));
    assert(contains(response.body, "\"message\":\"note saved\""));
    assert(std::filesystem::exists(file));

    const std::string saved =
        read_file(file);

    assert(contains(saved, "# Saved From Routes"));
  }

  {
    vix::note::NoteDocument doc;
    doc.add_markdown("# No Path");

    vix::note::NoteRoutes routes(doc);

    vix::note::NoteRouteResponse response =
        routes.post("/api/document/save");

    assert(response.status == 500);
    assert(!response.ok());
    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "\"message\":\"note document has no path\""));
  }

  {
    vix::note::NoteRoutesOptions options;
    options.enableSave = false;

    vix::note::NoteRoutes routes(options);

    vix::note::NoteRouteResponse response =
        routes.post("/api/document/save");

    assert(response.status == 403);
    assert(!response.ok());
    assert(contains(response.body, "\"ok\":false"));
    assert(contains(response.body, "save disabled"));
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteRequest request;
    request.method = vix::note::NoteRouteMethod::Put;
    request.path = "/api/document";

    vix::note::NoteRouteResponse response =
        routes.handle(request);

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.contentType == "application/json; charset=utf-8");
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteRouteResponse response =
        routes.put("/api/document", "{}");

    assert(response.status == 404);
    assert(!response.ok());
    assert(response.contentType == "application/json; charset=utf-8");
  }

  {
    vix::note::NoteRoutes routes;

    vix::note::NoteDocument doc("Replaced");
    doc.add_cpp("int main() { return 0; }");

    routes.set_document(doc);

    assert(routes.document().title() == "Replaced");
    assert(routes.document().cell_count() == 1);
    assert(routes.kernel().can_execute_cell(0));
  }


  {
    const std::filesystem::path home = root / "home-extensions";
    const std::filesystem::path global = home / ".vix" / "global";
    const std::filesystem::path pkgRoot = global / "packages" / "softadastra.pyrelune";
    std::filesystem::create_directories(pkgRoot);

#ifndef _WIN32
    setenv("HOME", home.string().c_str(), 1);
    setenv("VIX_GLOBAL_PREFIX", global.string().c_str(), 1);
#else
    _putenv_s("USERPROFILE", home.string().c_str());
    _putenv_s("VIX_GLOBAL_PREFIX", global.string().c_str());
#endif

    write_file(
        global / "installed.json",
        "{\n"
        "  \"packages\": [\n"
        "    {\n"
        "      \"id\": \"softadastra/pyrelune\",\n"
        "      \"version\": \"0.1.0\",\n"
        "      \"installed_path\": \".\",\n"
        "      \"extensions\": {\n"
        "        \"note\": {\n"
        "          \"api\": \"1\",\n"
        "          \"capabilities\": [\"cell-type\"],\n"
        "          \"cellTypes\": [\n"
        "            {\"id\": \"python\", \"label\": \"Python\", \"language\": \"python\", \"executable\": false}\n"
        "          ]\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    vix::note::NoteExtensionManager manager;
    manager.reload({}, true);

    assert(manager.registry().find_extension("softadastra/pyrelune") != nullptr);
    assert(manager.registry().find_cell_type("python") != nullptr);

    std::string error;
    assert(!manager.set_extension_enabled("vix.note.cpp", false, error));
    assert(contains(error, "built-in"));

    vix::note::NoteRoutesOptions options;
    options.allowPackageMutations = true;
    options.kernelOptions.extensionRegistry = &manager.registry();
    options.reloadExtensions = [&manager]() {
      manager.reload({}, true);
      return vix::note::NoteResult::success("extensions reloaded");
    };
    options.setExtensionEnabled = [&manager](const std::string &packageId, bool enabled) {
      std::string message;
      if (!manager.set_extension_enabled(packageId, enabled, message))
      {
        return vix::note::NoteResult::failure(message, 1).add_error(message);
      }
      return vix::note::NoteResult::success("extension state updated");
    };

    vix::note::NoteRoutes routes(options);

    vix::note::NoteRouteResponse before = routes.get("/api/extensions");
    assert(before.ok());
    assert(contains(before.body, "softadastra/pyrelune"));
    assert(contains(before.body, "\"enabled\":true"));
    assert(contains(before.body, "\"id\":\"python\""));

    vix::note::NoteRouteResponse disabled =
        routes.post("/api/extensions/disable", "{\"package\":\"softadastra/pyrelune\"}");
    assert(disabled.ok());
    assert(contains(disabled.body, "\"action\":\"disable\""));
    assert(contains(disabled.body, "\"enabled\":false"));

    vix::note::NoteRouteResponse afterDisable = routes.get("/api/extensions");
    assert(afterDisable.ok());
    assert(contains(afterDisable.body, "\"enabled\":false"));

    vix::note::NoteExtensionManager restarted;
    restarted.reload({}, true);
    const auto *disabledExt = restarted.registry().find_extension("softadastra/pyrelune");
    assert(disabledExt != nullptr);
    assert(!disabledExt->enabled);
    assert(restarted.registry().find_cell_type("python") == nullptr);

    vix::note::NoteRouteResponse enabled =
        routes.post("/api/extensions/enable", "{\"package\":\"softadastra/pyrelune\"}");
    assert(enabled.ok());
    assert(contains(enabled.body, "\"action\":\"enable\""));
    assert(contains(enabled.body, "\"enabled\":true"));

    vix::note::NoteRouteResponse reloaded =
        routes.post("/api/extensions/reload");
    assert(reloaded.ok());
    assert(contains(reloaded.body, "\"action\":\"reload\""));
  }

  std::filesystem::remove_all(root);

  return 0;
}
