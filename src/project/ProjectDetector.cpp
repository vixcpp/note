/**
 *
 *  @file ProjectDetector.cpp
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

#include <vix/note/project/ProjectDetector.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace vix::note
{
  namespace
  {
    bool exists_path(const fs::path &path)
    {
      std::error_code ec;
      return fs::exists(path, ec) && !ec;
    }

    bool is_directory_path(const fs::path &path)
    {
      std::error_code ec;
      return fs::is_directory(path, ec) && !ec;
    }

    bool is_regular_file_path(const fs::path &path)
    {
      std::error_code ec;
      return fs::is_regular_file(path, ec) && !ec;
    }

    fs::path absolute_normal_path(const fs::path &path)
    {
      std::error_code ec;
      fs::path absolute = fs::absolute(path, ec);

      if (ec)
      {
        return path.lexically_normal();
      }

      return absolute.lexically_normal();
    }

    std::string trim_copy(std::string value)
    {
      while (!value.empty() &&
             (value.front() == ' ' ||
              value.front() == '\t' ||
              value.front() == '\n' ||
              value.front() == '\r'))
      {
        value.erase(value.begin());
      }

      while (!value.empty() &&
             (value.back() == ' ' ||
              value.back() == '\t' ||
              value.back() == '\n' ||
              value.back() == '\r'))
      {
        value.pop_back();
      }

      return value;
    }

    std::string read_vix_json_include_field(const fs::path &manifest)
    {
      std::ifstream in(manifest, std::ios::binary);

      if (!in.is_open())
      {
        return {};
      }

      std::ostringstream buffer;
      buffer << in.rdbuf();

      const std::string text = buffer.str();
      const std::string key = "\"include\"";

      const std::size_t pos = text.find(key);

      if (pos == std::string::npos)
      {
        return {};
      }

      const std::size_t colon = text.find(':', pos + key.size());

      if (colon == std::string::npos)
      {
        return {};
      }

      const std::size_t quoteBegin = text.find('"', colon + 1);

      if (quoteBegin == std::string::npos)
      {
        return {};
      }

      const std::size_t quoteEnd = text.find('"', quoteBegin + 1);

      if (quoteEnd == std::string::npos)
      {
        return {};
      }

      return trim_copy(
          text.substr(
              quoteBegin + 1,
              quoteEnd - quoteBegin - 1));
    }

    void add_unique_include_path(
        ProjectContext &context,
        const fs::path &path)
    {
      if (!is_directory_path(path))
      {
        return;
      }

      const fs::path normalized =
          absolute_normal_path(path);

      for (const auto &existing : context.includePaths)
      {
        if (absolute_normal_path(existing) == normalized)
        {
          return;
        }
      }

      context.includePaths.push_back(normalized);
    }

    void add_include_path_if_exists(
        ProjectContext &context,
        const fs::path &path)
    {
      add_unique_include_path(context, path);
    }

    fs::path parent_or_self_directory(const fs::path &path)
    {
      if (path.empty())
      {
        return absolute_normal_path(fs::current_path());
      }

      const fs::path absolute = absolute_normal_path(path);

      if (is_regular_file_path(absolute))
      {
        return absolute.parent_path();
      }

      if (is_directory_path(absolute))
      {
        return absolute;
      }

      if (absolute.has_extension())
      {
        return absolute.parent_path();
      }

      return absolute;
    }

    bool is_project_root(const fs::path &dir)
    {
      return exists_path(dir / "vix.app") ||
             exists_path(dir / "vix.json") ||
             exists_path(dir / "CMakeLists.txt") ||
             exists_path(dir / ".vix") ||
             exists_path(dir / ".git");
    }

    fs::path find_project_root(
        fs::path start,
        bool searchParents)
    {
      start = absolute_normal_path(start);

      if (!searchParents)
      {
        return is_project_root(start) ? start : fs::path{};
      }

      fs::path current = start;

      while (!current.empty())
      {
        if (is_project_root(current))
        {
          return current;
        }

        const fs::path parent = current.parent_path();

        if (parent == current)
        {
          break;
        }

        current = parent;
      }

      return {};
    }

    std::string project_name_from_root(const fs::path &root)
    {
      if (root.empty())
      {
        return {};
      }

      return root.filename().string();
    }

    fs::path detect_manifest(const fs::path &root)
    {
      if (root.empty())
      {
        return {};
      }

      const fs::path vixJson = root / "vix.json";

      if (is_regular_file_path(vixJson))
      {
        return vixJson;
      }

      const fs::path vixApp = root / "vix.app";

      if (is_regular_file_path(vixApp))
      {
        return vixApp;
      }

      const fs::path manifest = root / ".vix" / "manifest.vix";

      if (is_regular_file_path(manifest))
      {
        return manifest;
      }

      return {};
    }

    void add_deps_include_paths(
        ProjectContext &context,
        const fs::path &depsDirectory)
    {
      if (!is_directory_path(depsDirectory))
      {
        return;
      }

      std::error_code ec;

      for (const auto &entry : fs::directory_iterator(depsDirectory, ec))
      {
        if (ec)
        {
          break;
        }

        std::error_code entryEc;

        if (!entry.is_directory(entryEc) || entryEc)
        {
          continue;
        }

        const fs::path packageRoot = entry.path();

        add_unique_include_path(
            context,
            packageRoot / "include");

        const fs::path packageManifest =
            packageRoot / "vix.json";

        const std::string includeValue =
            read_vix_json_include_field(packageManifest);

        if (!includeValue.empty())
        {
          add_unique_include_path(
              context,
              packageRoot / includeValue);
        }
      }
    }
  }

  ProjectDetector::ProjectDetector() = default;

  ProjectDetector::ProjectDetector(ProjectDetectOptions options)
      : options_(options)
  {
  }

  const ProjectDetectOptions &ProjectDetector::options() const noexcept
  {
    return options_;
  }

  void ProjectDetector::set_options(ProjectDetectOptions options) noexcept
  {
    options_ = options;
  }

  ProjectContext ProjectDetector::detect(const fs::path &notePath) const
  {
    const fs::path start =
        parent_or_self_directory(notePath);

    return detect_from(start, notePath);
  }

  ProjectContext ProjectDetector::detect_from(
      const fs::path &startPath,
      const fs::path &notePath) const
  {
    ProjectContext context;

    context.notePath = absolute_normal_path(notePath);

    const fs::path start =
        parent_or_self_directory(startPath);

    const fs::path root =
        find_project_root(start, options_.searchParents);

    if (root.empty())
    {
      context.enabled = false;
      context.workingDirectory = start;
      return context;
    }

    context.enabled = true;
    context.projectRoot = root;
    context.workingDirectory = root;
    context.projectName = project_name_from_root(root);
    context.manifestPath = detect_manifest(root);

    const fs::path deps = root / ".vix" / "deps";

    if (is_directory_path(deps))
    {
      context.depsDirectory = deps;
    }

    if (options_.includeProjectIncludeDirectory)
    {
      add_include_path_if_exists(context, root / "include");
    }

    if (options_.includeProjectSourceDirectory)
    {
      add_include_path_if_exists(context, root / "src");
    }

    if (options_.detectDeps && context.has_deps())
    {
      add_deps_include_paths(context, context.depsDirectory);
    }

    return context;
  }

  ProjectContext detect_project_context(const fs::path &notePath)
  {
    return ProjectDetector().detect(notePath);
  }
}
