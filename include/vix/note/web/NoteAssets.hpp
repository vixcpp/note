/**
 *
 *  @file NoteAssets.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Static web assets used by the Vix Note local UI.
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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vix::note
{
  /**
   * @brief Represents one in-memory web asset.
   *
   * NoteAsset is used by the local note UI server to serve small built-in
   * files such as the default HTML, CSS, and JavaScript assets.
   */
  struct NoteAsset
  {
    /**
     * @brief Public asset path.
     *
     * Example: `/`, `/assets/note.css`, or `/assets/note.js`.
     */
    std::string path;

    /**
     * @brief HTTP content type.
     *
     * Example: `text/html; charset=utf-8`.
     */
    std::string contentType;

    /**
     * @brief Asset content.
     */
    std::string content;

    /**
     * @brief Checks whether the asset has no content.
     *
     * @return True when the content is empty.
     */
    bool empty() const noexcept;
  };

  /**
   * @brief Registry for built-in Vix Note web assets.
   *
   * The first version keeps assets embedded in C++ so the UI can start without
   * requiring an external asset pipeline. Later versions can replace this with
   * generated assets or a packaged resource system.
   */
  class NoteAssets
  {
  public:
    /**
     * @brief Creates an asset registry with default assets.
     */
    NoteAssets();

    /**
     * @brief Returns all registered assets.
     *
     * @return Read-only list of assets.
     */
    const std::vector<NoteAsset> &all() const noexcept;

    /**
     * @brief Returns the number of registered assets.
     *
     * @return Asset count.
     */
    std::size_t size() const noexcept;

    /**
     * @brief Checks whether no assets are registered.
     *
     * @return True when the asset list is empty.
     */
    bool empty() const noexcept;

    /**
     * @brief Finds an asset by path.
     *
     * @param path Requested asset path.
     * @return Matching asset, or std::nullopt when not found.
     */
    std::optional<NoteAsset> find(std::string_view path) const;

    /**
     * @brief Checks whether an asset exists.
     *
     * @param path Requested asset path.
     * @return True when the asset exists.
     */
    bool contains(std::string_view path) const;

    /**
     * @brief Registers or replaces an asset.
     *
     * @param asset Asset to add.
     */
    void add_or_replace(NoteAsset asset);

    /**
     * @brief Removes an asset by path.
     *
     * @param path Asset path to remove.
     * @return True when an asset was removed.
     */
    bool remove(std::string_view path);

    /**
     * @brief Removes all registered assets.
     */
    void clear();

    /**
     * @brief Creates the default asset list.
     *
     * @return Default assets.
     */
    static std::vector<NoteAsset> defaults();

    /**
     * @brief Returns the default HTML page.
     *
     * @return HTML document.
     */
    static std::string default_index_html();

    /**
     * @brief Returns the default CSS asset.
     *
     * @return CSS content.
     */
    static std::string default_css();

    /**
     * @brief Returns the default JavaScript asset.
     *
     * @return JavaScript content.
     */
    static std::string default_js();

  private:
    /**
     * @brief Registered assets.
     */
    std::vector<NoteAsset> assets_;
  };

  /**
   * @brief Returns a content type from an asset path.
   *
   * @param path Asset path.
   * @return Content type string.
   */
  std::string note_asset_content_type(std::string_view path);

  /**
   * @brief Normalizes a requested web asset path.
   *
   * Empty paths and `/` are normalized to `/`.
   *
   * @param path Raw path.
   * @return Normalized path.
   */
  std::string normalize_note_asset_path(std::string_view path);
}

#endif // VIX_NOTE_WEB_NOTE_ASSETS_HPP
