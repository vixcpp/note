/**
 *
 *  @file NoteKernel.cpp
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

#include <vix/note/runtime/NoteKernel.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vix::note
{
  bool NoteKernelRunResult::has_failures() const noexcept
  {
    if (failed > 0)
    {
      return true;
    }

    for (const NoteResult &result : results)
    {
      if (result.failed())
      {
        return true;
      }
    }

    return false;
  }

  bool NoteKernelRunResult::has_skipped() const noexcept
  {
    if (skipped > 0)
    {
      return true;
    }

    for (const NoteResult &result : results)
    {
      if (result.was_skipped())
      {
        return true;
      }
    }

    return false;
  }

  bool NoteKernelRunResult::has_results() const noexcept
  {
    return !results.empty();
  }

  NoteKernel::NoteKernel()
  {
    sync_options();
  }

  NoteKernel::NoteKernel(NoteDocument document)
      : session_(std::move(document))
  {
    sync_options();
  }

  NoteKernel::NoteKernel(NoteKernelOptions options)
      : options_(std::move(options))
  {
    sync_options();
  }

  NoteKernel::NoteKernel(NoteDocument document, NoteKernelOptions options)
      : options_(std::move(options)),
        session_(std::move(document))
  {
    sync_options();
  }

  const NoteKernelOptions &NoteKernel::options() const noexcept
  {
    return options_;
  }

  void NoteKernel::set_options(NoteKernelOptions options)
  {
    options_ = std::move(options);
    sync_options();
  }

  const ProjectContext &NoteKernel::project_context() const noexcept
  {
    return options_.projectContext;
  }

  void NoteKernel::set_project_context(ProjectContext context)
  {
    options_.projectContext = std::move(context);
    sync_options();
  }

  const NoteSession &NoteKernel::session() const noexcept
  {
    return session_;
  }

  NoteSession &NoteKernel::session() noexcept
  {
    return session_;
  }

  const NoteDocument &NoteKernel::document() const noexcept
  {
    return session_.document();
  }

  NoteDocument &NoteKernel::document() noexcept
  {
    return session_.document();
  }

  void NoteKernel::set_document(NoteDocument document)
  {
    session_.set_document(std::move(document));
  }

  std::size_t NoteKernel::cell_count() const noexcept
  {
    return session_.cell_count();
  }

  bool NoteKernel::has_cell(std::size_t index) const noexcept
  {
    return session_.has_cell(index);
  }

  bool NoteKernel::can_execute_cell(std::size_t index) const noexcept
  {
    return session_.can_execute_cell(index);
  }

  bool NoteKernel::can_execute_cell(const std::string &id) const noexcept
  {
    const std::optional<std::size_t> index =
        session_.cell_index(id);

    if (!index)
    {
      return false;
    }

    return can_execute_cell(*index);
  }

  std::optional<std::size_t> NoteKernel::cell_index(const std::string &id) const noexcept
  {
    return session_.cell_index(id);
  }

  NoteResult NoteKernel::run_cell(std::size_t index)
  {
    NoteCell *cell = session_.cell_at(index);

    if (cell == nullptr)
    {
      return NoteResult::failure("cell index out of range", 1)
          .add_error("cell index out of range");
    }

    if (!cell->executable())
    {
      return NoteResult::skipped("cell is not executable");
    }

    NoteResult result;

    switch (cell->kind())
    {
    case NoteCellKind::Reply:
      result = run_reply_cell(*cell);
      break;

    case NoteCellKind::Cpp:
      result = run_cpp_cell_internal(*cell);
      break;

    case NoteCellKind::Markdown:
    case NoteCellKind::Html:
    case NoteCellKind::Unknown:
    default:
      result = NoteResult::skipped("cell is not executable");
      break;
    }

    if (cell->executable())
    {
      (void)session_.apply_result(index, result);
    }

    return result;
  }

  NoteResult NoteKernel::run_cell(const std::string &id)
  {
    const std::optional<std::size_t> index =
        session_.cell_index(id);

    if (!index)
    {
      return NoteResult::failure("cell not found: " + id, 1)
          .add_error("cell not found: " + id);
    }

    return run_cell(*index);
  }

  NoteKernelRunResult NoteKernel::run_all()
  {
    NoteKernelRunResult run;

    for (std::size_t i = 0; i < session_.cell_count(); ++i)
    {
      ++run.visited;

      const NoteCell *cell = session_.cell_at(i);

      if (cell == nullptr)
      {
        continue;
      }

      if (!cell->executable())
      {
        if (options_.includeNonExecutableAsSkipped)
        {
          NoteResult skipped =
              NoteResult::skipped("cell is not executable");

          run.results.push_back(skipped);
          ++run.skipped;
        }

        continue;
      }

      NoteResult result = run_cell(i);
      run.results.push_back(result);
      ++run.executed;

      if (result.failed())
      {
        ++run.failed;
        run.ok = false;

        if (options_.stopOnFirstFailure)
        {
          run.stopped = true;
          break;
        }
      }
      else if (result.was_skipped())
      {
        ++run.skipped;
      }
    }

    if (run.has_failures())
    {
      run.ok = false;
    }

    return run;
  }

  NoteKernelRunResult NoteKernel::run_executable_cells()
  {
    NoteKernelRunResult run;

    for (std::size_t i = 0; i < session_.cell_count(); ++i)
    {
      const NoteCell *cell = session_.cell_at(i);

      if (cell == nullptr || !cell->executable())
      {
        continue;
      }

      ++run.visited;

      NoteResult result = run_cell(i);
      run.results.push_back(result);
      ++run.executed;

      if (result.failed())
      {
        ++run.failed;
        run.ok = false;

        if (options_.stopOnFirstFailure)
        {
          run.stopped = true;
          break;
        }
      }
      else if (result.was_skipped())
      {
        ++run.skipped;
      }
    }

    if (run.has_failures())
    {
      run.ok = false;
    }

    return run;
  }

  void NoteKernel::clear_outputs()
  {
    session_.clear_outputs();
  }

  void NoteKernel::reset_execution()
  {
    session_.reset_execution();
    session_.clear_records();
  }

  void NoteKernel::reset()
  {
    session_.clear_outputs();
    session_.reset_execution();
    session_.clear_records();
  }

  NoteResult NoteKernel::run_reply_cell(const NoteCell &cell)
  {
    return replyRunner_.run_cell(cell);
  }

  NoteResult NoteKernel::run_cpp_cell_internal(const NoteCell &cell)
  {
    return cppRunner_.run_cell(cell);
  }

  void NoteKernel::sync_options()
  {
    options_.sessionOptions.stopOnFirstFailure = options_.stopOnFirstFailure;
    options_.cppOptions.projectContext = options_.projectContext;

    session_.set_options(options_.sessionOptions);
    cppRunner_.set_options(options_.cppOptions);
    replyRunner_.set_options(options_.replyOptions);
  }

  NoteKernelRunResult run_note(NoteDocument document)
  {
    NoteKernel kernel(std::move(document));
    return kernel.run_all();
  }

  NoteResult run_note_cell(NoteDocument document, std::size_t index)
  {
    NoteKernel kernel(std::move(document));
    return kernel.run_cell(index);
  }

  NoteResult run_note_cell(NoteDocument document, const std::string &id)
  {
    NoteKernel kernel(std::move(document));
    return kernel.run_cell(id);
  }
}
