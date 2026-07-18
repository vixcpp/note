/**
 *
 *  @file NoteExtensionRegistry.cpp
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
#include <vix/note/extensions/NoteExtensionRegistry.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace vix::note
{
  namespace
  {
    bool contains_string(const std::vector<std::string> &items, const std::string &value)
    {
      return std::find(items.begin(), items.end(), value) != items.end();
    }

    bool is_safe_command_name(const std::string &value)
    {
      if (value.empty())
        return false;
      if (value.find('/') != std::string::npos || value.find('\\') != std::string::npos)
        return false;
      for (char c : value)
      {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (!ok)
          return false;
      }
      return true;
    }

    std::filesystem::path home_dir()
    {
#ifdef _WIN32
      const char *home = std::getenv("USERPROFILE");
#else
      const char *home = std::getenv("HOME");
#endif
      return home ? std::filesystem::path(home) : std::filesystem::path();
    }

    std::filesystem::path global_prefix()
    {
      if (const char *prefix = std::getenv("VIX_GLOBAL_PREFIX"))
      {
        if (*prefix)
          return std::filesystem::path(prefix);
      }
      const auto home = home_dir();
      return home.empty() ? std::filesystem::path(".vix/global") : home / ".vix" / "global";
    }

    std::filesystem::path note_preferences_path()
    {
      const auto home = home_dir();
      const auto root = home.empty() ? std::filesystem::path(".vix") : home / ".vix";
      return root / "note" / "extensions.json";
    }

    bool is_disabled_package_id(const std::vector<std::string> &items, const std::string &id)
    {
      return std::find(items.begin(), items.end(), id) != items.end();
    }

    std::vector<std::string> load_disabled_extensions()
    {
      std::vector<std::string> disabled;
      std::ifstream in(note_preferences_path());
      if (!in)
        return disabled;
      json root;
      try
      {
        in >> root;
      }
      catch (...)
      {
        return disabled;
      }
      if (!root.contains("disabled") || !root["disabled"].is_array())
        return disabled;
      for (const auto &item : root["disabled"])
      {
        if (!item.is_string())
          continue;
        const std::string id = item.get<std::string>();
        if (!id.empty() && !is_disabled_package_id(disabled, id))
          disabled.push_back(id);
      }
      return disabled;
    }

    bool save_disabled_extensions(const std::vector<std::string> &disabled, std::string &error)
    {
      error.clear();
      const std::filesystem::path path = note_preferences_path();
      std::error_code ec;
      std::filesystem::create_directories(path.parent_path(), ec);
      if (ec)
      {
        error = "cannot create note preferences directory: " + ec.message();
        return false;
      }
      const std::filesystem::path tmp = path.string() + ".tmp";
      json root;
      root["disabled"] = disabled;
      {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
        {
          error = "cannot write note extension preferences";
          return false;
        }
        out << root.dump(2) << '\n';
        if (!out)
        {
          error = "cannot flush note extension preferences";
          return false;
        }
      }
      std::filesystem::rename(tmp, path, ec);
      if (ec)
      {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tmp, path, ec);
      }
      if (ec)
      {
        error = "cannot replace note extension preferences: " + ec.message();
        return false;
      }
      return true;
    }

    std::vector<std::string> string_array(const json &j, const char *key)
    {
      std::vector<std::string> out;
      if (!j.contains(key) || !j.at(key).is_array())
        return out;
      for (const auto &item : j.at(key))
      {
        if (!item.is_string())
          continue;
        std::string value = normalize_cell_type_id(item.get<std::string>());
        if (!value.empty() && !contains_string(out, value))
          out.push_back(value);
      }
      return out;
    }

    NoteCellTypeDescriptor parse_cell_type(const json &item)
    {
      NoteCellTypeDescriptor cell;
      cell.id = normalize_cell_type_id(item.value("id", ""));
      cell.label = item.value("label", cell.id);
      cell.language = normalize_cell_type_id(item.value("language", cell.id));
      cell.aliases = string_array(item, "aliases");
      cell.executable = item.value("executable", false);
      cell.commentLine = item.value("commentLine", std::string{});
      cell.commentBlock = item.value("commentBlock", std::string{});
      cell.defaultSource = item.value("defaultSource", std::string{});
      cell.placeholder = item.value("placeholder", std::string{});
      if (cell.placeholder.empty())
        cell.placeholder = "Write your explanation here.";
      return cell;
    }

    std::optional<NoteExtensionDescriptor> descriptor_from_note_json(
        const json &note,
        const std::string &packageId,
        const std::string &version,
        NoteExtensionSource source,
        const std::filesystem::path &rootPath,
        const std::filesystem::path &binDir,
        std::vector<std::string> *diagnostics)
    {
      if (!note.is_object())
      {
        if (diagnostics)
          diagnostics->push_back(packageId + ": extensions.note must be an object");
        return std::nullopt;
      }
      const std::string api = note.value("api", "");
      if (api != "1")
      {
        if (diagnostics)
          diagnostics->push_back(packageId + ": unsupported note extension API: " + (api.empty() ? "<missing>" : api));
        return std::nullopt;
      }
      if (!note.contains("cellTypes") || !note.at("cellTypes").is_array())
      {
        if (diagnostics)
          diagnostics->push_back(packageId + ": extensions.note.cellTypes must be an array");
        return std::nullopt;
      }

      NoteExtensionDescriptor d;
      d.id = packageId;
      d.packageId = packageId;
      d.version = version;
      d.apiVersion = api;
      d.source = source;
      d.rootPath = rootPath;
      d.capabilities = string_array(note, "capabilities");
      d.permissions = string_array(note, "permissions");

      for (const auto &item : note.at("cellTypes"))
      {
        if (!item.is_object())
        {
          d.diagnostics.push_back("cellTypes entries must be objects");
          continue;
        }
        auto cell = parse_cell_type(item);
        if (cell.id.empty() || cell.id == "unknown")
        {
          d.diagnostics.push_back("cell type id is empty or invalid");
          continue;
        }
        if (is_builtin_cell_type(cell.id))
        {
          d.diagnostics.push_back("external extension cannot replace reserved cell type: " + cell.id);
          continue;
        }
        cell.extensionId = d.id;
        d.cellTypes.push_back(std::move(cell));
      }

      if (d.cellTypes.empty())
      {
        d.available = false;
        d.diagnostics.push_back("no valid cell types declared");
      }

      if (note.contains("runtime") && note.at("runtime").is_object())
      {
        const auto &rt = note.at("runtime");
        d.runtimeProtocol = rt.value("protocol", "");
        d.runtimeMode = rt.value("mode", "");
        if (d.runtimeProtocol != "vix-note-extension-1")
        {
          d.available = false;
          d.diagnostics.push_back("unsupported runtime protocol: " + d.runtimeProtocol);
        }
        if (d.runtimeMode != "oneshot")
        {
          d.available = false;
          d.diagnostics.push_back("unsupported runtime mode: " + d.runtimeMode);
        }
        if (rt.contains("command"))
        {
          const std::string command = rt.value("command", "");
          if (!is_safe_command_name(command))
          {
            d.available = false;
            d.diagnostics.push_back("unsafe runtime command: " + command);
          }
          else
          {
            d.runtimeCommand = (binDir / command).string();
            std::error_code ec;
            if (!std::filesystem::exists(d.runtimeCommand, ec))
            {
              d.available = false;
              d.diagnostics.push_back("Runtime command not found: " + command);
            }
          }
        }
        else if (rt.contains("path"))
        {
          std::filesystem::path rel = rt.value("path", "");
          if (rel.empty() || rel.is_absolute())
          {
            d.available = false;
            d.diagnostics.push_back("runtime path must be relative");
          }
          for (const auto &part : rel)
          {
            if (part == "..")
              d.available = false;
          }
          d.runtimeCommand = (rootPath / rel).lexically_normal().string();
          std::error_code ec;
          if (!std::filesystem::exists(d.runtimeCommand, ec))
          {
            d.available = false;
            d.diagnostics.push_back("Runtime path not found: " + rel.generic_string());
          }
        }
        else if (std::any_of(d.cellTypes.begin(), d.cellTypes.end(), [](const auto &c)
                             { return c.executable; }))
        {
          d.available = false;
          d.diagnostics.push_back("runtime command/path is required for executable cell types");
        }
      }
      else if (std::any_of(d.cellTypes.begin(), d.cellTypes.end(), [](const auto &c)
                           { return c.executable; }))
      {
        d.available = false;
        d.diagnostics.push_back("runtime is required for executable cell types");
      }

      return d;
    }

    class CppRunnerAdapter final : public NoteCellRunner
    {
    public:
      explicit CppRunnerAdapter(CppCellRunnerOptions options) : runner_(std::move(options)) {}
      NoteResult run(const NoteCell &cell, NoteExecutionContext &) override { return runner_.run_cell(cell); }

    private:
      CppCellRunner runner_;
    };

    class ReplyRunnerAdapter final : public NoteCellRunner
    {
    public:
      explicit ReplyRunnerAdapter(ReplyCellRunnerOptions options) : runner_(std::move(options)) {}
      NoteResult run(const NoteCell &cell, NoteExecutionContext &) override { return runner_.run_cell(cell); }

    private:
      ReplyCellRunner runner_;
    };
  }

  bool NoteExtensionRegistry::register_extension(NoteExtensionDescriptor descriptor)
  {
    const std::string id = descriptor.id;
    auto existing = std::find_if(extensions_.begin(), extensions_.end(), [&](const auto &e)
                                 { return e.id == id; });
    if (existing != extensions_.end())
      *existing = descriptor;
    else
      extensions_.push_back(std::move(descriptor));
    return true;
  }

  bool NoteExtensionRegistry::register_cell_type(NoteCellTypeDescriptor descriptor, RunnerFactory factory)
  {
    descriptor.id = normalize_cell_type_id(descriptor.id);
    if (descriptor.id.empty() || descriptor.id == "unknown")
    {
      diagnostics_.push_back("invalid cell type id");
      return false;
    }
    if (!descriptor.builtin && is_builtin_cell_type(descriptor.id))
    {
      diagnostics_.push_back("external extension cannot replace reserved cell type: " + descriptor.id);
      return false;
    }
    auto existing = std::find_if(cellTypes_.begin(), cellTypes_.end(), [&](const auto &c)
                                 { return c.id == descriptor.id; });
    if (existing != cellTypes_.end())
    {
      if (existing->builtin)
      {
        diagnostics_.push_back("reserved built-in cell type cannot be replaced: " + descriptor.id);
        return false;
      }
      if (existing->extensionId != descriptor.extensionId)
      {
        diagnostics_.push_back("cell type conflict for '" + descriptor.id + "' between " + existing->extensionId + " and " + descriptor.extensionId);
        return false;
      }
      *existing = descriptor;
    }
    else
      cellTypes_.push_back(descriptor);

    if (factory)
    {
      auto fit = std::find_if(factories_.begin(), factories_.end(), [&](const auto &f)
                              { return f.first == descriptor.id; });
      if (fit != factories_.end())
        fit->second = std::move(factory);
      else
        factories_.push_back({descriptor.id, std::move(factory)});
    }
    return true;
  }

  const NoteExtensionDescriptor *NoteExtensionRegistry::find_extension(const std::string &id) const noexcept
  {
    auto it = std::find_if(extensions_.begin(), extensions_.end(), [&](const auto &e)
                           { return e.id == id; });
    return it == extensions_.end() ? nullptr : &*it;
  }

  const NoteCellTypeDescriptor *NoteExtensionRegistry::find_cell_type(const std::string &id) const noexcept
  {
    const std::string normalized = normalize_cell_type_id(id);
    auto it = std::find_if(cellTypes_.begin(), cellTypes_.end(), [&](const auto &c)
                           { return c.id == normalized; });
    return it == cellTypes_.end() ? nullptr : &*it;
  }

  const NoteCellTypeDescriptor *NoteExtensionRegistry::find_cell_type_by_alias(const std::string &alias) const noexcept
  {
    const std::string normalized = normalize_cell_type_id(alias);
    for (const auto &cell : cellTypes_)
      if (contains_string(cell.aliases, normalized))
        return &cell;
    return nullptr;
  }

  std::vector<NoteExtensionDescriptor> NoteExtensionRegistry::list_extensions() const { return extensions_; }
  std::vector<NoteCellTypeDescriptor> NoteExtensionRegistry::list_cell_types() const { return cellTypes_; }
  const std::vector<std::string> &NoteExtensionRegistry::diagnostics() const noexcept { return diagnostics_; }

  std::unique_ptr<NoteCellRunner> NoteExtensionRegistry::create_runner_for(const std::string &cellTypeId) const
  {
    const std::string normalized = normalize_cell_type_id(cellTypeId);
    auto it = std::find_if(factories_.begin(), factories_.end(), [&](const auto &f)
                           { return f.first == normalized; });
    return it == factories_.end() ? nullptr : it->second();
  }

  NoteExtensionRegistry &NoteExtensionManager::registry() noexcept { return registry_; }
  const NoteExtensionRegistry &NoteExtensionManager::registry() const noexcept { return registry_; }

  void NoteExtensionManager::register_builtins(const CppCellRunnerOptions &cppOptions, const ReplyCellRunnerOptions &replyOptions)
  {
    cppOptions_ = cppOptions;
    replyOptions_ = replyOptions;

    auto add = [&](const std::string &extId, NoteCellTypeDescriptor cell, NoteExtensionRegistry::RunnerFactory factory)
    {
      NoteExtensionDescriptor ext;
      ext.id = extId;
      ext.packageId = extId;
      ext.version = "builtin";
      ext.apiVersion = "1";
      ext.source = NoteExtensionSource::Builtin;
      ext.capabilities = {"cell-type"};
      if (cell.executable)
        ext.capabilities.push_back("kernel");
      cell.builtin = true;
      cell.extensionId = ext.id;
      cell.placeholder = cell.placeholder.empty() ? "Write your explanation here." : cell.placeholder;
      if (cell.id == "markdown" && cell.defaultSource.empty())
        cell.defaultSource = "Write your explanation here.";
      if (cell.id == "html" && cell.defaultSource.empty())
        cell.defaultSource = "<!-- Write your explanation here. -->\n";
      if ((cell.id == "cpp" || cell.id == "reply") && cell.commentLine.empty())
        cell.commentLine = "//";
      if ((cell.id == "cpp" || cell.id == "reply") && cell.defaultSource.empty())
        cell.defaultSource = "// Write your explanation here.\n";
      ext.cellTypes.push_back(cell);
      registry_.register_extension(ext);
      registry_.register_cell_type(cell, std::move(factory));
    };
    NoteCellTypeDescriptor markdown;
    markdown.id = "markdown";
    markdown.label = "Markdown";
    markdown.language = "markdown";
    markdown.aliases = {"md"};
    markdown.defaultSource = "Write your explanation here.";
    add("vix.note.markdown", std::move(markdown), {});

    NoteCellTypeDescriptor html;
    html.id = "html";
    html.label = "HTML";
    html.language = "html";
    html.defaultSource = "<!-- Write your explanation here. -->\n";
    add("vix.note.html", std::move(html), {});

    NoteCellTypeDescriptor cpp;
    cpp.id = "cpp";
    cpp.label = "C++";
    cpp.language = "cpp";
    cpp.aliases = {"c++"};
    cpp.executable = true;
    cpp.commentLine = "//";
    cpp.defaultSource = "// Write your explanation here.\n";
    add("vix.note.cpp", std::move(cpp), [cppOptions]
        { return std::make_unique<CppRunnerAdapter>(cppOptions); });

    NoteCellTypeDescriptor reply;
    reply.id = "reply";
    reply.label = "Reply";
    reply.language = "reply";
    reply.aliases = {"repl"};
    reply.executable = true;
    reply.commentLine = "//";
    reply.defaultSource = "// Write your explanation here.\n";
    add("vix.note.reply", std::move(reply), [replyOptions]
        { return std::make_unique<ReplyRunnerAdapter>(replyOptions); });
  }

  void NoteExtensionManager::discover_global()
  {
    includeExternal_ = true;
    const std::vector<std::string> disabled = load_disabled_extensions();
    const auto prefix = global_prefix();
    const auto manifest = prefix / "installed.json";
    std::ifstream in(manifest);
    if (!in)
      return;
    json root;
    try
    {
      in >> root;
    }
    catch (...)
    {
      return;
    }
    if (!root.contains("packages") || !root["packages"].is_array())
      return;
    for (const auto &pkg : root["packages"])
    {
      if (!pkg.is_object() || !pkg.contains("extensions") || !pkg["extensions"].contains("note"))
        continue;
      const std::string id = pkg.value("id", "");
      auto d = descriptor_from_note_json(pkg["extensions"]["note"], id, pkg.value("version", ""), NoteExtensionSource::Global, pkg.value("installed_path", ""), prefix / "bin", nullptr);
      if (!d)
        continue;
      d->enabled = !is_disabled_package_id(disabled, d->id);
      registry_.register_extension(*d);
      if (!d->enabled)
        continue;
      for (const auto &cell : d->cellTypes)
      {
        NoteExtensionDescriptor captured = *d;
        registry_.register_cell_type(cell, [captured]
                                     { return std::make_unique<ExternalProcessCellRunner>(captured); });
      }
    }
  }

  void NoteExtensionManager::discover_project(const ProjectContext &context)
  {
    projectContext_ = context;
    const std::vector<std::string> disabled = load_disabled_extensions();
    if (!context.enabled || context.projectRoot.empty())
      return;
    const auto deps = context.projectRoot / ".vix" / "deps";
    std::error_code ec;
    if (!std::filesystem::exists(deps, ec))
      return;
    for (const auto &entry : std::filesystem::directory_iterator(deps, ec))
    {
      if (ec || !entry.is_directory())
        continue;
      const auto manifest = entry.path() / "vix.json";
      std::ifstream in(manifest);
      if (!in)
        continue;
      json root;
      try
      {
        in >> root;
      }
      catch (...)
      {
        continue;
      }
      if (!root.contains("extensions") || !root["extensions"].contains("note"))
        continue;
      const std::string id = root.value("namespace", "") + "/" + root.value("name", entry.path().filename().string());
      auto d = descriptor_from_note_json(root["extensions"]["note"], id, root.value("version", ""), NoteExtensionSource::Project, entry.path(), {}, nullptr);
      if (!d)
        continue;
      d->enabled = !is_disabled_package_id(disabled, d->id);
      registry_.register_extension(*d);
      if (!d->enabled)
        continue;
      for (const auto &cell : d->cellTypes)
      {
        NoteExtensionDescriptor captured = *d;
        registry_.register_cell_type(cell, [captured]
                                     { return std::make_unique<ExternalProcessCellRunner>(captured); });
      }
    }
  }

  void NoteExtensionManager::reload(const ProjectContext &context, bool includeExternal)
  {
    projectContext_ = context;
    includeExternal_ = includeExternal;
    registry_ = NoteExtensionRegistry{};
    register_builtins(cppOptions_, replyOptions_);
    if (includeExternal_)
    {
      discover_global();
      discover_project(projectContext_);
    }
  }

  bool NoteExtensionManager::set_extension_enabled(const std::string &packageId, bool enabled, std::string &error)
  {
    error.clear();
    const NoteExtensionDescriptor *ext = registry_.find_extension(packageId);
    if (ext == nullptr)
    {
      error = "extension is not installed: " + packageId;
      return false;
    }
    if (ext->source == NoteExtensionSource::Builtin)
    {
      error = "built-in extensions cannot be enabled or disabled";
      return false;
    }
    std::vector<std::string> disabled = load_disabled_extensions();
    const bool alreadyDisabled = is_disabled_package_id(disabled, packageId);
    if (enabled && alreadyDisabled)
    {
      disabled.erase(std::remove(disabled.begin(), disabled.end(), packageId), disabled.end());
    }
    else if (!enabled && !alreadyDisabled)
    {
      disabled.push_back(packageId);
    }
    if (!save_disabled_extensions(disabled, error))
      return false;
    reload(projectContext_, includeExternal_);
    return true;
  }

  bool NoteExtensionManager::is_extension_disabled(const std::string &packageId) const
  {
    return is_disabled_package_id(load_disabled_extensions(), packageId);
  }

  ExternalProcessCellRunner::ExternalProcessCellRunner(NoteExtensionDescriptor descriptor)
      : descriptor_(std::move(descriptor)) {}

  NoteResult ExternalProcessCellRunner::run(const NoteCell &cell, NoteExecutionContext &context)
  {
    if (!descriptor_.available)
    {
      NoteResult result = NoteResult::failure("extension is unavailable", 1);
      for (const auto &diag : descriptor_.diagnostics)
        result.add_error(diag);
      return result;
    }
    if (descriptor_.runtimeProtocol != "vix-note-extension-1")
      return NoteResult::failure("unsupported runtime protocol: " + descriptor_.runtimeProtocol, 1).add_error("unsupported runtime protocol");
    if (descriptor_.runtimeMode != "oneshot")
      return NoteResult::failure("unsupported runtime mode: " + descriptor_.runtimeMode, 1).add_error("only oneshot mode is supported");

    json request = {
        {"protocol", "vix-note-extension-1"},
        {"requestId", context.documentId + ":" + cell.id()},
        {"cellId", cell.id()},
        {"cellType", cell.type_id()},
        {"source", cell.source()},
        {"executionCount", cell.execution_count()},
        {"workingDirectory", context.projectContext.effective_working_directory().string()},
        {"documentPath", context.documentPath.string()},
        {"extensionId", descriptor_.id},
        {"extensionVersion", descriptor_.version}};

#ifdef _WIN32
    (void)request;
    return NoteResult::failure("external note extension runner is not implemented on Windows yet", 1)
        .add_error("external note extension runner is not implemented on Windows yet");
#else
    int inPipe[2];
    int outPipe[2];
    int errPipe[2];
    if (pipe(inPipe) != 0 || pipe(outPipe) != 0 || pipe(errPipe) != 0)
      return NoteResult::failure("cannot create process pipes", 1).add_error("cannot create process pipes");

    const pid_t pid = fork();
    if (pid == 0)
    {
      dup2(inPipe[0], STDIN_FILENO);
      dup2(outPipe[1], STDOUT_FILENO);
      dup2(errPipe[1], STDERR_FILENO);
      close(inPipe[0]);
      close(inPipe[1]);
      close(outPipe[0]);
      close(outPipe[1]);
      close(errPipe[0]);
      close(errPipe[1]);
      execl(descriptor_.runtimeCommand.c_str(), descriptor_.runtimeCommand.c_str(), static_cast<char *>(nullptr));
      _exit(127);
    }
    close(inPipe[0]);
    close(outPipe[1]);
    close(errPipe[1]);
    if (pid < 0)
      return NoteResult::failure("cannot start extension process", 1).add_error("cannot start extension process");

    const std::string input = request.dump();
    (void)write(inPipe[1], input.data(), input.size());
    close(inPipe[1]);

    std::string stdoutText;
    std::string stderrText;
    char buffer[4096];
    auto start = std::chrono::steady_clock::now();
    int status = 0;
    bool done = false;
    bool tooLarge = false;
    while (!done)
    {
      fd_set set;
      FD_ZERO(&set);
      FD_SET(outPipe[0], &set);
      FD_SET(errPipe[0], &set);
      int maxFd = std::max(outPipe[0], errPipe[0]);
      timeval tv{0, 100000};
      if (select(maxFd + 1, &set, nullptr, nullptr, &tv) > 0)
      {
        if (FD_ISSET(outPipe[0], &set))
        {
          ssize_t n = read(outPipe[0], buffer, sizeof(buffer));
          if (n > 0)
            stdoutText.append(buffer, buffer + n);
        }
        if (FD_ISSET(errPipe[0], &set))
        {
          ssize_t n = read(errPipe[0], buffer, sizeof(buffer));
          if (n > 0)
            stderrText.append(buffer, buffer + n);
        }
      }
      if (stdoutText.size() + stderrText.size() > context.outputSizeLimit)
      {
        tooLarge = true;
        kill(pid, SIGKILL);
      }
      if (std::chrono::steady_clock::now() - start > context.timeout)
      {
        kill(pid, SIGKILL);
        close(outPipe[0]);
        close(errPipe[0]);
        return NoteResult::failure("extension execution timed out", 124).add_error("timeout");
      }
      const pid_t r = waitpid(pid, &status, WNOHANG);
      done = r == pid;
    }
    while (true)
    {
      ssize_t n = read(outPipe[0], buffer, sizeof(buffer));
      if (n <= 0)
        break;
      stdoutText.append(buffer, buffer + n);
    }
    while (true)
    {
      ssize_t n = read(errPipe[0], buffer, sizeof(buffer));
      if (n <= 0)
        break;
      stderrText.append(buffer, buffer + n);
    }
    close(outPipe[0]);
    close(errPipe[0]);
    if (tooLarge)
      return NoteResult::failure("extension output is too large", 1).add_error("output too large");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      return NoteResult::failure("extension process failed", WIFEXITED(status) ? WEXITSTATUS(status) : 1).add_stderr(stderrText);

    json response;
    try
    {
      response = json::parse(stdoutText);
    }
    catch (const std::exception &ex)
    {
      return NoteResult::failure("extension returned invalid JSON", 1).add_error(ex.what()).add_stderr(stderrText);
    }
    const std::string protocol = response.value("protocol", "");
    if (!protocol.empty() && protocol != "vix-note-extension-1")
      return NoteResult::failure("extension returned invalid protocol", 1).add_error("invalid protocol");
    if (!response.contains("ok") || !response["ok"].is_boolean())
      return NoteResult::failure("extension response missing boolean ok", 1).add_error("missing ok");

    if (!response["ok"].get<bool>())
    {
      const auto err = response.value("error", json::object());
      return NoteResult::failure(err.value("message", "extension failed"), 1).add_error(err.value("message", "extension failed"));
    }

    NoteResult result = NoteResult::success("extension cell executed");
    if (response.contains("stdout") && response["stdout"].is_string())
    {
      const std::string data = response["stdout"].get<std::string>();
      if (!data.empty())
        result.add_stdout(data);
    }
    if (response.contains("stderr") && response["stderr"].is_string())
    {
      const std::string data = response["stderr"].get<std::string>();
      if (!data.empty())
        result.add_stderr(data);
    }
    if (response.contains("outputs") && response["outputs"].is_array())
    {
      for (const auto &out : response["outputs"])
      {
        const std::string mime = out.value("mime", "text/plain");
        const std::string data = out.value("data", "");
        if (mime == "text/plain")
          result.add_stdout(data);
        else if (mime == "application/json" || mime == "text/html" || mime == "image/svg+xml")
        {
          NoteOutput noteOutput = NoteOutput::text(data);
          noteOutput.mime = mime;
          result.add_output(std::move(noteOutput));
        }
        else
          result.add_error("unsupported output MIME: " + mime);
      }
    }
    if (!stderrText.empty())
      result.add_stderr(stderrText);
    return result;
#endif
  }
}
