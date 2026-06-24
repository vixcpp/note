/**
 *
 *  @file ProjectContext.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Project context model for project-aware Vix Note execution.
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

#ifndef VIX_NOTE_PROJECT_PROJECT_CONTEXT_HPP
#define VIX_NOTE_PROJECT_PROJECT_CONTEXT_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace vix::note
{
  /**
   * @brief Describes the project context attached to a Vix Note document.
   *
   * ProjectContext is intentionally separate from NoteDocument. The document
   * stores note content, while ProjectContext stores execution environment
   * details such as project root, working directory, manifest path, dependency
   * include paths, and project name.
   */
  struct ProjectContext
  {
    /**
     * @brief True when a project root was detected.
     */
    bool enabled = false;

    /**
     * @brief Human-readable project name.
     */
    std::string projectName;

    /**
     * @brief Source note file path.
     */
    std::filesystem::path notePath;

    /**
     * @brief Detected project root.
     */
    std::filesystem::path projectRoot;

    /**
     * @brief Working directory used to run cells.
     */
    std::filesystem::path workingDirectory;

    /**
     * @brief Detected Vix manifest path when available.
     */
    std::filesystem::path manifestPath;

    /**
     * @brief Detected .vix/deps directory when available.
     */
    std::filesystem::path depsDirectory;

    /**
     * @brief Include paths passed to C++ cell execution.
     */
    std::vector<std::filesystem::path> includePaths;

    /**
     * @brief Environment entries in KEY=VALUE form.
     */
    std::vector<std::string> environment;

    /**
     * @brief Checks whether a project root is available.
     *
     * @return True when projectRoot is not empty.
     */
    bool has_project_root() const noexcept;

    /**
     * @brief Checks whether a manifest was detected.
     *
     * @return True when manifestPath is not empty.
     */
    bool has_manifest() const noexcept;

    /**
     * @brief Checks whether a dependency directory was detected.
     *
     * @return True when depsDirectory is not empty.
     */
    bool has_deps() const noexcept;

    /**
     * @brief Checks whether include paths are available.
     *
     * @return True when includePaths is not empty.
     */
    bool has_include_paths() const noexcept;

    /**
     * @brief Returns the effective working directory.
     *
     * @return workingDirectory when set, otherwise projectRoot.
     */
    std::filesystem::path effective_working_directory() const;
  };
}

#endif // VIX_NOTE_PROJECT_PROJECT_CONTEXT_HPP
