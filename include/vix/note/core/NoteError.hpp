/**
 *
 *  @file NoteError.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Error types used by the Vix Note core module.
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

#ifndef VIX_NOTE_CORE_NOTE_ERROR_HPP
#define VIX_NOTE_CORE_NOTE_ERROR_HPP

#include <stdexcept>
#include <string>
#include <string_view>

namespace vix::note
{
  /**
   * @brief High-level error category for Vix Note operations.
   *
   * NoteErrorCode is intentionally small. It gives callers a stable way to
   * understand where a failure came from without depending on exact text
   * messages.
   */
  enum class NoteErrorCode
  {
    /**
     * @brief Unknown or uncategorized error.
     */
    Unknown,

    /**
     * @brief A note document or cell could not be parsed.
     */
    Parse,

    /**
     * @brief A note document or asset could not be read from disk.
     */
    Read,

    /**
     * @brief A note document, asset, or output could not be written to disk.
     */
    Write,

    /**
     * @brief A cell failed during execution.
     */
    Runtime,

    /**
     * @brief A requested note feature is not supported yet.
     */
    Unsupported,

    /**
     * @brief A provided argument, path, cell, or document state is invalid.
     */
    Invalid
  };

  /**
   * @brief Exception type thrown by Vix Note operations.
   *
   * NoteError extends std::runtime_error with a lightweight error code.
   * It is used for parser, storage, runtime, export, and UI server failures
   * inside the note module.
   */
  class NoteError : public std::runtime_error
  {
  public:
    /**
     * @brief Creates a NoteError with an unknown error code.
     *
     * @param message Human-readable error message.
     */
    explicit NoteError(std::string message);

    /**
     * @brief Creates a NoteError with a specific error code.
     *
     * @param code    Error category.
     * @param message Human-readable error message.
     */
    NoteError(NoteErrorCode code, std::string message);

    /**
     * @brief Returns the error category.
     *
     * @return Stored error code.
     */
    NoteErrorCode code() const noexcept;

  private:
    /**
     * @brief Stored error category.
     */
    NoteErrorCode code_{NoteErrorCode::Unknown};
  };

  /**
   * @brief Converts a NoteErrorCode to a stable string name.
   *
   * @param code Error code to convert.
   * @return String representation of the error code.
   */
  std::string_view to_string(NoteErrorCode code) noexcept;
}

#endif // VIX_NOTE_CORE_NOTE_ERROR_HPP
