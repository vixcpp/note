/**
 *
 *  @file NoteStore.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Storage helpers for loading and saving Vix Note documents.
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

#ifndef VIX_NOTE_STORAGE_NOTE_STORE_HPP
#define VIX_NOTE_STORAGE_NOTE_STORE_HPP

#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteError.hpp>
#include <vix/note/core/NoteResult.hpp>
#include <vix/note/parser/NoteParser.hpp>

#include <filesystem>
#include <string>

namespace vix::note
{
  /**
   * @brief Options controlling note document storage.
   */
  struct NoteStoreOptions
  {
    /**
     * @brief Parser options used when loading a note document.
     */
    NoteParseOptions parseOptions;

    /**
     * @brief Creates parent directories before saving a document.
     */
    bool createParentDirectories = true;

    /**
     * @brief Writes files through a temporary file before replacing the target.
     */
    bool atomicWrite = true;
  };

  /**
   * @brief Result returned when loading a note document from storage.
   */
  struct NoteLoadResult
  {
    /**
     * @brief True when the document was loaded and parsed successfully.
     */
    bool ok = false;

    /**
     * @brief Loaded document.
     */
    NoteDocument document;

    /**
     * @brief Human-readable error message when loading fails.
     */
    std::string error;

    /**
     * @brief Checks whether the result contains an error message.
     *
     * @return True when an error message is present.
     */
    bool has_error() const noexcept;
  };

  /**
   * @brief Loads and saves Vix Note documents.
   *
   * NoteStore is responsible for file I/O only. Parsing is delegated to
   * NoteParser, while serialization writes a markdown-compatible `.vixnote`
   * document. Runtime outputs are not persisted in the first storage format.
   *
   * For the local UI, NoteStore is used before NoteServer starts: the CLI loads
   * a `.vixnote` file from disk, then passes the resulting NoteDocument to the
   * server route layer.
   */
  class NoteStore
  {
  public:
    /**
     * @brief Creates a store with default options.
     */
    NoteStore();

    /**
     * @brief Creates a store with custom options.
     *
     * @param options Store options.
     */
    explicit NoteStore(NoteStoreOptions options);

    /**
     * @brief Returns the current store options.
     *
     * @return Store options.
     */
    const NoteStoreOptions &options() const noexcept;

    /**
     * @brief Replaces the current store options.
     *
     * @param options New store options.
     */
    void set_options(NoteStoreOptions options) noexcept;

    /**
     * @brief Loads a note document from disk.
     *
     * The loaded document receives the source path through NoteDocument::path()
     * so the UI and API can show where the note came from.
     *
     * @param path Path to the `.vixnote` file.
     * @return Load result.
     */
    NoteLoadResult load(const std::filesystem::path &path) const;

    /**
     * @brief Loads a note document from disk or throws on failure.
     *
     * @param path Path to the `.vixnote` file.
     * @return Loaded document.
     *
     * @throws NoteError when reading or parsing fails.
     */
    NoteDocument load_or_throw(const std::filesystem::path &path) const;

    /**
     * @brief Saves a note document to its stored path.
     *
     * The document must already have a non-empty path.
     *
     * @param document Document to save.
     * @return Save result.
     */
    NoteResult save(const NoteDocument &document) const;

    /**
     * @brief Saves a note document to a specific path.
     *
     * Runtime outputs are intentionally not serialized in the first storage
     * format. Only the readable `.vixnote` source is written.
     *
     * @param document Document to save.
     * @param path     Target file path.
     * @return Save result.
     */
    NoteResult save(
        const NoteDocument &document,
        const std::filesystem::path &path) const;

    /**
     * @brief Saves a note document to its stored path or throws on failure.
     *
     * @param document Document to save.
     *
     * @throws NoteError when writing fails.
     */
    void save_or_throw(const NoteDocument &document) const;

    /**
     * @brief Saves a note document to a specific path or throws on failure.
     *
     * @param document Document to save.
     * @param path     Target file path.
     *
     * @throws NoteError when writing fails.
     */
    void save_or_throw(
        const NoteDocument &document,
        const std::filesystem::path &path) const;

    /**
     * @brief Serializes a note document to `.vixnote` text.
     *
     * The serialized format stays markdown-compatible: markdown cells are
     * written as normal markdown, while executable cells are written as fenced
     * blocks.
     *
     * @param document Document to serialize.
     * @return Serialized document text.
     */
    std::string serialize(const NoteDocument &document) const;

  private:
    /**
     * @brief Store options.
     */
    NoteStoreOptions options_;
  };

  /**
   * @brief Loads a note document using default store options.
   *
   * @param path Path to the `.vixnote` file.
   * @return Load result.
   */
  NoteLoadResult load_note(const std::filesystem::path &path);

  /**
   * @brief Loads a note document using default store options or throws.
   *
   * @param path Path to the `.vixnote` file.
   * @return Loaded document.
   *
   * @throws NoteError when reading or parsing fails.
   */
  NoteDocument load_note_or_throw(const std::filesystem::path &path);

  /**
   * @brief Saves a note document using default store options.
   *
   * @param document Document to save.
   * @param path     Target file path.
   * @return Save result.
   */
  NoteResult save_note(
      const NoteDocument &document,
      const std::filesystem::path &path);

  /**
   * @brief Serializes a note document using default store options.
   *
   * @param document Document to serialize.
   * @return Serialized document text.
   */
  std::string serialize_note(const NoteDocument &document);
}

#endif // VIX_NOTE_STORAGE_NOTE_STORE_HPP
