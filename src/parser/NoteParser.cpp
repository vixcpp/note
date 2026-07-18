/**
 *
 *  @file NoteParser.cpp
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

#include <vix/note/parser/NoteParser.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vix::note
{
  namespace
  {
    struct CellMetadata
    {
      std::string id;
      std::string title;
      NoteCellKind kind = NoteCellKind::Unknown;
      std::string typeId;
    };

    std::string trim_copy(std::string_view value)
    {
      std::size_t begin = 0;

      while (begin < value.size() &&
             (value[begin] == ' ' ||
              value[begin] == '\t' ||
              value[begin] == '\n' ||
              value[begin] == '\r'))
      {
        ++begin;
      }

      std::size_t end = value.size();

      while (end > begin &&
             (value[end - 1] == ' ' ||
              value[end - 1] == '\t' ||
              value[end - 1] == '\n' ||
              value[end - 1] == '\r'))
      {
        --end;
      }

      return std::string(value.substr(begin, end - begin));
    }

    bool starts_with(std::string_view value, std::string_view prefix)
    {
      return value.size() >= prefix.size() &&
             value.substr(0, prefix.size()) == prefix;
    }

    bool ends_with(std::string_view value, std::string_view suffix)
    {
      return value.size() >= suffix.size() &&
             value.substr(value.size() - suffix.size()) == suffix;
    }

    std::string lower_copy(std::string value)
    {
      for (char &c : value)
      {
        if (c >= 'A' && c <= 'Z')
        {
          c = static_cast<char>(c - 'A' + 'a');
        }
      }

      return value;
    }

    bool is_fence_line(std::string_view line, std::string &language)
    {
      const std::string trimmed = trim_copy(line);

      if (!starts_with(trimmed, "```"))
      {
        return false;
      }

      language = trim_copy(std::string_view(trimmed).substr(3));
      return true;
    }

    std::string language_to_cell_type_id(std::string language)
    {
      return normalize_cell_type_id(lower_copy(trim_copy(language)));
    }

    std::string extract_quoted_attribute(
        std::string_view value,
        std::string_view name)
    {
      const std::string text(value);

      const std::string pattern =
          std::string(name) + "=\"";

      const std::size_t begin =
          text.find(pattern);

      if (begin == std::string::npos)
      {
        return {};
      }

      const std::size_t valueBegin =
          begin + pattern.size();

      const std::size_t valueEnd =
          text.find('"', valueBegin);

      if (valueEnd == std::string::npos)
      {
        return {};
      }

      return text.substr(
          valueBegin,
          valueEnd - valueBegin);
    }

    bool parse_cell_metadata_comment(
        std::string_view line,
        CellMetadata &metadata)
    {
      metadata = CellMetadata{};

      const std::string trimmed =
          trim_copy(line);

      if (!starts_with(trimmed, "<!--") ||
          !ends_with(trimmed, "-->"))
      {
        return false;
      }

      if (trimmed.find("vixnote:cell") == std::string::npos)
      {
        return false;
      }

      metadata.id =
          extract_quoted_attribute(trimmed, "id");

      metadata.title =
          extract_quoted_attribute(trimmed, "title");

      const std::string kind =
          extract_quoted_attribute(trimmed, "kind");

      if (!kind.empty())
      {
        metadata.typeId = normalize_cell_type_id(kind);
        metadata.kind = builtin_kind_from_type_id(metadata.typeId);
      }

      return true;
    }

    std::string make_cell_id(std::size_t index)
    {
      return "cell-" + std::to_string(index);
    }

    bool contains_non_whitespace(std::string_view value)
    {
      for (char c : value)
      {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
        {
          return true;
        }
      }

      return false;
    }

    void append_line(std::string &target, const std::string &line)
    {
      target += line;
      target += '\n';
    }

    void remove_final_newline(std::string &value)
    {
      while (!value.empty() &&
             (value.back() == '\n' ||
              value.back() == '\r' ||
              value.back() == ' ' ||
              value.back() == '\t'))
      {
        value.pop_back();
      }
    }

    void trim_outer_whitespace(std::string &value)
    {
      while (!value.empty() &&
             (value.front() == '\n' ||
              value.front() == '\r' ||
              value.front() == ' ' ||
              value.front() == '\t'))
      {
        value.erase(value.begin());
      }

      while (!value.empty() &&
             (value.back() == '\n' ||
              value.back() == '\r' ||
              value.back() == ' ' ||
              value.back() == '\t'))
      {
        value.pop_back();
      }
    }

    std::string infer_title_from_markdown(std::string_view markdown)
    {
      std::istringstream in{std::string(markdown)};
      std::string line;

      while (std::getline(in, line))
      {
        const std::string trimmed = trim_copy(line);

        if (!starts_with(trimmed, "# "))
        {
          continue;
        }

        return trim_copy(std::string_view(trimmed).substr(2));
      }

      return {};
    }

    void assign_id_if_needed(
        NoteCell &cell,
        const NoteParseOptions &options,
        std::size_t cellIndex)
    {
      if (!options.assignCellIds || !cell.id().empty())
      {
        return;
      }

      cell.set_id(make_cell_id(cellIndex));
    }

    void apply_pending_metadata(
        NoteCell &cell,
        std::optional<CellMetadata> &pendingMetadata)
    {
      if (!pendingMetadata)
      {
        return;
      }

      if (!pendingMetadata->id.empty())
      {
        cell.set_id(pendingMetadata->id);
      }

      if (!pendingMetadata->title.empty())
      {
        cell.set_title(pendingMetadata->title);
      }

      if (!pendingMetadata->typeId.empty())
      {
        cell.set_type_id(pendingMetadata->typeId);
      }

      pendingMetadata.reset();
    }

    void append_markdown_cell(
        NoteDocument &document,
        std::string &markdown,
        const NoteParseOptions &options,
        std::optional<CellMetadata> &pendingMetadata)
    {
      if (!contains_non_whitespace(markdown))
      {
        markdown.clear();
        return;
      }

      trim_outer_whitespace(markdown);

      NoteCell cell = NoteCell::markdown(std::move(markdown));

      apply_pending_metadata(cell, pendingMetadata);
      assign_id_if_needed(cell, options, document.cell_count() + 1);

      document.add_cell(std::move(cell));
      markdown.clear();
    }

    void append_code_cell(
        NoteDocument &document,
        std::string typeId,
        std::string source,
        const NoteParseOptions &options,
        std::optional<CellMetadata> &pendingMetadata)
    {
      remove_final_newline(source);

      NoteCell cell({}, std::move(typeId), std::move(source));

      apply_pending_metadata(cell, pendingMetadata);
      assign_id_if_needed(cell, options, document.cell_count() + 1);

      document.add_cell(std::move(cell));
    }
  }

  bool NoteParseResult::has_diagnostics() const noexcept
  {
    return !diagnostics.empty();
  }

  NoteParser::NoteParser() = default;

  NoteParser::NoteParser(NoteParseOptions options)
      : options_(options)
  {
  }

  const NoteParseOptions &NoteParser::options() const noexcept
  {
    return options_;
  }

  void NoteParser::set_options(NoteParseOptions options) noexcept
  {
    options_ = options;
  }

  NoteParseResult NoteParser::parse(std::string_view source) const
  {
    NoteParseResult result;
    NoteDocument document;

    std::string markdownBuffer;
    std::string codeBuffer;
    std::string fenceLanguage;

    bool inFence = false;
    std::string fenceTypeId;
    std::size_t fenceStartLine = 0;
    std::size_t lineNumber = 0;

    std::optional<CellMetadata> pendingMetadata;

    std::istringstream in{std::string(source)};
    std::string line;

    while (std::getline(in, line))
    {
      ++lineNumber;

      std::string language;
      CellMetadata metadata;

      if (!inFence &&
          options_.readCellMetadata &&
          parse_cell_metadata_comment(line, metadata))
      {
        append_markdown_cell(
            document,
            markdownBuffer,
            options_,
            pendingMetadata);

        pendingMetadata = std::move(metadata);
        continue;
      }

      if (!inFence && is_fence_line(line, language))
      {
        append_markdown_cell(
            document,
            markdownBuffer,
            options_,
            pendingMetadata);

        inFence = true;
        fenceLanguage = language;
        fenceTypeId = language_to_cell_type_id(language);
        fenceStartLine = lineNumber;
        codeBuffer.clear();

        continue;
      }

      if (inFence && is_fence_line(line, language))
      {
        if (fenceTypeId.empty() || fenceTypeId == "markdown")
        {
          std::string fencedMarkdown;

          fencedMarkdown += "```";
          fencedMarkdown += fenceLanguage;
          fencedMarkdown += '\n';
          fencedMarkdown += codeBuffer;
          fencedMarkdown += "```";

          append_line(markdownBuffer, fencedMarkdown);
        }
        else
        {
          append_code_cell(
              document,
              fenceTypeId,
              std::move(codeBuffer),
              options_,
              pendingMetadata);

          codeBuffer.clear();
        }

        inFence = false;
        fenceLanguage.clear();
        fenceTypeId.clear();
        fenceStartLine = 0;

        continue;
      }

      if (inFence)
      {
        append_line(codeBuffer, line);
      }
      else
      {
        append_line(markdownBuffer, line);
      }
    }

    if (inFence)
    {
      result.ok = false;
      result.document = std::move(document);
      result.error =
          "unterminated fenced cell starting at line " +
          std::to_string(fenceStartLine);

      return result;
    }

    append_markdown_cell(
        document,
        markdownBuffer,
        options_,
        pendingMetadata);

    if (options_.inferTitle && !document.has_title())
    {
      for (const auto &cell : document.cells())
      {
        if (cell.kind() != NoteCellKind::Markdown)
        {
          continue;
        }

        const std::string title =
            infer_title_from_markdown(cell.source());

        if (!title.empty())
        {
          document.set_title(title);
          break;
        }
      }
    }

    result.ok = true;
    result.document = std::move(document);
    return result;
  }

  NoteDocument NoteParser::parse_or_throw(std::string_view source) const
  {
    NoteParseResult result = parse(source);

    if (!result.ok)
    {
      throw NoteError(
          NoteErrorCode::Parse,
          result.error.empty() ? "failed to parse note document" : result.error);
    }

    return std::move(result.document);
  }

  NoteParseResult parse_note(std::string_view source)
  {
    return NoteParser().parse(source);
  }

  NoteDocument parse_note_or_throw(std::string_view source)
  {
    return NoteParser().parse_or_throw(source);
  }
}
