/**
 *
 *  @file ProjectDetector.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Project detection helpers for project-aware Vix Note execution.
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

#ifndef VIX_NOTE_PROJECT_PROJECT_DETECTOR_HPP
#define VIX_NOTE_PROJECT_PROJECT_DETECTOR_HPP

#include <vix/note/project/ProjectContext.hpp>

#include <filesystem>

namespace vix::note
{
  /**
   * @brief Options controlling project detection.
   */
  struct ProjectDetectOptions
  {
    /**
     * @brief Search parent directories until a project root is found.
     */
    bool searchParents = true;

    /**
     * @brief Detect .vix/deps include directories.
     */
    bool detectDeps = true;

    /**
     * @brief Add project include/ when it exists.
     */
    bool includeProjectIncludeDirectory = true;

    /**
     * @brief Add project src/ when it exists.
     */
    bool includeProjectSourceDirectory = false;
  };

  /**
   * @brief Detects Vix/C++ project context for a note document.
   */
  class ProjectDetector
  {
  public:
    /**
     * @brief Creates a detector with default options.
     */
    ProjectDetector();

    /**
     * @brief Creates a detector with custom options.
     *
     * @param options Detection options.
     */
    explicit ProjectDetector(ProjectDetectOptions options);

    /**
     * @brief Returns current detection options.
     *
     * @return Detection options.
     */
    const ProjectDetectOptions &options() const noexcept;

    /**
     * @brief Replaces current detection options.
     *
     * @param options New options.
     */
    void set_options(ProjectDetectOptions options) noexcept;

    /**
     * @brief Detects project context from a note path.
     *
     * @param notePath Path to a .vixnote file.
     * @return Detected project context.
     */
    ProjectContext detect(const std::filesystem::path &notePath) const;

    /**
     * @brief Detects project context from a starting directory and note path.
     *
     * @param startPath Directory where detection starts.
     * @param notePath  Path to the note file.
     * @return Detected project context.
     */
    ProjectContext detect_from(
        const std::filesystem::path &startPath,
        const std::filesystem::path &notePath) const;

  private:
    /**
     * @brief Detection options.
     */
    ProjectDetectOptions options_;
  };

  /**
   * @brief Detects project context using default options.
   *
   * @param notePath Path to a .vixnote file.
   * @return Detected project context.
   */
  ProjectContext detect_project_context(const std::filesystem::path &notePath);
}

#endif // VIX_NOTE_PROJECT_PROJECT_DETECTOR_HPP
