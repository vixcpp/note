/**
 *
 *  @file note.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Public entry point for the Vix Note module.
 *
 *  Provides the stable public API for Vix Note, including the core document
 *  model, parser, storage helpers, runtime kernel, local UI route/server
 *  facade, and HTML exporter.
 *
 *  Usage:
 *    #include <vix/note/note.hpp>
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

#ifndef VIX_NOTE_NOTE_HPP
#define VIX_NOTE_NOTE_HPP

#include <vix/note/Version.hpp>

#include <vix/note/core/NoteCell.hpp>
#include <vix/note/core/NoteDocument.hpp>
#include <vix/note/core/NoteError.hpp>
#include <vix/note/core/NoteResult.hpp>

#include <vix/note/parser/NoteParser.hpp>

#include <vix/note/project/ProjectContext.hpp>
#include <vix/note/project/ProjectDetector.hpp>

#include <vix/note/storage/NoteStore.hpp>

#include <vix/note/runtime/CppCellRunner.hpp>
#include <vix/note/runtime/NoteKernel.hpp>
#include <vix/note/runtime/NoteSession.hpp>

#include <vix/note/web/NoteAssets.hpp>
#include <vix/note/web/NoteRoutes.hpp>
#include <vix/note/web/NoteServer.hpp>

#include <vix/note/export/HtmlExporter.hpp>

#endif // VIX_NOTE_NOTE_HPP
