/**
 *
 *  @file NoteResult.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Result and output types produced by Vix Note operations.
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

#ifndef VIX_NOTE_CORE_NOTE_RESULT_HPP
#define VIX_NOTE_CORE_NOTE_RESULT_HPP

#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>

namespace vix::note
{
  /**
   * @brief Execution status for a note operation or cell.
   */
  enum class NoteResultStatus
  {
    /**
     * @brief The operation completed successfully.
     */
    Success,

    /**
     * @brief The operation failed.
     */
    Failure,

    /**
     * @brief The operation was skipped.
     */
    Skipped
  };

  /**
   * @brief Output category produced by a note cell.
   */
  enum class NoteOutputKind
  {
    /**
     * @brief Plain text output.
     */
    Text,

    /**
     * @brief Standard output stream content.
     */
    Stdout,

    /**
     * @brief Standard error stream content.
     */
    Stderr,

    /**
     * @brief HTML output ready for UI rendering.
     */
    Html,

    /**
     * @brief Generic error output.
     */
    Error,

    /**
     * @brief Compiler diagnostic output.
     */
    CompilerError,

    /**
     * @brief Runtime failure output.
     */
    RuntimeError,

    /**
     * @brief Debug metadata output.
     */
    Debug,

    /**
     * @brief Beginner-friendly learning hint.
     */
    Hint,

    /**
     * @brief Raw execution log output.
     */
    RawLog
  };

  /**
   * @brief Represents one output emitted by a note cell.
   *
   * A cell can produce multiple outputs. For example, a C++ cell may emit
   * stdout, stderr, compiler diagnostics, runtime errors, debug metadata,
   * raw logs, and beginner-friendly hints.
   */
  struct NoteOutput
  {
    /**
     * @brief Output kind.
     */
    NoteOutputKind kind{NoteOutputKind::Text};

    /**
     * @brief Output content.
     */
    std::string content;

    std::string mime{"text/plain"};

    nlohmann::json metadata = nlohmann::json::object();

    /**
     * @brief Creates a text output.
     *
     * @param content Output content.
     * @return Created output.
     */
    static NoteOutput text(std::string content);

    /**
     * @brief Creates a stdout output.
     *
     * @param content Output content.
     * @return Created output.
     */
    static NoteOutput stdout_text(std::string content);

    /**
     * @brief Creates a stderr output.
     *
     * @param content Output content.
     * @return Created output.
     */
    static NoteOutput stderr_text(std::string content);

    /**
     * @brief Creates an HTML output.
     *
     * @param content HTML content.
     * @return Created output.
     */
    static NoteOutput html(std::string content);

    /**
     * @brief Creates a generic error output.
     *
     * @param content Error content.
     * @return Created output.
     */
    static NoteOutput error(std::string content);

    /**
     * @brief Creates a compiler error output.
     *
     * @param content Compiler diagnostic content.
     * @return Created output.
     */
    static NoteOutput compiler_error(std::string content);

    /**
     * @brief Creates a runtime error output.
     *
     * @param content Runtime error content.
     * @return Created output.
     */
    static NoteOutput runtime_error(std::string content);

    /**
     * @brief Creates a debug metadata output.
     *
     * @param content Debug metadata content.
     * @return Created output.
     */
    static NoteOutput debug(std::string content);

    /**
     * @brief Creates a beginner-friendly hint output.
     *
     * @param content Hint content.
     * @return Created output.
     */
    static NoteOutput hint(std::string content);

    /**
     * @brief Creates a raw log output.
     *
     * @param content Raw log content.
     * @return Created output.
     */
    static NoteOutput raw_log(std::string content);

    /**
     * @brief Checks whether the output has no content.
     *
     * @return True when the content is empty.
     */
    bool empty() const noexcept;
  };

  /**
   * @brief Result returned by note parsing, execution, storage, or export.
   *
   * NoteResult is intentionally lightweight. It can represent a successful
   * operation, a failure, or a skipped operation, while keeping outputs that
   * can later be rendered by the UI notebook.
   */
  class NoteResult
  {
  public:
    /**
     * @brief Creates a successful result.
     *
     * @param message Optional human-readable message.
     * @return Successful result.
     */
    static NoteResult success(std::string message = {});

    /**
     * @brief Creates a failed result.
     *
     * @param message Human-readable failure message.
     * @param exitCode Optional process or runtime exit code.
     * @return Failed result.
     */
    static NoteResult failure(std::string message, int exitCode = 1);

    /**
     * @brief Creates a skipped result.
     *
     * @param message Optional human-readable message.
     * @return Skipped result.
     */
    static NoteResult skipped(std::string message = {});

    /**
     * @brief Creates a result with a custom status.
     *
     * @param status   Result status.
     * @param message  Optional human-readable message.
     * @param exitCode Optional process or runtime exit code.
     */
    NoteResult(
        NoteResultStatus status = NoteResultStatus::Success,
        std::string message = {},
        int exitCode = 0);

    /**
     * @brief Returns the result status.
     *
     * @return Stored status.
     */
    NoteResultStatus status() const noexcept;

    /**
     * @brief Returns the result exit code.
     *
     * @return Stored exit code.
     */
    int exit_code() const noexcept;

    /**
     * @brief Returns the human-readable message.
     *
     * @return Stored message.
     */
    const std::string &message() const noexcept;

    /**
     * @brief Returns all outputs produced by the operation.
     *
     * @return Read-only output list.
     */
    const std::vector<NoteOutput> &outputs() const noexcept;

    /**
     * @brief Checks whether the result is successful.
     *
     * @return True when the status is Success.
     */
    bool ok() const noexcept;

    /**
     * @brief Checks whether the result failed.
     *
     * @return True when the status is Failure.
     */
    bool failed() const noexcept;

    /**
     * @brief Checks whether the result was skipped.
     *
     * @return True when the status is Skipped.
     */
    bool was_skipped() const noexcept;

    /**
     * @brief Checks whether the result contains outputs.
     *
     * @return True when at least one output is stored.
     */
    bool has_outputs() const noexcept;

    /**
     * @brief Adds a generic output.
     *
     * @param output Output to append.
     * @return Reference to this result.
     */
    NoteResult &add_output(NoteOutput output);

    /**
     * @brief Adds a plain text output.
     *
     * @param content Output content.
     * @return Reference to this result.
     */
    NoteResult &add_text(std::string content);

    /**
     * @brief Adds a stdout output.
     *
     * @param content Output content.
     * @return Reference to this result.
     */
    NoteResult &add_stdout(std::string content);

    /**
     * @brief Adds a stderr output.
     *
     * @param content Output content.
     * @return Reference to this result.
     */
    NoteResult &add_stderr(std::string content);

    /**
     * @brief Adds an HTML output.
     *
     * @param content HTML content.
     * @return Reference to this result.
     */
    NoteResult &add_html(std::string content);

    /**
     * @brief Adds a generic error output.
     *
     * @param content Error content.
     * @return Reference to this result.
     */
    NoteResult &add_error(std::string content);

    /**
     * @brief Adds a compiler error output.
     *
     * @param content Compiler diagnostic content.
     * @return Reference to this result.
     */
    NoteResult &add_compiler_error(std::string content);

    /**
     * @brief Adds a runtime error output.
     *
     * @param content Runtime error content.
     * @return Reference to this result.
     */
    NoteResult &add_runtime_error(std::string content);

    /**
     * @brief Adds a debug metadata output.
     *
     * @param content Debug metadata content.
     * @return Reference to this result.
     */
    NoteResult &add_debug(std::string content);

    /**
     * @brief Adds a beginner-friendly hint output.
     *
     * @param content Hint content.
     * @return Reference to this result.
     */
    NoteResult &add_hint(std::string content);

    /**
     * @brief Adds a raw log output.
     *
     * @param content Raw log content.
     * @return Reference to this result.
     */
    NoteResult &add_raw_log(std::string content);

  private:
    /**
     * @brief Stored result status.
     */
    NoteResultStatus status_{NoteResultStatus::Success};

    /**
     * @brief Optional human-readable message.
     */
    std::string message_;

    /**
     * @brief Process or runtime exit code.
     */
    int exitCode_{0};

    /**
     * @brief Outputs produced by the operation.
     */
    std::vector<NoteOutput> outputs_;
  };

  /**
   * @brief Converts a NoteResultStatus to a stable string name.
   *
   * @param status Status to convert.
   * @return String representation.
   */
  std::string_view to_string(NoteResultStatus status) noexcept;

  /**
   * @brief Converts a NoteOutputKind to a stable string name.
   *
   * @param kind Output kind to convert.
   * @return String representation.
   */
  std::string_view to_string(NoteOutputKind kind) noexcept;
}

#endif // VIX_NOTE_CORE_NOTE_RESULT_HPP
