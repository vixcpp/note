/**
 *
 *  @file NoteStore.cpp
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

#include <vix/note/storage/NoteStore.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace vix::note
{
  namespace
  {
    bool read_text_file(
        const std::filesystem::path &path,
        std::string &out,
        std::string &err)
    {
      out.clear();
      err.clear();

      std::ifstream in(path, std::ios::binary);

      if (!in.is_open())
      {
        err = "cannot open note file: " + path.string();
        return false;
      }

      std::ostringstream buffer;
      buffer << in.rdbuf();

      if (in.bad())
      {
        err = "cannot read note file: " + path.string();
        return false;
      }

      out = buffer.str();
      return true;
    }

    bool ensure_parent_directory(
        const std::filesystem::path &path,
        std::string &err)
    {
      err.clear();

      const std::filesystem::path parent = path.parent_path();

      if (parent.empty())
      {
        return true;
      }

      std::error_code ec;
      std::filesystem::create_directories(parent, ec);

      if (ec)
      {
        err = "cannot create directory: " + parent.string() + ": " + ec.message();
        return false;
      }

      return true;
    }

    bool write_text_file_direct(
        const std::filesystem::path &path,
        const std::string &content,
        std::string &err)
    {
      err.clear();

      std::ofstream out(path, std::ios::binary | std::ios::trunc);

      if (!out.is_open())
      {
        err = "cannot write note file: " + path.string();
        return false;
      }

      out << content;

      if (!out.good())
      {
        err = "cannot write note file: " + path.string();
        return false;
      }

      return true;
    }

    bool write_text_file_atomic(
        const std::filesystem::path &path,
        const std::string &content,
        std::string &err)
    {
      err.clear();

      const std::filesystem::path tmp =
          path.parent_path() /
          (path.filename().string() + ".tmp");

      if (!write_text_file_direct(tmp, content, err))
      {
        return false;
      }

      std::error_code ec;

      if (std::filesystem::exists(path, ec))
      {
        std::filesystem::remove(path, ec);

        if (ec)
        {
          std::filesystem::remove(tmp);
          err = "cannot replace note file: " + path.string() + ": " + ec.message();
          return false;
        }
      }

      std::filesystem::rename(tmp, path, ec);

      if (ec)
      {
        std::filesystem::remove(tmp);
        err = "cannot replace note file: " + path.string() + ": " + ec.message();
        return false;
      }

      return true;
    }

    void append_fenced_cell(
        std::string &out,
        const std::string &language,
        const std::string &source)
    {
      out += "```";
      out += language;
      out += '\n';
      out += source;

      if (out.empty() || out.back() != '\n')
      {
        out += '\n';
      }

      out += "```";
    }

    void append_cell_separator(std::string &out)
    {
      if (!out.empty())
      {
        out += "\n\n";
      }
    }
  }

  bool NoteLoadResult::has_error() const noexcept
  {
    return !error.empty();
  }

  NoteStore::NoteStore() = default;

  NoteStore::NoteStore(NoteStoreOptions options)
      : options_(options)
  {
  }

  const NoteStoreOptions &NoteStore::options() const noexcept
  {
    return options_;
  }

  void NoteStore::set_options(NoteStoreOptions options) noexcept
  {
    options_ = options;
  }

  NoteLoadResult NoteStore::load(const std::filesystem::path &path) const
  {
    NoteLoadResult result;

    std::string source;
    std::string err;

    if (!read_text_file(path, source, err))
    {
      result.ok = false;
      result.error = err;
      return result;
    }

    NoteParser parser(options_.parseOptions);
    NoteParseResult parsed = parser.parse(source);

    if (!parsed.ok)
    {
      result.ok = false;
      result.document = std::move(parsed.document);
      result.error = parsed.error.empty()
                         ? "cannot parse note file: " + path.string()
                         : parsed.error;
      return result;
    }

    parsed.document.set_path(path.string());

    result.ok = true;
    result.document = std::move(parsed.document);
    return result;
  }

  NoteDocument NoteStore::load_or_throw(const std::filesystem::path &path) const
  {
    NoteLoadResult result = load(path);

    if (!result.ok)
    {
      throw NoteError(
          NoteErrorCode::Read,
          result.error.empty() ? "failed to load note document" : result.error);
    }

    return std::move(result.document);
  }

  NoteResult NoteStore::save(const NoteDocument &document) const
  {
    if (document.path().empty())
    {
      return NoteResult::failure("note document has no path", 1)
          .add_error("note document has no path");
    }

    return save(document, document.path());
  }

  NoteResult NoteStore::save(
      const NoteDocument &document,
      const std::filesystem::path &path) const
  {
    if (path.empty())
    {
      return NoteResult::failure("empty note path", 1)
          .add_error("empty note path");
    }

    std::string err;

    if (options_.createParentDirectories &&
        !ensure_parent_directory(path, err))
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    const std::string content = serialize(document);

    const bool ok =
        options_.atomicWrite
            ? write_text_file_atomic(path, content, err)
            : write_text_file_direct(path, content, err);

    if (!ok)
    {
      return NoteResult::failure(err, 1).add_error(err);
    }

    return NoteResult::success("note saved").add_text(path.string());
  }

  void NoteStore::save_or_throw(const NoteDocument &document) const
  {
    NoteResult result = save(document);

    if (!result.ok())
    {
      throw NoteError(
          NoteErrorCode::Write,
          result.message().empty() ? "failed to save note document" : result.message());
    }
  }

  void NoteStore::save_or_throw(
      const NoteDocument &document,
      const std::filesystem::path &path) const
  {
    NoteResult result = save(document, path);

    if (!result.ok())
    {
      throw NoteError(
          NoteErrorCode::Write,
          result.message().empty() ? "failed to save note document" : result.message());
    }
  }

  std::string NoteStore::serialize(const NoteDocument &document) const
  {
    std::string out;

    for (const NoteCell &cell : document.cells())
    {
      append_cell_separator(out);

      switch (cell.kind())
      {
      case NoteCellKind::Markdown:
        out += cell.source();
        break;

      case NoteCellKind::Reply:
        append_fenced_cell(out, "reply", cell.source());
        break;

      case NoteCellKind::Cpp:
        append_fenced_cell(out, "cpp", cell.source());
        break;

      case NoteCellKind::Html:
        append_fenced_cell(out, "html", cell.source());
        break;

      case NoteCellKind::Unknown:
      default:
        out += cell.source();
        break;
      }
    }

    if (!out.empty() && out.back() != '\n')
    {
      out += '\n';
    }

    return out;
  }

  NoteLoadResult load_note(const std::filesystem::path &path)
  {
    return NoteStore().load(path);
  }

  NoteDocument load_note_or_throw(const std::filesystem::path &path)
  {
    return NoteStore().load_or_throw(path);
  }

  NoteResult save_note(
      const NoteDocument &document,
      const std::filesystem::path &path)
  {
    return NoteStore().save(document, path);
  }

  std::string serialize_note(const NoteDocument &document)
  {
    return NoteStore().serialize(document);
  }
}
