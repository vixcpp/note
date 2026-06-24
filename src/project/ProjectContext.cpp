/**
 *
 *  @file ProjectContext.cpp
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

#include <vix/note/project/ProjectContext.hpp>

#include <filesystem>

namespace vix::note
{
  bool ProjectContext::has_project_root() const noexcept
  {
    return !projectRoot.empty();
  }

  bool ProjectContext::has_manifest() const noexcept
  {
    return !manifestPath.empty();
  }

  bool ProjectContext::has_deps() const noexcept
  {
    return !depsDirectory.empty();
  }

  bool ProjectContext::has_include_paths() const noexcept
  {
    return !includePaths.empty();
  }

  std::filesystem::path ProjectContext::effective_working_directory() const
  {
    if (!workingDirectory.empty())
    {
      return workingDirectory;
    }

    return projectRoot;
  }
}
