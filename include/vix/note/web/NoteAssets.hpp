/**
 *
 *  @file NoteAssets.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Filesystem-backed static web assets used by the Vix Note local UI.
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

#ifndef VIX_NOTE_WEB_NOTE_ASSETS_HPP
#define VIX_NOTE_WEB_NOTE_ASSETS_HPP

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vix::note
{
  struct NoteAsset
  {
    std::string path;
    std::string contentType;
    std::string content;

    bool empty() const noexcept;
  };

  struct NoteAssetDirectoryOptions
  {
    bool clearBeforeLoad = false;
    bool keepEmbeddedFallback = false;
  };

  struct NoteAssetResolveOptions
  {
    std::filesystem::path customDirectory;
    bool useEnvironmentDirectory = true;
    bool useBuildDirectory = true;
    bool useExecutableRelativeDirectory = true;
    bool useInstalledDirectory = true;
    bool useGlobalDirectory = true;
    bool useSourceDirectory = true;
    bool keepEmbeddedFallback = false;
  };

  struct NoteAssetResolveResult
  {
    std::filesystem::path directory;
    std::vector<std::filesystem::path> checked;
    std::string error;

    bool found() const noexcept;
  };

  class NoteAssets
  {
  public:
    NoteAssets();
    explicit NoteAssets(std::filesystem::path root);
    explicit NoteAssets(std::vector<NoteAsset> assets);

    const std::filesystem::path &root() const noexcept;
    const std::vector<NoteAsset> &all() const noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;
    bool valid() const noexcept;
    const std::string &error() const noexcept;

    std::optional<NoteAsset> find(std::string_view path) const;
    bool contains(std::string_view path) const;

    void add_or_replace(NoteAsset asset);
    bool load_from_directory(
        const std::filesystem::path &directory,
        NoteAssetDirectoryOptions options,
        std::string &error);
    bool load_from_directory(
        const std::filesystem::path &directory,
        std::string &error);
    bool remove(std::string_view path);
    void clear();

    std::optional<std::filesystem::path> resolve(
        std::string_view relativePath) const;

    static std::vector<NoteAsset> defaults();
    static std::vector<NoteAsset> from_directory(
        const std::filesystem::path &directory,
        std::string &error);
    static std::string default_index_html();
    static std::string default_css();
    static std::string default_js();
    static std::string content_type_for(const std::filesystem::path &path);

  private:
    std::filesystem::path root_;
    std::filesystem::path canonicalRoot_;
    std::vector<NoteAsset> overrides_;
    std::string error_;

    bool set_root(std::filesystem::path root, std::string *error);
  };

  std::filesystem::path note_installed_asset_directory();
  std::filesystem::path note_build_asset_directory();
  std::filesystem::path note_source_asset_directory();
  std::optional<std::filesystem::path> note_current_executable_path();

  std::vector<std::filesystem::path> note_asset_search_paths(
      const NoteAssetResolveOptions &options = {});

  NoteAssetResolveResult resolve_note_asset_directory(
      const NoteAssetResolveOptions &options = {});

  bool load_best_available_note_assets(
      NoteAssets &assets,
      const NoteAssetResolveOptions &options,
      std::string &error);

  bool read_note_asset_file(
      const std::filesystem::path &path,
      std::string &out,
      std::string &err);

  bool note_asset_directory_is_valid(
      const std::filesystem::path &directory,
      std::string *error = nullptr);

  std::string note_asset_public_path(const std::filesystem::path &path);
  std::string note_asset_content_type(std::string_view path);
  std::string normalize_note_asset_path(std::string_view path);
}

#endif // VIX_NOTE_WEB_NOTE_ASSETS_HPP
