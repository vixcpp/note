/**
 *
 *  @file NoteExtensionRegistry.hpp
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
#ifndef VIX_NOTE_EXTENSIONS_NOTE_EXTENSION_REGISTRY_HPP
#define VIX_NOTE_EXTENSIONS_NOTE_EXTENSION_REGISTRY_HPP

#include <vix/note/core/NoteCell.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/project/ProjectContext.hpp>
#include <vix/note/runtime/CppCellRunner.hpp>
#include <vix/note/runtime/ReplyCellRunner.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vix::note
{
  enum class NoteExtensionSource
  {
    Builtin,
    Global,
    Project
  };

  struct NoteExecutionContext
  {
    std::string documentId;
    std::filesystem::path documentPath;
    std::string cellId;
    std::string cellType;
    int executionCount{0};
    ProjectContext projectContext;
    std::chrono::milliseconds timeout{10000};
    std::size_t outputSizeLimit{1024 * 1024};
  };

  class NoteCellRunner
  {
  public:
    virtual ~NoteCellRunner() = default;
    virtual NoteResult run(const NoteCell &cell, NoteExecutionContext &context) = 0;
  };

  struct NoteCellTypeDescriptor
  {
    std::string id;
    std::string label;
    std::string language;
    std::vector<std::string> aliases;
    bool executable{false};
    bool builtin{false};
    std::string extensionId;
    std::string commentLine;
    std::string commentBlock;
    std::string defaultSource;
    std::string placeholder;
  };

  struct NoteExtensionDescriptor
  {
    std::string id;
    std::string packageId;
    std::string version;
    std::string apiVersion;
    NoteExtensionSource source{NoteExtensionSource::Builtin};
    std::filesystem::path rootPath;
    std::string iconPath;
    std::vector<std::string> capabilities;
    std::vector<std::string> permissions;
    std::vector<NoteCellTypeDescriptor> cellTypes;
    std::string runtimeCommand;
    std::string runtimeProtocol;
    std::string runtimeMode;
    bool available{true};
    bool enabled{true};
    std::vector<std::string> diagnostics;
  };

  class NoteExtensionRegistry
  {
  public:
    using RunnerFactory = std::function<std::unique_ptr<NoteCellRunner>()>;

    bool register_extension(NoteExtensionDescriptor descriptor);
    bool register_cell_type(NoteCellTypeDescriptor descriptor, RunnerFactory factory = {});

    const NoteExtensionDescriptor *find_extension(const std::string &id) const noexcept;
    const NoteCellTypeDescriptor *find_cell_type(const std::string &id) const noexcept;
    const NoteCellTypeDescriptor *find_cell_type_by_alias(const std::string &alias) const noexcept;

    std::vector<NoteExtensionDescriptor> list_extensions() const;
    std::vector<NoteCellTypeDescriptor> list_cell_types() const;

    std::unique_ptr<NoteCellRunner> create_runner_for(const std::string &cellTypeId) const;
    const std::vector<std::string> &diagnostics() const noexcept;

  private:
    std::vector<NoteExtensionDescriptor> extensions_;
    std::vector<NoteCellTypeDescriptor> cellTypes_;
    std::vector<std::pair<std::string, RunnerFactory>> factories_;
    std::vector<std::string> diagnostics_;
  };

  class NoteExtensionManager
  {
  public:
    NoteExtensionRegistry &registry() noexcept;
    const NoteExtensionRegistry &registry() const noexcept;
    void register_builtins(const CppCellRunnerOptions &cppOptions = {}, const ReplyCellRunnerOptions &replyOptions = {});
    void discover_global();
    void discover_project(const ProjectContext &context);
    void reload(const ProjectContext &context = {}, bool includeExternal = true);
    bool set_extension_enabled(const std::string &packageId, bool enabled, std::string &error);
    bool is_extension_disabled(const std::string &packageId) const;

  private:
    CppCellRunnerOptions cppOptions_;
    ReplyCellRunnerOptions replyOptions_;
    ProjectContext projectContext_;
    bool includeExternal_{true};
    NoteExtensionRegistry registry_;
  };

  class ExternalProcessCellRunner final : public NoteCellRunner
  {
  public:
    explicit ExternalProcessCellRunner(NoteExtensionDescriptor descriptor);
    NoteResult run(const NoteCell &cell, NoteExecutionContext &context) override;

  private:
    NoteExtensionDescriptor descriptor_;
  };
}

#endif
