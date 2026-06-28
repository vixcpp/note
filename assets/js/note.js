/*
 * Vix Note — notebook frontend
 *
 * Lightweight editor experience over the existing Vix Note HTTP API:
 *   GET    /api/document
 *   POST   /api/cells                 { kind, source, index? }
 *   PUT    /api/cells/<id>            { kind, source }
 *   DELETE /api/cells/<id>
 *   POST   /api/cells/<id>/run
 *   POST   /api/cells/<id>/move       { index }
 *   POST   /api/run-all
 *   POST   /api/document/save
 *   POST   /api/document/new          { path, title }
 *   POST   /api/document/open         { path }
 *   POST   /api/directory/create      { path }
 *   POST   /api/directory/list        { path }
 *   POST   /api/path/delete           { path, recursive }
 *   POST   /api/path/rename           { path, newName }
 *
 * Goals: one cell = one DOM node (no duplication), stable status bar,
 * VS Code-style activity bar + explorer + open tabs, a notebook toolbar
 * scoped to the editor zone, and — new in this revision — VS Code-style
 * INLINE creation of files and folders directly inside the explorer tree
 * (no modal pop-ups for "New note" / "New folder").
 *
 * No framework, no external dependency, vanilla JS only.
 *
 * Copyright 2026, Gaspard Kirira. MIT License.
 */
(() => {
  "use strict";

  /* ==========================================================
   * State
   * ======================================================== */
  const state = {
    document: null,
    selectedId: null,
    editing: false,
    kernel: "idle",
    busy: false,
    sidebarCollapsed: false,
    sidebarWidth: 260,
    focusMode: false,
    activePanel: "explorer", // explorer | problems
    // Explorer entries are loaded from the local backend directory API.
    // Tabs stay session-local because open editor buffers are UI state.
    explorer: {
      rootPath: ".",
      currentPath: ".",
      selectedDirPath: ".",
      loadingPath: null,

      // path -> { path, type: 'file'|'dir', title?, modified, openable?, extension?, size? }
      entries: new Map(),

      // Directory paths already loaded from the backend.
      loadedDirs: new Set(),

      // Directory paths visually expanded in the tree.
      expandedDirs: new Set(["."]),

      // VS Code-style inline create. Null when no draft is active.
      // { kind: 'file'|'dir', parentPath: string, error: string|null }
      draft: null,
    },
    tabs: [], // [{ path, title, dirty, doc? }]
    activeTabPath: null,

    // Diagnostics / Problems. A pure projection of NoteResult outputs for the
    // active note. Never persisted: diagnostics belong to the live run only.
    diagnostics: {
      status: "idle", // idle | running | success | failed
      items: [], // [{ id, cellId, cellLabel, severity, kind, message, ts }]
      byCell: new Map(), // cellId -> count of error-severity items
    },
  };

  const DEFAULT_SIDEBAR_WIDTH = 260;
  const MIN_SIDEBAR_WIDTH = 190;
  const MAX_SIDEBAR_WIDTH = 520;
  const TABS_STORAGE_KEY = "vix-note:tabs:v1";

  const app = document.querySelector("[data-note-app]");

  const sel = {
    cells: "[data-note-cells]",
    project: "[data-note-project]",
    cellCount: "[data-note-cell-count]",
    execCount: "[data-note-exec-count]",
    kernel: "[data-note-kernel]",
    message: "[data-note-message]",
    toolbarKind: '[data-action="toolbar-kind"]',
    statusMode: "[data-status-mode]",
    statusPosition: "[data-status-position]",
    statusKind: "[data-status-kind]",
    statusKernel: "[data-status-kernel]",
    sidebar: "[data-sidebar]",
    sidebarResizer: "[data-sidebar-resizer]",
    explorerList: "[data-explorer-list]",
    explorerCount: "[data-explorer-count]",
    explorerSearch: "[data-explorer-search]",
    tabsBar: "[data-tabs-bar]",
    problemsList: "[data-problems-list]",
    problemsCount: "[data-problems-count]",
    problemsSummary: "[data-problems-summary]",
    problemsSummaryText: "[data-problems-summary-text]",
    problemsBadge: "[data-activity-problems-badge]",
    statusProblems: "[data-status-problems]",
    statusProblemsCount: "[data-status-problems-count]",
  };

  const $ = (s, root = document) => root.querySelector(s);
  const $all = (s, root = document) => Array.from(root.querySelectorAll(s));

  /* ==========================================================
   * Helpers
   * ======================================================== */
  function escapeHtml(value) {
    return String(value ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function normalizeKind(kind) {
    return String(kind || "unknown").toLowerCase();
  }

  function safeClass(value) {
    return String(value || "unknown")
      .toLowerCase()
      .replace(/[^a-z0-9_-]/g, "-");
  }

  function isCodeKind(kind) {
    const k = normalizeKind(kind);
    return k === "cpp" || k === "reply";
  }

  function pad2(n) {
    return String(n).padStart(2, "0");
  }

  function suggestedNoteName() {
    const d = new Date();
    const stamp =
      `${d.getFullYear()}${pad2(d.getMonth() + 1)}${pad2(d.getDate())}` +
      `_${pad2(d.getHours())}${pad2(d.getMinutes())}${pad2(d.getSeconds())}`;
    return `note_${stamp}.vixnote`;
  }

  function baseName(path) {
    return (
      String(path || "")
        .split(/[\\/]/)
        .pop() || String(path || "")
    );
  }

  function stripExtension(name) {
    const base = baseName(name);
    const dot = base.lastIndexOf(".");
    return dot > 0 ? base.slice(0, dot) : base;
  }

  function titleFromFileName(name) {
    const stem = stripExtension(name).replace(/[_-]+/g, " ").trim();
    if (!stem) return "New Note";
    return stem.replace(/\b\w/g, (c) => c.toUpperCase());
  }

  function noteTitleFromPath(path) {
    return titleFromFileName(baseName(path));
  }

  function defaultIntroSource(title) {
    return `# ${title}\n\nStart writing your lesson here.`;
  }

  function isDefaultIntroSource(source, title) {
    const text = String(source || "").trim();

    return (
      text === `# ${title}\n\nStart writing your lesson here.` ||
      text === `# ${title}\n\nStart writing your note here.`
    );
  }

  async function retitleActiveDocumentAfterRename(oldPath, newPath, type) {
    if (type !== "file") {
      return;
    }

    if (!String(newPath || "").endsWith(".vixnote")) {
      return;
    }

    if (!state.document) {
      return;
    }

    if (
      normalizeExplorerPath(state.document.path) !==
      normalizeExplorerPath(oldPath)
    ) {
      return;
    }

    const oldTitle = noteTitleFromPath(oldPath);
    const newTitle = noteTitleFromPath(newPath);
    const currentTitle = String(state.document.title || "").trim();

    /*
     * Only auto-retitle notes that still use the generated title.
     * If the user gave the note a custom title, do not overwrite it.
     */
    if (currentTitle && currentTitle !== oldTitle) {
      state.document.path = newPath;
      document.title = `${currentTitle} · Vix Note`;
      return;
    }

    state.document.path = newPath;
    state.document.title = newTitle;

    document.title = `${newTitle} · Vix Note`;

    const tab = activeTab();
    if (tab) {
      tab.title = newTitle;
    }

    const first = cells()[0];

    if (
      first &&
      normalizeKind(first.kind) === "markdown" &&
      isDefaultIntroSource(first.source, oldTitle)
    ) {
      first.source = defaultIntroSource(newTitle);

      await api(`/api/cells/${encodeURIComponent(first.id)}`, {
        method: "PUT",
        body: JSON.stringify({
          kind: first.kind,
          source: first.source,
        }),
      });
    }

    await api("/api/document/update", {
      method: "POST",
      body: JSON.stringify({
        title: newTitle,
        save: true,
      }),
    });

    renderDocument(
      {
        ok: true,
        document: state.document,
      },
      {
        fullRepaint: true,
      },
    );

    persistTabs();
  }

  function kindLabel(kind) {
    switch (normalizeKind(kind)) {
      case "markdown":
        return "Markdown";
      case "reply":
        return "Reply";
      case "cpp":
        return "C++";
      case "html":
        return "HTML";
      default:
        return "Code";
    }
  }

  function relativeTime(epochMs) {
    if (!epochMs) return "";
    const diff = Date.now() - epochMs;
    const sec = Math.round(diff / 1000);
    if (sec < 45) return "just now";
    const min = Math.round(sec / 60);
    if (min < 60) return min === 1 ? "1 min ago" : `${min} min ago`;
    const hr = Math.round(min / 60);
    if (hr < 24) return hr === 1 ? "1 hour ago" : `${hr} hours ago`;
    const day = Math.round(hr / 24);
    if (day === 1) return "Yesterday";
    if (day < 7) return `${day} days ago`;
    const wk = Math.round(day / 7);
    if (wk < 5) return wk === 1 ? "1 week ago" : `${wk} weeks ago`;
    return new Date(epochMs).toLocaleDateString();
  }

  function setText(s, value) {
    const el = $(s);
    if (el) el.textContent = value;
  }

  function setKernel(status) {
    state.kernel = status;
    app.setAttribute("data-kernel", status);
    const label =
      status === "busy" ? "Busy" : status === "error" ? "Error" : "Idle";
    setText(sel.kernel, label);
    setText(sel.statusKernel, label);
  }

  let messageTimer = null;
  function setMessage(message, kind = "info") {
    const el = $(sel.message);
    if (!el) return;
    clearTimeout(messageTimer);
    if (!message) {
      el.hidden = true;
      el.textContent = "";
      el.className = "vn-Notice";
      return;
    }
    el.hidden = false;
    el.textContent = message;
    el.className = `vn-Notice vn-Notice--${safeClass(kind)}`;
    if (kind === "success" || kind === "info") {
      messageTimer = setTimeout(() => setMessage(""), 2600);
    }
  }

  function clearMessageQuietly() {
    setMessage("");
  }

  function setBusy(busy) {
    state.busy = busy;
    for (const b of $all(".vn-ToolbarButton")) b.disabled = busy;
  }

  /* ==========================================================
   * API layer
   * ======================================================== */
  async function api(path, options = {}, config = {}) {
    const headers = {
      Accept: "application/json",
      ...(options.body ? { "Content-Type": "application/json" } : {}),
      ...(options.headers || {}),
    };
    const response = await fetch(path, { ...options, headers });
    const contentType = response.headers.get("content-type") || "";
    const text = await response.text();

    let body;
    if (contentType.includes("application/json")) {
      try {
        body = text ? JSON.parse(text) : {};
      } catch (_) {
        body = { ok: false, error: text || "Invalid JSON response" };
      }
    } else {
      body = { ok: false, error: text };
    }

    if (!response.ok && !config.allowErrorResponse) {
      const msg =
        body.error ||
        body.message ||
        body?.result?.message ||
        `Request failed (${response.status})`;
      throw new Error(msg);
    }
    return body;
  }

  function unwrapDocument(payload) {
    if (!payload) return null;
    if (payload.document) return payload.document;
    return payload;
  }

  /* ==========================================================
   * Model accessors
   * ======================================================== */
  function cells() {
    return state.document && Array.isArray(state.document.cells)
      ? state.document.cells
      : [];
  }
  function findCell(id) {
    return cells().find((c) => String(c.id) === String(id)) || null;
  }
  function cellIndex(id) {
    return cells().findIndex((c) => String(c.id) === String(id));
  }
  function projectLabel(project) {
    if (!project || !project.enabled) return "No project";
    return project.projectName || project.projectRoot || "Project";
  }
  function executedCount() {
    return cells().filter((c) => Number(c.executionCount || 0) > 0).length;
  }
  function currentDocPath() {
    return (state.document && state.document.path) || "";
  }
  function normalizeExplorerPath(path) {
    let p = String(path || ".")
      .trim()
      .replaceAll("\\", "/");

    while (p.startsWith("./")) {
      p = p.slice(2);
    }

    while (p.length > 1 && p.endsWith("/")) {
      p = p.slice(0, -1);
    }

    return p || ".";
  }

  function parentPath(path) {
    const p = normalizeExplorerPath(path);

    if (p === "." || !p.includes("/")) {
      return ".";
    }

    const parts = p.split("/");
    parts.pop();

    return parts.join("/") || ".";
  }

  function documentDisplayTitle(doc) {
    if (!doc) {
      return "Untitled note";
    }

    const title = String(doc.title || "").trim();

    if (title) {
      return title;
    }

    const file = baseName(doc.path || "");

    if (file) {
      return file;
    }

    return "Untitled note";
  }

  function ancestorDirectoriesForPath(path) {
    const normalized = normalizeExplorerPath(path);

    const dirs = ["."];
    const parts = normalized.split("/");

    // Remove file name.
    parts.pop();

    let acc = "";

    for (const part of parts) {
      if (!part || part === ".") {
        continue;
      }

      acc = acc ? `${acc}/${part}` : part;
      dirs.push(acc);
    }

    return Array.from(new Set(dirs));
  }

  async function loadExplorerForDocumentPath(path) {
    const normalized = normalizeExplorerPath(path);
    const dirs = ancestorDirectoriesForPath(normalized);

    for (const dir of dirs) {
      state.explorer.expandedDirs.add(dir);

      await loadDirectory(dir, {
        silent: true,
        force: true,
      });
    }

    /*
     * Important:
     * Do NOT create a fake explorer file here.
     *
     * The explorer must only show files returned by /api/directory/list
     * or files explicitly created/opened through the backend.
     *
     * Unsaved notes live only in the editor/tabs, not in the disk explorer.
     */
    renderExplorer();
  }

  function joinExplorerPath(dir, name) {
    const folder = normalizeExplorerPath(dir || ".");
    const cleanName = String(name || "")
      .trim()
      .replaceAll("\\", "/")
      .split("/")
      .filter(Boolean)
      .pop();

    if (!cleanName) {
      return "";
    }

    return folder === "." ? cleanName : `${folder}/${cleanName}`;
  }
  function knownDirectories() {
    const dirs = Array.from(state.explorer.entries.values())
      .filter((entry) => entry.type === "dir")
      .map((entry) => normalizeExplorerPath(entry.path));

    if (!dirs.includes(".")) {
      dirs.unshift(".");
    }

    return Array.from(new Set(dirs)).sort((a, b) => {
      if (a === ".") return -1;
      if (b === ".") return 1;
      return a.localeCompare(b);
    });
  }

  function shouldShowEntry(entry) {
    if (!entry) {
      return false;
    }

    if (entry.type === "dir") {
      return true;
    }

    return entry.openable || String(entry.path || "").endsWith(".vixnote");
  }

  function entryModified(entry) {
    const modified = Number(entry && entry.modified ? entry.modified : 0);
    return Number.isFinite(modified) ? modified : 0;
  }

  function directChildrenOf(parent) {
    const parentPathValue = normalizeExplorerPath(parent);

    return Array.from(state.explorer.entries.values())
      .filter(shouldShowEntry)
      .filter((entry) => {
        const path = normalizeExplorerPath(entry.path);

        if (path === parentPathValue) {
          return false;
        }

        return parentPath(path) === parentPathValue;
      })
      .sort((a, b) => {
        if (a.type !== b.type) {
          return a.type === "dir" ? -1 : 1;
        }

        return String(a.title || baseName(a.path)).localeCompare(
          String(b.title || baseName(b.path)),
          undefined,
          { sensitivity: "base" },
        );
      });
  }

  function removeTabState(path) {
    const normalized = normalizeExplorerPath(path);

    state.tabs = state.tabs.filter((tab) => {
      return normalizeExplorerPath(tab.path) !== normalized;
    });

    if (
      state.activeTabPath &&
      normalizeExplorerPath(state.activeTabPath) === normalized
    ) {
      state.activeTabPath = state.tabs.length ? state.tabs[0].path : null;
    }

    state.explorer.entries.delete(normalized);

    persistTabs();
    renderOpenTabs();
    renderTabsBar();
    renderExplorer();
  }

  function isMissingNoteError(error) {
    const message = String(
      error && error.message ? error.message : error || "",
    ).toLowerCase();

    return (
      message.includes("cannot open note file") ||
      message.includes("not found") ||
      message.includes("no such file") ||
      message.includes("failed to load note")
    );
  }

  function isVirtualUnsavedDocument(doc) {
    const path = normalizeExplorerPath(doc?.path || "");
    const title = String(doc?.title || "")
      .trim()
      .toLowerCase();

    return (
      !path ||
      path === "untitled.vixnote" ||
      path === "untitled" ||
      title === "tmp"
    );
  }

  function buildExplorerTreeRows(parent = ".", depth = 0, rows = []) {
    const parentPathValue = normalizeExplorerPath(parent);

    if (parentPathValue === ".") {
      const root = state.explorer.entries.get(".") || {
        path: ".",
        type: "dir",
        title: ".",
        modified: 0,
        openable: false,
      };

      rows.push({
        ...root,
        depth: 0,
      });

      // Draft directly under root renders right after the root row.
      pushDraftRowIfMatches(".", 1, rows);
    }

    if (
      parentPathValue !== "." &&
      !state.explorer.expandedDirs.has(parentPathValue)
    ) {
      return rows;
    }

    const children = directChildrenOf(parentPathValue);

    for (const child of children) {
      const path = normalizeExplorerPath(child.path);

      rows.push({
        ...child,
        depth: depth + 1,
      });

      if (child.type === "dir" && state.explorer.expandedDirs.has(path)) {
        // Draft directly inside this expanded directory renders as the
        // first child row of the directory.
        pushDraftRowIfMatches(path, depth + 2, rows);
        buildExplorerTreeRows(path, depth + 1, rows);
      } else if (child.type === "dir") {
        // Collapsed directory: still allow a draft inside it (it forces
        // an expand when the draft is started, so this is mostly defensive).
        pushDraftRowIfMatches(path, depth + 2, rows);
      }
    }

    return rows;
  }

  function pushDraftRowIfMatches(parentPathValue, depth, rows) {
    const draft = state.explorer.draft;
    if (!draft) return;
    if (normalizeExplorerPath(draft.parentPath) !== parentPathValue) return;

    rows.push({
      isDraft: true,
      draftKind: draft.kind,
      path: `__draft__:${parentPathValue}`,
      type: draft.kind === "dir" ? "dir" : "file",
      depth,
    });
  }

  function explorerTreeRows() {
    const filter = (
      ($(sel.explorerSearch) && $(sel.explorerSearch).value) ||
      ""
    )
      .trim()
      .toLowerCase();

    if (!filter) {
      return buildExplorerTreeRows(".", 0, []);
    }

    /*
     * Search mode: show matching entries with their natural depth.
     * This keeps search simple while normal mode stays a real tree.
     * Inline drafts are not shown while filtering.
     */
    return Array.from(state.explorer.entries.values())
      .filter(shouldShowEntry)
      .filter((entry) => {
        return (
          String(entry.path || "")
            .toLowerCase()
            .includes(filter) ||
          String(entry.title || "")
            .toLowerCase()
            .includes(filter)
        );
      })
      .sort((a, b) => {
        if (a.type !== b.type) {
          return a.type === "dir" ? -1 : 1;
        }

        return String(a.path || "").localeCompare(String(b.path || ""));
      })
      .map((entry) => ({
        ...entry,
        depth:
          normalizeExplorerPath(entry.path) === "."
            ? 0
            : normalizeExplorerPath(entry.path).split("/").length,
      }));
  }

  async function toggleDirectory(path) {
    const dirPath = normalizeExplorerPath(path);

    state.explorer.selectedDirPath = dirPath;
    state.explorer.currentPath = dirPath;

    if (state.explorer.expandedDirs.has(dirPath) && dirPath !== ".") {
      state.explorer.expandedDirs.delete(dirPath);
      renderExplorer();
      return;
    }

    state.explorer.expandedDirs.add(dirPath);

    await loadDirectory(dirPath, {
      force: false,
      silent: true,
    });
  }

  /* ==========================================================
   * Markdown renderer (small, dependency-free)
   * ======================================================== */
  function renderMarkdown(source) {
    const lines = String(source || "").split(/\r?\n/);
    const blocks = [];
    let para = [];
    let inCode = false;
    let code = [];

    const inline = (line) => {
      let s = escapeHtml(line);
      s = s.replace(/`([^`]+)`/g, "<code>$1</code>");
      s = s.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
      s = s.replace(/\*([^*]+)\*/g, "<em>$1</em>");
      return s;
    };
    const flushPara = () => {
      if (!para.length) return;
      blocks.push(`<p>${para.map(inline).join("<br>")}</p>`);
      para = [];
    };

    for (const line of lines) {
      const t = line.trim();
      if (t.startsWith("```")) {
        if (inCode) {
          blocks.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
          code = [];
          inCode = false;
        } else {
          flushPara();
          inCode = true;
        }
        continue;
      }
      if (inCode) {
        code.push(line);
        continue;
      }
      if (!t) {
        flushPara();
        continue;
      }
      if (t.startsWith("### ")) {
        flushPara();
        blocks.push(`<h3>${inline(t.slice(4))}</h3>`);
        continue;
      }
      if (t.startsWith("## ")) {
        flushPara();
        blocks.push(`<h2>${inline(t.slice(3))}</h2>`);
        continue;
      }
      if (t.startsWith("# ")) {
        flushPara();
        blocks.push(`<h1>${inline(t.slice(2))}</h1>`);
        continue;
      }
      para.push(line);
    }
    if (inCode)
      blocks.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
    flushPara();
    return blocks.length ? blocks.join("\n") : "<p></p>";
  }

  /* ==========================================================
   * Syntax highlighter
   * ======================================================== */
  const CPP_KEYWORDS = new Set([
    "alignas",
    "alignof",
    "and",
    "auto",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "class",
    "const",
    "constexpr",
    "consteval",
    "constinit",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "nullptr",
    "operator",
    "or",
    "private",
    "protected",
    "public",
    "register",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "while",
    "wchar_t",
    "override",
    "final",
    "concept",
    "requires",
    "co_await",
    "co_return",
    "co_yield",
  ]);
  const CPP_TYPES = new Set([
    "string",
    "vector",
    "map",
    "set",
    "unordered_map",
    "unordered_set",
    "array",
    "size_t",
    "ssize_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "ostream",
    "istream",
    "ostringstream",
    "istringstream",
    "optional",
    "pair",
    "tuple",
    "shared_ptr",
    "unique_ptr",
    "function",
    "string_view",
    "filesystem",
    "path",
    "atomic",
    "thread",
    "mutex",
  ]);
  const REPLY_KEYWORDS = new Set([
    "true",
    "false",
    "null",
    "let",
    "const",
    "var",
    "fn",
    "if",
    "else",
    "while",
    "for",
    "return",
    "print",
    "println",
  ]);

  function tokenizeCode(source, kind) {
    const k = normalizeKind(kind);
    const isReply = k === "reply";
    const keywords = isReply ? REPLY_KEYWORDS : CPP_KEYWORDS;
    const types = isReply ? new Set() : CPP_TYPES;

    let out = "";
    const s = String(source ?? "");
    let i = 0;
    const n = s.length;
    const push = (cls, text) => {
      out += `<span class="${cls}">${escapeHtml(text)}</span>`;
    };
    const raw = (text) => {
      out += escapeHtml(text);
    };

    while (i < n) {
      const c = s[i];
      if (!isReply && c === "#" && (i === 0 || s[i - 1] === "\n")) {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-pre", s.slice(i, j));
        i = j;
        continue;
      }
      if (c === "/" && s[i + 1] === "/") {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      if (isReply && c === "#") {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      if (c === "/" && s[i + 1] === "*") {
        let j = i + 2;
        while (j < n && !(s[j] === "*" && s[j + 1] === "/")) j++;
        j = Math.min(n, j + 2);
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      if (c === '"' || c === "'") {
        const q = c;
        let j = i + 1;
        while (j < n) {
          if (s[j] === "\\") {
            j += 2;
            continue;
          }
          if (s[j] === q) {
            j++;
            break;
          }
          if (s[j] === "\n") break;
          j++;
        }
        push("tok-str", s.slice(i, j));
        i = j;
        continue;
      }
      if (/[0-9]/.test(c)) {
        let j = i;
        while (j < n && /[0-9a-fA-FxXuUlL.']/.test(s[j])) j++;
        push("tok-num", s.slice(i, j));
        i = j;
        continue;
      }
      if (/[A-Za-z_]/.test(c)) {
        let j = i;
        while (j < n && /[A-Za-z0-9_]/.test(s[j])) j++;
        const word = s.slice(i, j);
        let k2 = j;
        while (k2 < n && s[k2] === " ") k2++;
        const isCall = s[k2] === "(";
        if (keywords.has(word)) push("tok-kw", word);
        else if (types.has(word)) push("tok-type", word);
        else if (!isReply && /^[A-Z]/.test(word)) push("tok-type", word);
        else if (isCall) push("tok-fn", word);
        else raw(word);
        i = j;
        continue;
      }
      raw(c);
      i++;
    }
    if (!out.endsWith("\n")) out += "\n";
    return out;
  }

  /* ==========================================================
   * Output rendering
   * ======================================================== */
  function renderOutputs(outputs) {
    const list = Array.isArray(outputs) ? outputs : [];
    if (!list.length) return "";
    return list
      .map((o) => {
        const kind = normalizeKind(o.kind);
        const content = String(o.content ?? "");
        if (kind === "html") {
          return `<div class="vn-Output vn-Output--html">${content}</div>`;
        }
        const label =
          kind === "stdout"
            ? ""
            : `<span class="vn-Output__kind">${escapeHtml(kind)}</span>`;
        return `<div class="vn-Output vn-Output--${safeClass(kind)}">${label}<pre>${escapeHtml(content)}</pre></div>`;
      })
      .join("");
  }

  function runningOutputHtml(message) {
    return `<div class="vn-Output vn-Output--running"><pre>${escapeHtml(message)}</pre></div>`;
  }

  /* ==========================================================
   * Cell icons
   * ======================================================== */
  const ICONS = {
    run: '<svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>',
    up: '<svg viewBox="0 0 24 24"><path d="M12 7l6 6-1.4 1.4L12 9.8l-4.6 4.6L6 13l6-6z"/></svg>',
    down: '<svg viewBox="0 0 24 24"><path d="M12 17l-6-6 1.4-1.4L12 14.2l4.6-4.6L18 11l-6 6z"/></svg>',
    del: '<svg viewBox="0 0 24 24"><path d="M6 7h12l-1 14H7L6 7zm3-3h6l1 2H8l1-2z"/></svg>',
    copy: '<svg viewBox="0 0 24 24"><path d="M16 1H4a2 2 0 00-2 2v14h2V3h12V1zm3 4H8a2 2 0 00-2 2v14a2 2 0 002 2h11a2 2 0 002-2V7a2 2 0 00-2-2zm0 16H8V7h11v14z"/></svg>',
    edit: '<svg viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 000-1.41l-2.34-2.34a1 1 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
  };

  /* ==========================================================
   * Cell rendering pieces
   * ======================================================== */
  function inPrompt(cell) {
    if (!isCodeKind(cell.kind))
      return `<div class="vn-InputPrompt vn-InputPrompt--empty"></div>`;
    const n = Number(cell.executionCount || 0);
    const label = n > 0 ? `In&nbsp;[${n}]:` : `In&nbsp;[&nbsp;]:`;
    const cls =
      n > 0 ? "vn-InputPrompt" : "vn-InputPrompt vn-InputPrompt--empty";
    return `<div class="${cls}">${label}</div>`;
  }
  function outPrompt(cell) {
    if (!isCodeKind(cell.kind)) return "";
    const n = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    if (!outs.length) return `<div class="vn-OutputPrompt"></div>`;
    const label = n > 0 ? `Out[${n}]:` : `Out[&nbsp;]:`;
    return `<div class="vn-OutputPrompt">${label}</div>`;
  }

  function editorBlock(cell) {
    const kind = normalizeKind(cell.kind);
    const source = String(cell.source ?? "");
    const code = isCodeKind(kind);
    const editorCls = code ? "vn-Editor" : "vn-Editor vn-Editor--plain";
    const highlight = code
      ? `<div class="vn-Editor__highlight" data-highlight>${tokenizeCode(source, kind)}</div>`
      : "";

    return `
  <div class="${editorCls}">
    <div class="vn-Editor__wrap">
      ${highlight}
      <div class="vn-Editor__lineFocus" data-line-focus aria-hidden="true"></div>
      <textarea spellcheck="false" data-action="edit-source" rows="1">${escapeHtml(source)}</textarea>
    </div>
  </div>`;
  }

  function cellBody(cell) {
    const kind = normalizeKind(cell.kind);
    if (kind === "markdown") {
      return `
        <div class="vn-MarkdownCell">
          <div class="vn-RenderedMarkdown" data-rendered>${renderMarkdown(cell.source)}</div>
          <div class="vn-InputArea">
            <div class="vn-InputPrompt vn-InputPrompt--empty"></div>
            ${editorBlock(cell)}
          </div>
        </div>`;
    }
    if (kind === "html") {
      return `
        <div class="vn-HtmlCell">
          <div class="vn-RenderedHTML" data-rendered>${String(cell.source || "")}</div>
          <div class="vn-InputArea">
            <div class="vn-InputPrompt vn-InputPrompt--empty"></div>
            ${editorBlock(cell)}
          </div>
        </div>`;
    }
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    const outputArea = outs.length
      ? `<div class="vn-OutputArea">
           ${outPrompt(cell)}
           <div class="vn-OutputArea__list">${renderOutputs(outs)}</div>
         </div>`
      : "";
    return `
      <div class="vn-CodeCell">
        <div class="vn-InputArea">
          ${inPrompt(cell)}
          ${editorBlock(cell)}
        </div>
        ${outputArea}
      </div>`;
  }

  function cellToolbar(cell) {
    const firstBtn = isCodeKind(cell.kind)
      ? `<button type="button" data-cell-action="run" title="Run">${ICONS.run}</button>`
      : `<button type="button" data-cell-action="edit" title="Edit source">${ICONS.edit}</button>`;
    return `
      <div class="vn-Cell__toolbar">
        ${firstBtn}
        <button type="button" data-cell-action="duplicate" title="Duplicate">${ICONS.copy}</button>
        <button type="button" data-cell-action="up" title="Move up">${ICONS.up}</button>
        <button type="button" data-cell-action="down" title="Move down">${ICONS.down}</button>
        <button type="button" data-cell-action="delete" title="Delete">${ICONS.del}</button>
      </div>`;
  }

  function renderCell(cell) {
    const kind = normalizeKind(cell.kind);
    const id = String(cell.id || "");
    const selected = state.selectedId === id;
    const editing = selected && state.editing;
    const typeClass =
      kind === "markdown"
        ? "vn-Cell--markdown"
        : kind === "html"
          ? "vn-Cell--html"
          : "vn-Cell--code";
    const classes = [
      "vn-Cell",
      typeClass,
      selected ? "is-selected" : "",
      editing ? "is-editing" : "",
    ]
      .filter(Boolean)
      .join(" ");

    return `
      <div class="vn-CellInsert">
        <button class="vn-CellInsert__btn" type="button" data-insert-after="${escapeHtml(id)}" title="Insert cell below">+</button>
      </div>
      <div class="${classes}" data-cell-id="${escapeHtml(id)}" data-kind="${escapeHtml(kind)}" tabindex="-1">
        <div class="vn-Cell__collapser" data-cell-action="select"></div>
        <div class="vn-Cell__body">
          ${cellToolbar(cell)}
          ${cellBody(cell)}
        </div>
      </div>`;
  }

  function renderEmpty(message) {
    const c = $(sel.cells);
    if (c)
      c.innerHTML = `<div class="vn-Notebook__empty">${escapeHtml(message)}</div>`;
  }

  function clearEditorNoOpenNote() {
    state.document = null;
    state.selectedId = null;
    state.editing = false;
    state.kernel = "idle";
    state.activeTabPath = null;

    setText(sel.cellCount, "0");
    setText(sel.execCount, "0");
    setText(sel.statusPosition, "Cell 0 of 0");
    setText(sel.statusKind, "—");
    setText(sel.statusMode, "Command");

    document.title = "Vix Note";

    if (state.diagnostics) clearDiagnostics();

    renderEmpty(
      "No open note. Open a note from the explorer or create a new one.",
    );
    renderOpenTabs();
    renderTabsBar();
  }

  function updateStatusBar() {
    const list = cells();
    const idx = cellIndex(state.selectedId);
    setText(sel.statusMode, state.editing ? "Edit" : "Command");
    setText(
      sel.statusPosition,
      `Cell ${idx >= 0 ? idx + 1 : 0} of ${list.length}`,
    );
    const cell = findCell(state.selectedId);
    setText(sel.statusKind, cell ? kindLabel(cell.kind) : "—");
  }

  /* ==========================================================
   * Keyed reconcile — guarantees one cell = one DOM node
   * ======================================================== */
  function cellSignature(cell) {
    const kind = normalizeKind(cell.kind);
    const exec = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs.length : 0;
    const selected = state.selectedId === String(cell.id);
    const ed = selected && state.editing;
    return `${kind}|${exec}|${outs}|${selected ? 1 : 0}|${ed ? 1 : 0}`;
  }

  function buildCellNodes(cell) {
    const tpl = document.createElement("template");
    tpl.innerHTML = renderCell(cell).trim();
    return Array.from(tpl.content.children);
  }

  function patchCellOutputs(cellEl, cell) {
    const codeCell = $(".vn-CodeCell", cellEl);
    if (!codeCell) return;
    const inP = $(".vn-InputPrompt", codeCell);
    if (inP) {
      const tmp = document.createElement("div");
      tmp.innerHTML = inPrompt(cell);
      const fresh = tmp.firstElementChild;
      if (fresh) inP.replaceWith(fresh);
    }
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    let oa = $(".vn-OutputArea", codeCell);
    if (!outs.length) {
      if (oa) oa.remove();
      return;
    }
    const html = `<div class="vn-OutputArea">${outPrompt(cell)}<div class="vn-OutputArea__list">${renderOutputs(outs)}</div></div>`;
    const tmp = document.createElement("div");
    tmp.innerHTML = html;
    const fresh = tmp.firstElementChild;
    if (oa) oa.replaceWith(fresh);
    else codeCell.appendChild(fresh);
  }

  function insertBarOf(cellEl) {
    const prev = cellEl.previousElementSibling;
    return prev && prev.classList.contains("vn-CellInsert") ? prev : null;
  }

  function reconcileCells(container, list) {
    const existing = new Map();
    for (const el of $all(".vn-Cell", container))
      existing.set(el.dataset.cellId, el);

    const desired = [];
    for (const cell of list) {
      const id = String(cell.id);
      const sig = cellSignature(cell);
      const prev = existing.get(id);

      if (prev && prev.dataset.sig === sig) {
        desired.push([insertBarOf(prev), prev]);
        existing.delete(id);
      } else if (prev && state.editing && state.selectedId === id) {
        patchCellOutputs(prev, cell);
        prev.dataset.sig = sig;
        desired.push([insertBarOf(prev), prev]);
        existing.delete(id);
      } else {
        const nodes = buildCellNodes(cell);
        const bar = nodes[0];
        const cellEl = nodes[1];
        if (cellEl && cellEl.dataset) cellEl.dataset.sig = sig;
        if (prev) {
          const oldBar = insertBarOf(prev);
          if (oldBar) oldBar.remove();
          prev.remove();
          existing.delete(id);
        }
        desired.push([bar, cellEl]);
      }
    }

    for (const [, el] of existing) {
      const bar = insertBarOf(el);
      if (bar) bar.remove();
      el.remove();
    }

    const frag = document.createDocumentFragment();
    for (const [bar, cellEl] of desired) {
      if (bar) frag.appendChild(bar);
      if (cellEl) frag.appendChild(cellEl);
    }
    container.appendChild(frag);
  }

  function renderDocument(payload, opts = {}) {
    const doc = unwrapDocument(payload);
    if (!doc) {
      renderEmpty("Unable to load the note document.");
      return;
    }
    state.document = doc;

    const title = documentDisplayTitle(doc);
    const count = Number(doc.cellCount || (doc.cells ? doc.cells.length : 0));
    const project = doc.project || null;

    setText(sel.project, projectLabel(project));
    setText(sel.cellCount, String(count));
    setText(sel.execCount, String(executedCount()));
    document.title = `${title} · Vix Note`;

    if (doc.path && !isVirtualUnsavedDocument(doc)) {
      syncActiveTab({
        ...doc,
        title,
      });

      renderOpenTabs();
      renderTabsBar();
    } else {
      renderTabsBar();
    }
    const container = $(sel.cells);
    if (!container) return;
    const list = cells();
    if (!list.length) {
      renderEmpty(
        "This note has no cells yet. Use the toolbar + or the insert button to add a cell.",
      );
      updateStatusBar();
      return;
    }
    if (!state.selectedId || !findCell(state.selectedId)) {
      state.selectedId = String(list[0].id);
    }

    const firstPaint = !$(".vn-Cell", container);
    const scroller = container.closest(".vn-NotebookPanel");
    const prevScroll = scroller ? scroller.scrollTop : 0;

    if (firstPaint || opts.fullRepaint) {
      container.innerHTML = list.map(renderCell).join("");
      for (const el of $all(".vn-Cell", container)) {
        const cell = findCell(el.dataset.cellId);
        if (cell) el.dataset.sig = cellSignature(cell);
      }
    } else {
      reconcileCells(container, list);
    }

    if (scroller) scroller.scrollTop = prevScroll;
    autosizeAll();
    syncToolbarKind();
    updateStatusBar();

    // Cells may have been added/removed/reordered; prune stale diagnostics
    // and re-apply problem badges to the freshly painted cells.
    if (state.diagnostics && state.diagnostics.items.length) {
      recomputeDiagnosticsState();
    } else {
      refreshCellProblemBadges();
    }
  }

  /* ==========================================================
   * Editor autosize + highlight + textarea editing
   * ======================================================== */
  const EDITOR_INDENT = "  ";

  function autosize(textarea) {
    if (!textarea) return;

    textarea.style.height = "auto";

    const nextHeight = Math.max(textarea.scrollHeight, 96);
    textarea.style.height = `${nextHeight}px`;

    const wrap = textarea.closest(".vn-Editor__wrap");
    const hl = wrap ? $("[data-highlight]", wrap) : null;

    if (hl) {
      hl.style.height = `${nextHeight}px`;
    }

    updateLineFocus(textarea);
  }

  function autosizeAll() {
    for (const ta of $all('textarea[data-action="edit-source"]')) {
      autosize(ta);
      updateHighlight(ta);
      updateLineFocus(ta);
    }
  }

  function updateHighlight(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");
    if (!wrap) return;
    const hl = $("[data-highlight]", wrap);
    if (!hl) return;
    const cellEl = textarea.closest(".vn-Cell");
    const kind = cellEl ? cellEl.dataset.kind : "cpp";
    hl.innerHTML = tokenizeCode(textarea.value, kind);
  }

  function syncScroll(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");
    if (!wrap) return;
    const hl = $("[data-highlight]", wrap);
    if (hl) {
      hl.scrollTop = textarea.scrollTop;
      hl.scrollLeft = textarea.scrollLeft;
    }
    updateLineFocus(textarea);
  }

  function textareaLineInfo(textarea) {
    const value = textarea.value || "";
    const cursor = textarea.selectionStart || 0;
    const before = value.slice(0, cursor);
    const lines = before.split("\n");
    const line = lines.length;
    const column = lines[lines.length - 1].length + 1;
    return { line, column };
  }

  function updateCursorStatus(textarea) {
    const info = textareaLineInfo(textarea);
    setText(sel.statusPosition, `Ln ${info.line}, Col ${info.column}`);
  }

  function updateLineFocus(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");
    if (!wrap) return;
    const focus = $("[data-line-focus]", wrap);
    if (!focus) return;
    const info = textareaLineInfo(textarea);
    const style = window.getComputedStyle(textarea);
    const lineHeight = Number.parseFloat(style.lineHeight) || 22;
    const paddingTop = Number.parseFloat(style.paddingTop) || 0;
    const top = paddingTop + (info.line - 1) * lineHeight - textarea.scrollTop;
    focus.style.transform = `translateY(${top}px)`;
    focus.style.height = `${lineHeight}px`;
  }

  function markTextareaChanged(textarea) {
    autosize(textarea);
    updateHighlight(textarea);
    updateLineFocus(textarea);
    updateCursorStatus(textarea);

    const cellEl = textarea.closest(".vn-Cell");
    if (!cellEl) return;

    const cell = findCell(cellEl.dataset.cellId);
    if (cell) cell.source = textarea.value;

    setDirty(true);

    const kind = cellEl.dataset.kind;
    if (kind === "markdown") {
      const rendered = $("[data-rendered]", cellEl);
      if (rendered) rendered.innerHTML = renderMarkdown(textarea.value);
    } else if (kind === "html") {
      const rendered = $("[data-rendered]", cellEl);
      if (rendered) rendered.innerHTML = String(textarea.value || "");
    }
  }

  function selectedLineRange(textarea) {
    const value = textarea.value;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    let lineStart = value.lastIndexOf("\n", Math.max(0, start - 1)) + 1;
    let lineEnd = value.indexOf("\n", end);
    if (lineEnd === -1) lineEnd = value.length;
    return { lineStart, lineEnd, start, end };
  }

  function indentTextarea(textarea) {
    const value = textarea.value;
    const { lineStart, lineEnd, start, end } = selectedLineRange(textarea);
    const selected = value.slice(lineStart, lineEnd);

    if (start === end) {
      textarea.value = value.slice(0, start) + EDITOR_INDENT + value.slice(end);
      textarea.selectionStart = textarea.selectionEnd =
        start + EDITOR_INDENT.length;
      markTextareaChanged(textarea);
      return;
    }

    const indented = selected
      .split("\n")
      .map((line) => EDITOR_INDENT + line)
      .join("\n");

    textarea.value =
      value.slice(0, lineStart) + indented + value.slice(lineEnd);
    textarea.selectionStart = start + EDITOR_INDENT.length;
    textarea.selectionEnd =
      end + EDITOR_INDENT.length * selected.split("\n").length;
    markTextareaChanged(textarea);
  }

  function outdentTextarea(textarea) {
    const value = textarea.value;
    const { lineStart, lineEnd, start, end } = selectedLineRange(textarea);
    const selected = value.slice(lineStart, lineEnd);
    const lines = selected.split("\n");

    let removedBeforeStart = 0;
    let removedTotal = 0;

    const outdented = lines
      .map((line, index) => {
        let remove = 0;
        if (line.startsWith(EDITOR_INDENT)) remove = EDITOR_INDENT.length;
        else if (line.startsWith("\t")) remove = 1;
        else if (line.startsWith(" ")) remove = 1;
        if (index === 0) removedBeforeStart = remove;
        removedTotal += remove;
        return line.slice(remove);
      })
      .join("\n");

    textarea.value =
      value.slice(0, lineStart) + outdented + value.slice(lineEnd);
    textarea.selectionStart = Math.max(lineStart, start - removedBeforeStart);
    textarea.selectionEnd = Math.max(
      textarea.selectionStart,
      end - removedTotal,
    );
    markTextareaChanged(textarea);
  }

  function toggleCommentTextarea(textarea) {
    const cellEl = textarea.closest(".vn-Cell");
    const kind = normalizeKind(cellEl ? cellEl.dataset.kind : "cpp");
    const marker = kind === "html" ? null : kind === "markdown" ? "> " : "// ";
    if (!marker) return;

    const value = textarea.value;
    const { lineStart, lineEnd, start, end } = selectedLineRange(textarea);
    const selected = value.slice(lineStart, lineEnd);
    const lines = selected.split("\n");

    const allCommented = lines
      .filter((line) => line.trim() !== "")
      .every((line) => line.trimStart().startsWith(marker.trim()));

    let delta = 0;

    const next = lines
      .map((line) => {
        if (line.trim() === "") return line;
        const indent = line.match(/^\s*/)?.[0] || "";
        if (allCommented) {
          const afterIndent = line.slice(indent.length);
          if (afterIndent.startsWith(marker)) {
            delta -= marker.length;
            return indent + afterIndent.slice(marker.length);
          }
          if (afterIndent.startsWith(marker.trim())) {
            delta -= marker.trim().length;
            return (
              indent + afterIndent.slice(marker.trim().length).replace(/^ /, "")
            );
          }
          return line;
        }
        delta += marker.length;
        return indent + marker + line.slice(indent.length);
      })
      .join("\n");

    textarea.value = value.slice(0, lineStart) + next + value.slice(lineEnd);
    textarea.selectionStart = start;
    textarea.selectionEnd = Math.max(start, end + delta);
    markTextareaChanged(textarea);
  }

  function moveCurrentLine(textarea, direction) {
    const value = textarea.value;
    const cursor = textarea.selectionStart;
    const lines = value.split("\n");

    let pos = 0;
    let lineIndex = 0;
    for (; lineIndex < lines.length; ++lineIndex) {
      const nextPos = pos + lines[lineIndex].length + 1;
      if (cursor < nextPos) break;
      pos = nextPos;
    }

    const target = lineIndex + direction;
    if (target < 0 || target >= lines.length) return;

    const currentLine = lines[lineIndex];
    lines[lineIndex] = lines[target];
    lines[target] = currentLine;

    textarea.value = lines.join("\n");

    const movedBy =
      direction < 0 ? -(lines[lineIndex].length + 1) : lines[target].length + 1;
    const nextCursor = Math.max(
      0,
      Math.min(textarea.value.length, cursor + movedBy),
    );
    textarea.selectionStart = textarea.selectionEnd = nextCursor;
    markTextareaChanged(textarea);
  }

  /* ==========================================================
   * Selection / modes
   * ======================================================== */
  function cssEscape(v) {
    return String(v).replace(/["\\]/g, "\\$&");
  }
  function cellElById(id) {
    return $(`.vn-Cell[data-cell-id="${cssEscape(id)}"]`);
  }

  function selectCell(id, { edit = false, focus = true } = {}) {
    state.selectedId = String(id);
    state.editing = edit;
    for (const el of $all(".vn-Cell")) {
      const isSel = el.dataset.cellId === String(id);
      el.classList.toggle("is-selected", isSel);
      el.classList.toggle("is-editing", isSel && edit);
    }
    const el = cellElById(id);
    if (el) {
      if (edit) {
        const ta = $('textarea[data-action="edit-source"]', el);
        if (ta && focus) {
          ta.focus({ preventScroll: true });
          autosize(ta);
        }
      } else if (focus) {
        el.focus({ preventScroll: true });
      }
    }
    syncToolbarKind();
    updateStatusBar();
  }

  function enterEditMode() {
    if (state.selectedId) selectCell(state.selectedId, { edit: true });
  }

  function exitEditMode(options = {}) {
    const repaint = options.repaint !== false;
    state.editing = false;
    if (app) {
      app.classList.remove("is-editing");
      app.classList.add("is-command");
    }
    setText(sel.statusMode, "Command");
    if (repaint) {
      renderDocument(
        { ok: true, document: state.document },
        { fullRepaint: true },
      );
    }
  }

  function toggleCellEdit(cellId) {
    const id = String(cellId || "");
    if (!id) return;
    if (state.editing && state.selectedId === id) {
      const cellEl = cellElById(id);
      if (cellEl) localUpdateFromDom(cellEl);
      exitEditMode();
      return;
    }
    selectCell(id, { edit: true, focus: true });
  }

  function enterCommandMode() {
    state.editing = false;
    const el = cellElById(state.selectedId);
    if (el) {
      el.classList.remove("is-editing");
      el.focus({ preventScroll: true });
    }
    updateStatusBar();
  }
  function selectAdjacent(delta) {
    const list = cells();
    const idx = cellIndex(state.selectedId);
    if (idx < 0) return;
    const next = idx + delta;
    if (next < 0 || next >= list.length) return;
    selectCell(list[next].id, { edit: false });
    const el = cellElById(list[next].id);
    if (el) el.scrollIntoView({ block: "nearest" });
  }

  function toolbarKindLabel(kind) {
    switch (normalizeKind(kind)) {
      case "cpp":
        return "C++";
      case "reply":
        return "Reply";
      case "markdown":
        return "Markdown";
      case "html":
        return "HTML";
      default:
        return "C++";
    }
  }

  function closeToolbarKindMenu() {
    const button = $(sel.toolbarKind);
    const menu = $("[data-toolbar-kind-menu]");
    const root = $("[data-cell-type-select]");

    if (button) {
      button.setAttribute("aria-expanded", "false");
    }

    if (menu) {
      menu.setAttribute("hidden", "");
    }

    if (root) {
      root.classList.remove("is-open");
    }
  }
  function toggleToolbarKindMenu() {
    const button = $(sel.toolbarKind);
    const menu = $("[data-toolbar-kind-menu]");
    const root = $("[data-cell-type-select]");

    if (!button || !menu) {
      return;
    }

    const nextOpen = menu.hasAttribute("hidden");

    if (nextOpen) {
      menu.removeAttribute("hidden");
    } else {
      menu.setAttribute("hidden", "");
    }

    button.setAttribute("aria-expanded", nextOpen ? "true" : "false");

    if (root) {
      root.classList.toggle("is-open", nextOpen);
    }
  }

  function setToolbarKind(kind, options = {}) {
    const nextKind = ["cpp", "reply", "markdown", "html"].includes(
      normalizeKind(kind),
    )
      ? normalizeKind(kind)
      : "cpp";

    const button = $(sel.toolbarKind);
    const label = $("[data-toolbar-kind-label]");

    if (button) {
      button.dataset.kind = nextKind;
    }

    if (label) {
      label.textContent = toolbarKindLabel(nextKind);
    }

    for (const option of $all("[data-kind-option]")) {
      const active = option.dataset.kindOption === nextKind;
      option.classList.toggle("is-active", active);
      option.setAttribute("aria-selected", active ? "true" : "false");
    }

    if (options.applyToCell !== false && state.selectedId) {
      changeKind(state.selectedId, nextKind);
    }
  }

  function syncToolbarKind() {
    const cell = findCell(state.selectedId);
    const kind = cell ? normalizeKind(cell.kind) : currentToolbarKind();

    setToolbarKind(
      ["cpp", "reply", "markdown", "html"].includes(kind) ? kind : "cpp",
      { applyToCell: false },
    );
  }

  /* ==========================================================
   * Cell sync
   * ======================================================== */
  function localUpdateFromDom(cellEl) {
    const cell = findCell(cellEl.dataset.cellId);
    if (!cell) return null;
    const ta = $('textarea[data-action="edit-source"]', cellEl);
    if (ta) cell.source = ta.value;
    return cell;
  }
  async function pushCell(cellEl) {
    const cell = localUpdateFromDom(cellEl);
    if (!cell) return;
    const id = encodeURIComponent(cellEl.dataset.cellId);
    const result = await api(`/api/cells/${id}`, {
      method: "PUT",
      body: JSON.stringify({ kind: cell.kind, source: cell.source }),
    });
    if (result.document) state.document = unwrapDocument(result.document);
  }

  /* ==========================================================
   * Dirty tracking (per active tab)
   * ======================================================== */
  function setDirty(dirty) {
    const tab = activeTab();
    if (tab) tab.dirty = !!dirty;
    persistTabs();
    renderOpenTabs();
    renderTabsBar();
  }
  function isDirty() {
    const tab = activeTab();
    return !!(tab && tab.dirty);
  }

  /* ==========================================================
   * Cell actions
   * ======================================================== */
  function defaultSource(kind) {
    const k = normalizeKind(kind);
    if (k === "cpp")
      return '#include <iostream>\n\nint main()\n{\n  std::cout << "Hello from Vix Note\\n";\n  return 0;\n}\n';
    if (k === "reply") return 'x = 1 + 2 * 3\nprintln("x =", x)\n';
    if (k === "html")
      return "<section>\n  <h2>Hello</h2>\n  <p>Rendered by the note UI.</p>\n</section>\n";
    return "Write your explanation here.";
  }

  async function addCell(
    kind,
    { afterId = null, atIndex = null, source = null } = {},
  ) {
    setMessage("");
    setBusy(true);
    const body = {
      kind,
      source: source != null ? source : defaultSource(kind),
    };
    if (atIndex != null) body.index = atIndex;
    else if (afterId != null) {
      const idx = cellIndex(afterId);
      if (idx >= 0) body.index = idx + 1;
    }
    try {
      const result = await api("/api/cells", {
        method: "POST",
        body: JSON.stringify(body),
      });
      const newId = result.cellId || result.cell?.id || null;
      if (result.document) {
        state.selectedId = newId || state.selectedId;
        renderDocument(result.document);
      } else await loadDocument();
      if (newId) {
        selectCell(newId, { edit: normalizeKind(kind) !== "markdown" });
        const el = cellElById(newId);
        if (el) el.scrollIntoView({ block: "nearest" });
      }
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to add cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function duplicateCell(id) {
    const cell = findCell(id);
    if (!cell) return;
    await addCell(normalizeKind(cell.kind), {
      afterId: id,
      source: String(cell.source || ""),
    });
  }

  async function runCellById(id) {
    const cellEl = cellElById(id);
    const cell = findCell(id);
    if (!cellEl || !cell) return;

    if (!isCodeKind(cell.kind)) {
      localUpdateFromDom(cellEl);
      try {
        await pushCell(cellEl);
      } catch (_) {}
      return;
    }

    cellEl.classList.add("is-running");
    setMessage("");
    setKernel("busy");
    setDiagnosticsRunning();

    const codeCell = $(".vn-CodeCell", cellEl);
    if (codeCell) {
      let oa = $(".vn-OutputArea", codeCell);
      if (!oa) {
        codeCell.insertAdjacentHTML(
          "beforeend",
          `<div class="vn-OutputArea"><div class="vn-OutputPrompt">Out[&nbsp;]:</div><div class="vn-OutputArea__list">${runningOutputHtml("Running…")}</div></div>`,
        );
      } else {
        const listEl = $(".vn-OutputArea__list", oa);
        if (listEl) listEl.innerHTML = runningOutputHtml("Running…");
      }
    }

    try {
      await pushCell(cellEl);
      const key = encodeURIComponent(id);
      const result = await api(
        `/api/cells/${key}/run`,
        { method: "POST" },
        { allowErrorResponse: true },
      );

      if (result.document) {
        state.document = unwrapDocument(result.document);
        setText(sel.execCount, String(executedCount()));
        const fresh = findCell(id);
        const stillEl = cellElById(id);
        if (fresh && stillEl) {
          patchCellOutputs(stillEl, fresh);
          stillEl.dataset.sig = cellSignature(fresh);
        } else {
          renderDocument(result.document);
        }
      } else {
        await loadDocument();
      }

      const status = normalizeKind(result?.result?.status);

      // Project the result into diagnostics for this cell.
      setCellDiagnostics(id, result?.result);

      if (status === "failure") {
        setKernel("error");
        setMessage(
          result?.result?.message || "Cell execution failed.",
          "error",
        );
        setTimeout(() => setKernel("idle"), 1200);
      } else if (status === "skipped") {
        setKernel("idle");
        setMessage(result?.result?.message || "Cell skipped.", "warning");
      } else {
        setKernel("idle");
      }
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to run cell.", "error");
      state.diagnostics.status = "failed";
      renderProblems();
      setTimeout(() => setKernel("idle"), 1200);
    } finally {
      const el = cellElById(id);
      if (el) el.classList.remove("is-running");
    }
  }

  async function runAll() {
    setMessage("");
    setBusy(true);
    setKernel("busy");
    setDiagnosticsRunning();
    try {
      for (const el of $all(".vn-Cell")) {
        try {
          await pushCell(el);
        } catch (_) {}
      }
      const result = await api(
        "/api/run-all",
        { method: "POST" },
        { allowErrorResponse: true },
      );
      if (result.document) renderDocument(result.document);

      // Project every cell result into diagnostics in one pass.
      setRunAllDiagnostics(result);

      if (result.ok) {
        setKernel("idle");
        setMessage(`Ran ${result.executed || 0} cell(s).`, "success");
      } else if (result.stopped) {
        setKernel("error");
        setMessage("Run stopped after a failed cell.", "error");
        setTimeout(() => setKernel("idle"), 1200);
      } else {
        setKernel("error");
        setMessage("Run completed with failures.", "error");
        setTimeout(() => setKernel("idle"), 1200);
      }
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to run all cells.", "error");
      state.diagnostics.status = "failed";
      renderProblems();
      setTimeout(() => setKernel("idle"), 1200);
    } finally {
      setBusy(false);
    }
  }

  async function moveCellById(id, direction) {
    const idx = cellIndex(id);
    if (idx < 0) return;
    const target = direction === "up" ? idx - 1 : idx + 1;
    const list = cells();
    if (target < 0 || target >= list.length) return;
    setMessage("");
    setBusy(true);
    try {
      const cellEl = cellElById(id);
      if (cellEl) {
        try {
          await pushCell(cellEl);
        } catch (_) {}
      }
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}/move`, {
        method: "POST",
        body: JSON.stringify({ index: target }),
      });
      state.selectedId = String(id);
      if (result.document) renderDocument(result.document);
      else await loadDocument();
      selectCell(id, { edit: false });
      const el = cellElById(id);
      if (el) el.scrollIntoView({ block: "nearest" });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to move cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function deleteCellById(id) {
    setMessage("");
    setBusy(true);
    const idx = cellIndex(id);
    const list = cells();
    const fallbackId =
      idx >= 0
        ? (list[idx + 1] && list[idx + 1].id) ||
          (list[idx - 1] && list[idx - 1].id) ||
          null
        : null;
    try {
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}`, { method: "DELETE" });
      state.selectedId = fallbackId ? String(fallbackId) : null;
      if (result.document) renderDocument(result.document);
      else await loadDocument();
      if (state.selectedId) selectCell(state.selectedId, { edit: false });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to delete cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function changeKind(id, newKind) {
    const cellEl = cellElById(id);
    const cell = findCell(id);
    if (!cellEl || !cell) return;
    if (normalizeKind(cell.kind) === normalizeKind(newKind)) {
      selectCell(id, { edit: false });
      return;
    }
    localUpdateFromDom(cellEl);
    cell.kind = newKind || "cpp";
    setBusy(true);
    try {
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}`, {
        method: "PUT",
        body: JSON.stringify({ kind: cell.kind, source: cell.source }),
      });
      if (result.document) {
        state.selectedId = String(id);
        renderDocument(result.document);
      }
      selectCell(id, { edit: false });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to change cell type.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function saveNote() {
    setMessage("");
    setBusy(true);
    try {
      for (const el of $all(".vn-Cell")) {
        try {
          await pushCell(el);
        } catch (_) {}
      }
      const saved = await api("/api/document/save", { method: "POST" });
      const savedDoc =
        unwrapDocument(saved?.document || saved) || state.document;

      if (savedDoc && savedDoc.path) {
        state.document = savedDoc;

        touchExplorerEntry(savedDoc.path, "file", baseName(savedDoc.path), {
          modified: Date.now(),
          openable: true,
          extension: ".vixnote",
        });

        openTab(savedDoc.path, documentDisplayTitle(savedDoc));
        state.activeTabPath = normalizeExplorerPath(savedDoc.path);

        await loadDirectory(parentPath(savedDoc.path), {
          silent: true,
          force: true,
        });

        renderExplorer();
        renderTabsBar();
        renderOpenTabs();
      }

      setDirty(false);
      clearMessageQuietly();
    } catch (error) {
      setMessage(error.message || "Failed to save note.", "error");
      setDirty(true);
    } finally {
      setBusy(false);
    }
  }

  /* ==========================================================
   * VS Code-style INLINE create (files + folders)
   *
   * No modal: a single input row appears inside the tree, right where
   * the new entry will live. Enter commits, Escape / blur cancels.
   * ======================================================== */

  // The folder a new entry should be created in, given the current
  // selection. If a file is selected, we use its parent folder, exactly
  // like VS Code.
  function inlineCreateParent(explicitDir) {
    if (explicitDir != null) {
      return normalizeExplorerPath(explicitDir);
    }

    const selected = normalizeExplorerPath(
      state.explorer.selectedDirPath || ".",
    );
    const entry = state.explorer.entries.get(selected);

    if (entry && entry.type === "file") {
      return parentPath(selected);
    }

    return selected || ".";
  }

  async function startInlineCreate(kind, explicitDir = null) {
    // Search mode hides drafts; clear the filter so the row is visible.
    const search = $(sel.explorerSearch);
    if (search && search.value) {
      search.value = "";
    }

    const parent = inlineCreateParent(explicitDir);

    // Make sure the parent is expanded and loaded so the draft row has a
    // place to live and we can validate duplicates against real entries.
    if (parent !== ".") {
      state.explorer.expandedDirs.add(parent);
    }

    state.explorer.selectedDirPath = parent;
    state.explorer.currentPath = parent;
    state.explorer.draft = { kind, parentPath: parent, error: null };

    // Ensure the activity bar shows the explorer panel.
    if (state.activePanel !== "explorer" || state.sidebarCollapsed) {
      setPanel("explorer");
    }

    // Load the directory contents silently (so duplicate detection works),
    // then render and focus the inline input.
    if (parent !== "." && !state.explorer.loadedDirs.has(parent)) {
      await loadDirectory(parent, { silent: true, force: false });
    } else {
      renderExplorer();
    }

    focusDraftInput();
  }

  function cancelInlineCreate() {
    if (!state.explorer.draft) return;
    state.explorer.draft = null;
    renderExplorer();
  }

  function focusDraftInput() {
    // Defer to the next frame so the freshly-rendered input exists.
    requestAnimationFrame(() => {
      const input = $(".vn-Tree__input");
      if (!input) return;
      input.focus();
      input.select();
    });
  }

  function validateDraftName(rawName) {
    const draft = state.explorer.draft;
    if (!draft) return { ok: false };

    let name = String(rawName || "")
      .trim()
      .replaceAll("\\", "/");
    name = baseName(name);

    if (!name) {
      return { ok: false, error: "A name is required." };
    }

    if (/[<>:"|?*]/.test(name)) {
      return { ok: false, error: "Name contains invalid characters." };
    }

    if (draft.kind === "file" && !name.endsWith(".vixnote")) {
      name = `${name}.vixnote`;
    }

    const path = joinExplorerPath(draft.parentPath, name);
    if (!path) {
      return { ok: false, error: "Invalid name." };
    }

    const existing = state.explorer.entries.get(normalizeExplorerPath(path));
    if (existing) {
      return {
        ok: false,
        error:
          draft.kind === "dir"
            ? "A folder with that name already exists."
            : "A file with that name already exists.",
      };
    }

    return { ok: true, name, path };
  }

  async function commitInlineCreate(rawName) {
    const draft = state.explorer.draft;
    if (!draft) return;

    const result = validateDraftName(rawName);
    if (!result.ok) {
      // Empty name + Enter = silently cancel, like VS Code.
      if (!String(rawName || "").trim()) {
        cancelInlineCreate();
        return;
      }
      draft.error = result.error || "Invalid name.";
      renderExplorer();
      focusDraftInput();
      return;
    }

    const { name, path } = result;
    const kind = draft.kind;

    // Clear the draft now; the optimistic entry + backend call take over.
    state.explorer.draft = null;

    setBusy(true);
    setMessage("");

    try {
      if (kind === "dir") {
        await api("/api/directory/create", {
          method: "POST",
          body: JSON.stringify({ path }),
        });

        touchExplorerEntry(path, "dir", baseName(path), {
          modified: Date.now(),
          openable: false,
        });

        state.explorer.expandedDirs.add(parentPath(path));
        state.explorer.expandedDirs.add(path);
        state.explorer.selectedDirPath = path;

        await loadDirectory(parentPath(path), { silent: true, force: true });
        clearMessageQuietly();
      } else {
        const title = titleFromFileName(name);
        const doc = await api("/api/document/new", {
          method: "POST",
          body: JSON.stringify({ path, title }),
        });
        const d = unwrapDocument(doc);

        openTab(d.path, d.title || title);
        state.selectedId = null;
        renderDocument(doc, { fullRepaint: true });
        setDirty(false);

        touchExplorerEntry(d.path, "file", baseName(d.path), {
          modified: Date.now(),
          openable: true,
          extension: ".vixnote",
        });

        state.explorer.selectedDirPath = parentPath(d.path);
        await loadDirectory(parentPath(d.path), { silent: true, force: true });
        clearMessageQuietly();
      }
    } catch (error) {
      setMessage(
        error.message ||
          (kind === "dir"
            ? "Failed to create folder."
            : "Failed to create note."),
        "error",
      );
    } finally {
      setBusy(false);
      renderExplorer();
    }
  }

  // Public entry points used by toolbar buttons, menus and context menus.
  function newNote(dir = null) {
    return startInlineCreate("file", dir);
  }
  function newFolder(parentDir = null) {
    return startInlineCreate("dir", parentDir);
  }

  /* ==========================================================
   * Open note — kept as a lightweight modal (it takes a path, not a
   * new name in the tree, so inline editing doesn't apply here).
   * ======================================================== */
  async function openNote(prefill) {
    const data = await showModalForm({
      title: "Open note",
      fields: [
        {
          name: "path",
          label: "Path",
          value: prefill || "lessons/intro.vixnote",
          placeholder: "lessons/intro.vixnote",
          hint: "Path to an existing .vixnote file",
        },
      ],
      confirm: "Open",
    });
    if (!data) return;
    const path = (data.path || "").trim();
    if (!path) return;
    await openNotePath(path);
  }

  async function openNotePath(path, options = {}) {
    const silent = !!options.silent;
    if (!path) return;
    if (!path.endsWith(".vixnote")) {
      setMessage("Note path must end with .vixnote", "error");
      return;
    }
    setBusy(true);

    if (!silent) {
      setMessage("");
    }
    try {
      const doc = await api("/api/document/open", {
        method: "POST",
        body: JSON.stringify({ path }),
      });
      const d = unwrapDocument(doc);

      // Diagnostics belong to a single note; reset when loading another.
      clearDiagnostics();

      openTab(d.path, documentDisplayTitle(d));
      state.selectedId = null;
      renderDocument(doc, { fullRepaint: true });
      setDirty(false);
      persistTabs();

      touchExplorerEntry(d.path, "file", baseName(d.path), {
        modified: Date.now(),
        openable: true,
        extension: ".vixnote",
      });

      await loadExplorerForDocumentPath(d.path);
      if (!silent) {
        // Opening a note is a navigation action, not a success event.
        // Keep the editor clean.
        setMessage("");
      }
    } catch (error) {
      if (isMissingNoteError(error)) {
        removeTabState(path);
        console.debug(`[Vix Note] Removed stale tab: ${path}`);
        if (state.activeTabPath) {
          await openNotePath(state.activeTabPath, { silent: true });
        } else {
          clearEditorNoOpenNote();
          await loadDirectory(".", { silent: true, force: true });
        }
        return;
      }
      setMessage(error.message || "Failed to open note.", "error");
    } finally {
      setBusy(false);
    }
  }

  /* ==========================================================
   * Delete / rename
   * ======================================================== */
  async function deletePath(path, options = {}) {
    if (!path) return;
    const recursive = !!options.recursive;
    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/delete", {
        method: "POST",
        body: JSON.stringify({ path, recursive }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to delete path.");
      }

      const deletedActiveDocument =
        result.currentDeleted ||
        normalizeExplorerPath(path) === normalizeExplorerPath(currentDocPath());

      state.explorer.entries.delete(normalizeExplorerPath(path));

      const prefix = `${normalizeExplorerPath(path)}/`;
      for (const key of Array.from(state.explorer.entries.keys())) {
        if (key.startsWith(prefix)) {
          state.explorer.entries.delete(key);
        }
      }

      state.explorer.loadedDirs.delete(normalizeExplorerPath(path));
      state.explorer.expandedDirs.delete(normalizeExplorerPath(path));

      await loadDirectory(parentPath(path), { silent: true, force: true });

      if (deletedActiveDocument) {
        state.tabs = state.tabs.filter((tab) => {
          return (
            normalizeExplorerPath(tab.path) !== normalizeExplorerPath(path)
          );
        });
        state.activeTabPath = state.tabs.length ? state.tabs[0].path : null;
        if (state.activeTabPath) {
          await openNotePath(state.activeTabPath, { silent: true });
        } else {
          state.document = null;
          state.selectedId = null;
          await loadDocument();
        }
      }

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();
      clearMessageQuietly();
    } catch (error) {
      setMessage(error.message || "Failed to delete path.", "error");
    } finally {
      setBusy(false);
    }
  }

  // Rename is also inline (VS Code style): an input replaces the row label.
  async function startInlineRename(path, type = "file") {
    const normalized = normalizeExplorerPath(path);
    cancelInlineCreate();
    state.explorer.rename = {
      path: normalized,
      type,
      error: null,
    };
    renderExplorer();
    requestAnimationFrame(() => {
      const input = $(".vn-Tree__input--rename");
      if (!input) return;
      input.focus();
      // Select the name without the extension, like VS Code.
      const value = input.value;
      const dot = value.lastIndexOf(".");
      if (type === "file" && dot > 0) {
        input.setSelectionRange(0, dot);
      } else {
        input.select();
      }
    });
  }

  function cancelInlineRename() {
    if (!state.explorer.rename) return;
    state.explorer.rename = null;
    renderExplorer();
  }

  async function commitInlineRename(rawName) {
    const ren = state.explorer.rename;
    if (!ren) return;

    const oldPath = ren.path;
    const type = ren.type;

    let newName = String(rawName || "")
      .trim()
      .replaceAll("\\", "/");
    newName = baseName(newName);

    if (!newName || newName === baseName(oldPath)) {
      cancelInlineRename();
      return;
    }

    if (
      type === "file" &&
      oldPath.endsWith(".vixnote") &&
      !newName.endsWith(".vixnote")
    ) {
      newName = `${newName}.vixnote`;
    }

    const newPathOptimistic = joinExplorerPath(parentPath(oldPath), newName);
    if (state.explorer.entries.has(normalizeExplorerPath(newPathOptimistic))) {
      ren.error = "A file or folder with that name already exists.";
      renderExplorer();
      requestAnimationFrame(() => {
        const input = $(".vn-Tree__input--rename");
        if (input) input.focus();
      });
      return;
    }

    state.explorer.rename = null;
    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/rename", {
        method: "POST",
        body: JSON.stringify({ path: oldPath, newName }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to rename path.");
      }

      const newPath = normalizeExplorerPath(
        result.newPath || joinExplorerPath(parentPath(oldPath), newName),
      );

      const oldEntry = state.explorer.entries.get(oldPath);
      state.explorer.entries.delete(oldPath);

      const oldPrefix = `${oldPath}/`;
      const newPrefix = `${newPath}/`;
      for (const [key, entry] of Array.from(state.explorer.entries.entries())) {
        if (key.startsWith(oldPrefix)) {
          const renamedChildPath = normalizeExplorerPath(
            newPrefix + key.slice(oldPrefix.length),
          );
          state.explorer.entries.delete(key);
          state.explorer.entries.set(renamedChildPath, {
            ...entry,
            path: renamedChildPath,
            title: baseName(renamedChildPath),
          });
        }
      }

      state.explorer.entries.set(newPath, {
        ...(oldEntry || {}),
        path: newPath,
        type,
        title: baseName(newPath),
        modified: Date.now(),
        openable: type === "file" ? newPath.endsWith(".vixnote") : false,
        extension: type === "file" ? ".vixnote" : "",
      });

      if (state.explorer.loadedDirs.has(oldPath)) {
        state.explorer.loadedDirs.delete(oldPath);
        state.explorer.loadedDirs.add(newPath);
      }
      if (state.explorer.expandedDirs.has(oldPath)) {
        state.explorer.expandedDirs.delete(oldPath);
        state.explorer.expandedDirs.add(newPath);
      }
      if (state.explorer.selectedDirPath === oldPath) {
        state.explorer.selectedDirPath = newPath;
      }
      if (state.explorer.currentPath === oldPath) {
        state.explorer.currentPath = newPath;
      }

      for (const tab of state.tabs) {
        if (tab.path === oldPath) {
          const oldTitle = noteTitleFromPath(oldPath);
          const newTitle = noteTitleFromPath(newPath);

          tab.path = newPath;

          if (
            !tab.title ||
            tab.title === baseName(oldPath) ||
            tab.title === oldTitle
          ) {
            tab.title = newTitle;
          }
        }
      }
      if (state.activeTabPath === oldPath) {
        state.activeTabPath = newPath;
      }
      await retitleActiveDocumentAfterRename(oldPath, newPath, type);

      await loadDirectory(parentPath(newPath), { silent: true, force: true });

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();
      clearMessageQuietly();
    } catch (error) {
      state.explorer.rename = {
        path: oldPath,
        type,
        error:
          error.message || "Path not found. Save the note before renaming it.",
      };

      renderExplorer();

      requestAnimationFrame(() => {
        const input = $(".vn-Tree__input--rename");
        if (input) {
          input.focus();
          input.select();
        }
      });

      setMessage(
        error.message || "Path not found. Save the note before renaming it.",
        "error",
      );
    } finally {
      setBusy(false);
    }
  }

  async function confirmDelete(label, type = "file") {
    return showModalConfirm({
      title: "Delete from disk",
      body:
        type === "dir"
          ? `Delete folder “${escapeHtml(label)}” from disk? Empty folders are deleted directly. Non-empty folders need recursive delete.`
          : `Delete file “${escapeHtml(label)}” from disk? This cannot be undone from Vix Note.`,
      confirm: "Delete",
      danger: true,
    });
  }

  /* ==========================================================
   * Outputs (client-side display only)
   * ======================================================== */
  function clearCellOutput(id) {
    const cellEl = cellElById(id);
    if (!cellEl) return;
    const oa = $(".vn-OutputArea", cellEl);
    if (oa) oa.remove();
    const cell = findCell(id);
    if (cell) cell.outputs = [];
    // Drop this cell's diagnostics too.
    state.diagnostics.items = state.diagnostics.items.filter(
      (d) => d.cellId !== String(id),
    );
    recomputeDiagnosticsState();
    renderProblems();
  }
  function clearAllOutputs() {
    for (const cell of cells()) cell.outputs = [];
    for (const oa of $all(".vn-OutputArea")) oa.remove();
    clearDiagnostics();
    setMessage("Outputs cleared from view.", "info");
  }

  async function restartKernel(runAfter = false) {
    setKernel("busy");
    setMessage("Restarting kernel…", "info");
    setTimeout(async () => {
      setKernel("idle");
      if (runAfter) await runAll();
      else setMessage("Kernel restarted.", "info");
    }, 400);
  }

  async function restoreFirstAvailableTab() {
    while (state.tabs.length) {
      const path = state.activeTabPath || state.tabs[0].path;

      try {
        const before = state.activeTabPath;

        await openNotePath(path, { silent: true });

        if (
          !state.document ||
          normalizeExplorerPath(currentDocPath()) !==
            normalizeExplorerPath(path)
        ) {
          removeTabState(path);
          continue;
        }

        await loadExplorerForDocumentPath(path);
        return true;
      } catch (error) {
        removeTabState(path);
      }
    }

    clearEditorNoOpenNote();
    await loadDirectory(".", { silent: true, force: true });
    return false;
  }

  function isStartupScratchDocument(doc) {
    if (!doc) return true;

    const title = String(doc.title || "")
      .trim()
      .toLowerCase();
    const path = normalizeExplorerPath(doc.path || "").toLowerCase();
    const list = Array.isArray(doc.cells) ? doc.cells : [];
    const firstSource = String(list[0]?.source || "").toLowerCase();

    return (
      title === "tmp" &&
      (path === "untitled.vixnote" || path === "untitled") &&
      firstSource.includes("start writing your note here")
    );
  }

  /* ==========================================================
   * Load
   * ======================================================== */
  async function loadDocument() {
    setMessage("");
    const restored = restorePersistedTabs();

    if (restored) {
      renderOpenTabs();
      renderTabsBar();
      if (state.activeTabPath) {
        const ok = await restoreFirstAvailableTab();
        if (ok) setKernel("idle");
        return;
      }
      clearEditorNoOpenNote();
      await loadDirectory(".", { silent: true, force: true });
      return;
    }

    try {
      const doc = await api("/api/document");
      const d = unwrapDocument(doc);

      if (isStartupScratchDocument(d)) {
        clearEditorNoOpenNote();

        await loadDirectory(".", {
          silent: true,
          force: true,
        });

        setKernel("idle");
        return;
      }

      if (d && d.path && !state.activeTabPath) {
        openTab(d.path, documentDisplayTitle(d));
      }

      renderDocument(doc);
      setKernel("idle");

      if (d && d.path) {
        await loadExplorerForDocumentPath(d.path);
      } else {
        await loadDirectory(".", {
          silent: true,
          force: true,
        });
      }
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to load note document.", "error");
      renderEmpty("Unable to load the note document.");
      await loadDirectory(".", { silent: true, force: true });
    }
  }

  /* ==========================================================
   * Explorer (backend-backed model)
   * ======================================================== */
  function touchExplorerEntry(path, type, title, options = {}) {
    const normalized = normalizeExplorerPath(path);
    if (!normalized) return;

    const existing = state.explorer.entries.get(normalized);

    state.explorer.entries.set(normalized, {
      path: normalized,
      type,
      title: title || (existing && existing.title) || baseName(normalized),
      modified:
        options.modified ?? (existing && existing.modified) ?? Date.now(),
      openable: options.openable ?? (existing && existing.openable) ?? false,
      extension: options.extension ?? (existing && existing.extension) ?? "",
      size: options.size ?? (existing && existing.size) ?? 0,
    });

    if (normalized !== ".") {
      const parts = normalized.split("/");
      parts.pop();
      let acc = "";
      for (const part of parts) {
        if (!part) continue;
        acc = acc ? `${acc}/${part}` : part;
        if (!state.explorer.entries.has(acc)) {
          state.explorer.entries.set(acc, {
            path: acc,
            type: "dir",
            title: part,
            modified: 0,
            openable: false,
            extension: "",
            size: 0,
          });
        }
      }
    }
  }

  function mergeDirectoryList(payload, requestedPath) {
    const dirPath = normalizeExplorerPath(
      requestedPath || payload?.path || ".",
    );

    touchExplorerEntry(dirPath, "dir", baseName(dirPath), {
      modified: 0,
      openable: false,
    });

    state.explorer.loadedDirs.add(dirPath);
    state.explorer.expandedDirs.add(dirPath);

    const entries = Array.isArray(payload?.entries) ? payload.entries : [];

    for (const entry of entries) {
      const name = String(
        entry.name || baseName(entry.path || "") || "",
      ).trim();
      if (!name) continue;

      let path;
      if (dirPath === ".") {
        path = normalizeExplorerPath(entry.path || name);
      } else {
        const rawPath = normalizeExplorerPath(entry.path || name);
        if (rawPath === dirPath || rawPath.startsWith(`${dirPath}/`)) {
          path = rawPath;
        } else {
          path = normalizeExplorerPath(`${dirPath}/${name}`);
        }
      }

      if (!path || path === dirPath) continue;

      const type = entry.type === "dir" ? "dir" : "file";
      touchExplorerEntry(path, type, name || baseName(path), {
        modified: Number(entry.modified || 0),
        openable: !!entry.openable,
        extension: entry.extension || "",
        size: Number(entry.size || 0),
      });
    }
  }

  async function loadDirectory(path = ".", options = {}) {
    const dirPath = normalizeExplorerPath(path);
    const force = !!options.force;
    const silent = !!options.silent;

    if (!force && state.explorer.loadedDirs.has(dirPath)) {
      state.explorer.expandedDirs.add(dirPath);
      renderExplorer();
      return;
    }

    state.explorer.loadingPath = dirPath;
    if (!silent) setMessage(`Loading ${dirPath}…`, "info");
    renderExplorer();

    try {
      const payload = await api("/api/directory/list", {
        method: "POST",
        body: JSON.stringify({ path: dirPath }),
      });

      if (!payload || payload.ok === false) {
        throw new Error(payload?.error || "Failed to list directory");
      }

      state.explorer.currentPath = dirPath;
      mergeDirectoryList(payload, dirPath);
      if (!silent) setMessage("Explorer refreshed.", "success");
    } catch (error) {
      if (!silent) {
        setMessage(error.message || "Failed to load directory.", "error");
      }
    } finally {
      state.explorer.loadingPath = null;
      renderExplorer();
    }
  }

  async function refreshExplorer(path = ".") {
    const dirPath = normalizeExplorerPath(path || ".");
    state.explorer.draft = null;
    state.explorer.rename = null;
    state.explorer.entries.clear();
    state.explorer.loadedDirs.clear();
    state.explorer.expandedDirs.clear();
    state.explorer.expandedDirs.add(".");
    await loadDirectory(dirPath, { force: true, silent: false });
  }

  function fileIcon() {
    return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3v6h6"/></svg>';
  }

  function dirIcon(entry) {
    const path = normalizeExplorerPath(entry && entry.path);
    const loaded = state.explorer.loadedDirs.has(path);
    const expanded = state.explorer.expandedDirs.has(path);

    if (loaded && expanded) {
      return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1H3V7z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 10h18l-2 8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-.8L3 10z"/></svg>';
    }
    return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"/></svg>';
  }

  function draftIcon(kind) {
    return kind === "dir"
      ? '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"/></svg>'
      : fileIcon();
  }

  function draftRowHtml(row) {
    const draft = state.explorer.draft;
    const isDir = draft.kind === "dir";
    const placeholder = isDir ? "new-folder" : "new-note.vixnote";
    const defaultValue = isDir ? "" : suggestedNoteName();
    const errorAttr = draft.error ? " has-error" : "";
    const errorHtml = draft.error
      ? `<span class="vn-Tree__inputError">${escapeHtml(draft.error)}</span>`
      : "";
    const hint = isDir ? "Enter to create folder" : "Enter to create note";

    return `
    <div
      class="vn-Tree__row vn-Tree__row--draft"
      style="--depth:${Number(row.depth || 0)}"
    >
      <span class="vn-Tree__chevron"></span>
      ${draftIcon(draft.kind)}

      <div class="vn-Tree__inputWrap">
        <input
          class="vn-Tree__input${errorAttr}"
          type="text"
          data-tree-input
          value="${escapeHtml(defaultValue)}"
          placeholder="${escapeHtml(placeholder)}"
          autocomplete="off"
          spellcheck="false"
        />
        <span class="vn-Tree__inputHint">${escapeHtml(hint)}</span>
        ${errorHtml}
      </div>
    </div>`;
  }

  function renameRowHtml(row) {
    const ren = state.explorer.rename;
    const errorAttr = ren.error ? " has-error" : "";
    const errorHtml = ren.error
      ? `<span class="vn-Tree__inputError">${escapeHtml(ren.error)}</span>`
      : "";
    const icon = row.type === "dir" ? dirIcon(row) : fileIcon();
    const chevron =
      row.type === "dir"
        ? `<span class="vn-Tree__chevron">${
            state.explorer.expandedDirs.has(normalizeExplorerPath(row.path))
              ? "▾"
              : "▸"
          }</span>`
        : `<span class="vn-Tree__chevron"></span>`;

    return `
      <div
        class="vn-Tree__row vn-Tree__row--rename"
        style="--depth:${Number(row.depth || 0)}"
      >
        ${chevron}
        ${icon}
       <div class="vn-Tree__inputWrap">
          <input
            class="vn-Tree__input vn-Tree__input--rename${errorAttr}"
            type="text"
            data-tree-rename-input
            value="${escapeHtml(baseName(row.path))}"
            autocomplete="off"
            spellcheck="false"
          />
          <span class="vn-Tree__inputHint">Enter to rename</span>
          ${errorHtml}
        </div>
      </div>`;
  }

  function renderExplorer() {
    const listEl = $(sel.explorerList);
    if (!listEl) return;

    const entries = explorerTreeRows();
    const loadingPath = state.explorer.loadingPath;
    const renamePath = state.explorer.rename
      ? normalizeExplorerPath(state.explorer.rename.path)
      : null;

    // Count excludes the draft pseudo-row.
    const realCount = entries.filter((e) => !e.isDraft).length;
    setText(sel.explorerCount, String(realCount));

    if (loadingPath && !entries.length && !state.explorer.draft) {
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        Loading ${escapeHtml(loadingPath)}…
      </p>`;
      return;
    }

    if (!entries.length) {
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        No notes found. Create one with <strong>New note</strong> or refresh the explorer.
      </p>`;
      return;
    }

    listEl.innerHTML = entries
      .map((e) => {
        if (e.isDraft) {
          return draftRowHtml(e);
        }

        const path = normalizeExplorerPath(e.path);

        if (renamePath && path === renamePath) {
          return renameRowHtml(e);
        }

        const active =
          e.type === "file" && path === state.activeTabPath ? " is-active" : "";
        const loading =
          e.type === "dir" && path === loadingPath ? " is-loading" : "";
        const meta =
          e.type === "file"
            ? `<span class="vn-Tree__meta">${escapeHtml(relativeTime(entryModified(e)))}</span>`
            : "";
        const icon = e.type === "dir" ? dirIcon(e) : fileIcon();
        const depth = Number(e.depth || 0);
        const expanded = state.explorer.expandedDirs.has(path);

        return `
        <div
          class="vn-Tree__row${active}${loading}${expanded ? " is-expanded" : ""}"
          data-tree-path="${escapeHtml(path)}"
          data-tree-type="${escapeHtml(e.type)}"
          data-tree-openable="${e.openable ? "true" : "false"}"
          style="--depth:${depth}"
          tabindex="0"
        >
          ${
            e.type === "dir"
              ? `<span class="vn-Tree__chevron">${expanded ? "▾" : "▸"}</span>`
              : `<span class="vn-Tree__chevron"></span>`
          }
          ${icon}
          <span class="vn-Tree__label" title="${escapeHtml(path)}">
            ${escapeHtml(e.type === "file" ? baseName(path) : e.title || baseName(path))}
          </span>
          ${meta}
          <button
            class="vn-Tree__menuBtn"
            type="button"
            data-tree-menu="${escapeHtml(path)}"
            title="More actions"
            aria-label="More actions"
          >⋯</button>
        </div>`;
      })
      .join("");
  }

  function persistTabs() {
    try {
      const payload = {
        activeTabPath: state.activeTabPath,
        tabs: state.tabs.map((tab) => ({
          path: tab.path,
          title: tab.title || baseName(tab.path),
          dirty: false,
        })),
      };
      localStorage.setItem(TABS_STORAGE_KEY, JSON.stringify(payload));
    } catch (_) {}
  }

  function restorePersistedTabs() {
    try {
      const raw = localStorage.getItem(TABS_STORAGE_KEY);
      if (!raw) return false;
      const payload = JSON.parse(raw);
      const tabs = Array.isArray(payload.tabs) ? payload.tabs : [];

      state.tabs = tabs
        .filter((tab) => tab && tab.path)
        .map((tab) => ({
          path: normalizeExplorerPath(tab.path),
          title: tab.title || baseName(tab.path),
          dirty: false,
        }));

      state.activeTabPath =
        payload.activeTabPath &&
        state.tabs.some(
          (tab) => tab.path === normalizeExplorerPath(payload.activeTabPath),
        )
          ? normalizeExplorerPath(payload.activeTabPath)
          : state.tabs.length
            ? state.tabs[0].path
            : null;

      return true;
    } catch (_) {
      state.tabs = [];
      state.activeTabPath = null;
      return false;
    }
  }

  function clearPersistedTabs() {
    try {
      localStorage.setItem(
        TABS_STORAGE_KEY,
        JSON.stringify({ activeTabPath: null, tabs: [] }),
      );
    } catch (_) {}
  }

  /* ==========================================================
   * Tabs
   * ======================================================== */
  function activeTab() {
    return state.tabs.find((t) => t.path === state.activeTabPath) || null;
  }

  function openTab(path, title) {
    if (!path) return;
    const normalized = normalizeExplorerPath(path);
    let tab = state.tabs.find((t) => t.path === normalized);
    if (!tab) {
      tab = {
        path: normalized,
        title: title || baseName(normalized),
        dirty: false,
      };
      state.tabs.push(tab);
    } else if (title) {
      tab.title = title;
    }
    state.activeTabPath = normalized;
    persistTabs();
    renderOpenTabs();
    renderTabsBar();
  }

  function syncActiveTab(doc) {
    if (!doc || !doc.path) return;
    const title = documentDisplayTitle(doc);
    if (state.activeTabPath !== doc.path) {
      openTab(doc.path, title);
    } else {
      const tab = activeTab();
      if (tab) tab.title = title;
    }
  }

  async function switchTab(path) {
    if (!path || path === state.activeTabPath) return;
    if (isDirty()) {
      const proceed = await showModalConfirm({
        title: "Unsaved changes",
        body: `“${escapeHtml(activeTab().title)}” has unsaved changes. Switch anyway? Your unsaved edits in the editor may be replaced.`,
        confirm: "Switch tab",
        danger: true,
      });
      if (!proceed) return;
    }
    state.activeTabPath = path;
    persistTabs();
    await openNotePath(path);
  }

  async function closeTab(path) {
    const normalized = normalizeExplorerPath(path);
    const idx = state.tabs.findIndex((t) => t.path === normalized);
    if (idx < 0) return;

    const tab = state.tabs[idx];
    if (tab.dirty) {
      const proceed = await showModalConfirm({
        title: "Close tab",
        body: `“${escapeHtml(tab.title)}” has unsaved changes. Close without saving?`,
        confirm: "Close tab",
        danger: true,
      });
      if (!proceed) return;
    }

    state.tabs.splice(idx, 1);

    if (state.activeTabPath === normalized) {
      const next = state.tabs[idx] || state.tabs[idx - 1] || null;
      if (next) {
        state.activeTabPath = next.path;
        persistTabs();
        await openNotePath(next.path);
      } else {
        state.activeTabPath = null;
        clearPersistedTabs();
        clearEditorNoOpenNote();
        return;
      }
    } else {
      persistTabs();
    }

    renderOpenTabs();
    renderTabsBar();
  }

  function tabDot(tab) {
    return tab.dirty
      ? '<span class="vn-Tab__dot" title="Unsaved changes"></span>'
      : "";
  }

  function renderTabsBar() {
    const bar = $(sel.tabsBar);
    if (!bar) return;
    if (!state.tabs.length) {
      bar.innerHTML = `<div class="vn-TabsBar__empty">No open notes</div>`;
      return;
    }
    bar.innerHTML = state.tabs
      .map((t) => {
        const active = t.path === state.activeTabPath ? " is-active" : "";
        return `<div class="vn-Tab${active}" data-tab-path="${escapeHtml(t.path)}" title="${escapeHtml(t.path)}">
            ${tabDot(t)}
            <span class="vn-Tab__label">${escapeHtml(t.title)}</span>
            <button class="vn-Tab__close" type="button" data-tab-close="${escapeHtml(t.path)}" aria-label="Close tab">×</button>
          </div>`;
      })
      .join("");
  }

  /* ==========================================================
   * Diagnostics / Problems
   *
   * Diagnostics are a pure projection of NoteResult outputs. The runtime
   * already classifies outputs (compiler_error, runtime_error, error, stderr,
   * hint); we only read those, attach the owning cell, and render them.
   *
   * Severity mapping:
   *   compiler_error / runtime_error / error / stderr -> "error"
   *   hint                                            -> "hint"
   *   everything else (stdout, debug, raw_log, text, html, ...) -> ignored
   * ======================================================== */
  const DIAGNOSTIC_SEVERITY = {
    compiler_error: "error",
    runtime_error: "error",
    error: "error",
    stderr: "error",
    hint: "hint",
  };

  let diagnosticSeq = 0;

  function severityForOutputKind(kind) {
    return DIAGNOSTIC_SEVERITY[normalizeKind(kind)] || null;
  }

  function cellLabelFor(cellId) {
    const cell = findCell(cellId);
    if (cell && cell.title) return String(cell.title);
    const idx = cellIndex(cellId);
    if (idx >= 0) return `Cell ${idx + 1}`;
    return String(cellId || "cell");
  }

  // Build diagnostics from a single NoteResult-like object for a known cell.
  function diagnosticsFromResult(result, cellId) {
    const outputs = Array.isArray(result?.outputs) ? result.outputs : [];
    const items = [];
    for (const out of outputs) {
      const severity = severityForOutputKind(out.kind);
      if (!severity) continue;
      const message = String(out.content ?? "").trim();
      if (!message) continue;
      items.push({
        id: `diag-${++diagnosticSeq}`,
        cellId: String(cellId),
        cellLabel: cellLabelFor(cellId),
        severity,
        kind: normalizeKind(out.kind),
        message,
        ts: Date.now(),
      });
    }
    return items;
  }

  // Replace diagnostics for a single cell (used after running one cell).
  function setCellDiagnostics(cellId, result) {
    const id = String(cellId);
    state.diagnostics.items = state.diagnostics.items.filter(
      (d) => d.cellId !== id,
    );
    const fresh = diagnosticsFromResult(result, id);
    state.diagnostics.items.push(...fresh);
    recomputeDiagnosticsState();
    renderProblems();
  }

  // Rebuild all diagnostics from a run-all kernel result. results[] follows
  // the order of executable cells, so we zip against the document skipping
  // non-executable cells.
  function setRunAllDiagnostics(runResult) {
    const results = Array.isArray(runResult?.results) ? runResult.results : [];
    const executable = cells().filter((c) => isCodeKind(c.kind));
    state.diagnostics.items = [];
    const n = Math.min(results.length, executable.length);
    for (let i = 0; i < n; i++) {
      const cell = executable[i];
      const items = diagnosticsFromResult(results[i], cell.id);
      state.diagnostics.items.push(...items);
    }
    recomputeDiagnosticsState();
    renderProblems();
  }

  function clearDiagnostics() {
    state.diagnostics.items = [];
    state.diagnostics.status = "idle";
    state.diagnostics.byCell = new Map();
    renderProblems();
    refreshCellProblemBadges();
  }

  function setDiagnosticsRunning() {
    state.diagnostics.status = "running";
    renderProblems();
  }

  function recomputeDiagnosticsState() {
    // Drop diagnostics whose cell no longer exists.
    state.diagnostics.items = state.diagnostics.items.filter(
      (d) => findCell(d.cellId) !== null,
    );

    const byCell = new Map();
    let errorCount = 0;
    for (const d of state.diagnostics.items) {
      if (d.severity === "error") {
        errorCount++;
        byCell.set(d.cellId, (byCell.get(d.cellId) || 0) + 1);
      }
    }
    state.diagnostics.byCell = byCell;
    state.diagnostics.status = errorCount > 0 ? "failed" : "success";

    refreshCellProblemBadges();
  }

  function errorDiagnosticCount() {
    return state.diagnostics.items.filter((d) => d.severity === "error").length;
  }

  const PROBLEM_ICONS = {
    error:
      '<svg viewBox="0 0 24 24" class="vn-Problem__icon" aria-hidden="true"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.7"/><path d="M9 9l6 6M15 9l-6 6" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/></svg>',
    hint: '<svg viewBox="0 0 24 24" class="vn-Problem__icon" aria-hidden="true"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.7"/><path d="M12 11v5" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/><circle cx="12" cy="7.8" r="1" fill="currentColor"/></svg>',
  };

  function firstMessageLine(message) {
    const line = String(message || "").split(/\r?\n/)[0] || "";
    return line.length > 160 ? `${line.slice(0, 159)}…` : line;
  }

  function renderProblems() {
    const errorCount = errorDiagnosticCount();
    const total = state.diagnostics.items.length;
    const status = state.diagnostics.status;

    // Panel header count + status-bar count + activity badge.
    setText(sel.problemsCount, String(errorCount));
    setText(sel.statusProblemsCount, String(errorCount));

    const badge = $(sel.problemsBadge);
    if (badge) {
      if (errorCount > 0) {
        badge.hidden = false;
        badge.textContent = String(errorCount > 99 ? "99+" : errorCount);
      } else {
        badge.hidden = true;
      }
    }

    const statusBtn = $(sel.statusProblems);
    if (statusBtn) {
      statusBtn.classList.toggle("has-errors", errorCount > 0);
    }

    // Summary line.
    const summary = $(sel.problemsSummary);
    const summaryText = $(sel.problemsSummaryText);
    if (summary) summary.setAttribute("data-state", status);
    if (summaryText) {
      if (status === "running") {
        summaryText.textContent = "Running…";
      } else if (errorCount > 0) {
        summaryText.textContent =
          errorCount === 1 ? "1 problem found" : `${errorCount} problems found`;
      } else if (status === "success") {
        summaryText.textContent = "No problems — last run succeeded";
      } else {
        summaryText.textContent = "No problems detected";
      }
    }

    // List body.
    const listEl = $(sel.problemsList);
    if (!listEl) return;

    if (status === "running" && !total) {
      listEl.innerHTML = `
        <div class="vn-Problems__loading">
          <span class="vn-spinner" aria-hidden="true"></span>
          Running cells…
        </div>`;
      return;
    }

    if (!total) {
      listEl.innerHTML = `
        <p class="vn-Tree__empty">
          No problems detected. Run a C++ or Reply cell to see compiler
          and runtime diagnostics here.
        </p>`;
      return;
    }

    // Group items by cell, errors first within each group.
    const groups = new Map();
    for (const d of state.diagnostics.items) {
      if (!groups.has(d.cellId)) groups.set(d.cellId, []);
      groups.get(d.cellId).push(d);
    }

    const runningClass = status === "running" ? " is-stale" : "";

    const rows = [];
    for (const [cellId, items] of groups) {
      items.sort((a, b) => {
        if (a.severity !== b.severity) return a.severity === "error" ? -1 : 1;
        return a.ts - b.ts;
      });
      const label = cellLabelFor(cellId);
      rows.push(
        `<div class="vn-Problems__group${runningClass}">
           <button class="vn-Problems__groupHead" type="button" data-problem-goto="${escapeHtml(cellId)}" title="Go to ${escapeHtml(label)}">
             <svg viewBox="0 0 24 24" class="vn-Problems__cellIcon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3v6h6"/></svg>
             <span class="vn-Problems__cellName">${escapeHtml(label)}</span>
             <span class="vn-Problems__cellCount">${items.length}</span>
           </button>
           ${items
             .map(
               (d) => `
             <button
               class="vn-Problem vn-Problem--${escapeHtml(d.severity)}"
               type="button"
               data-problem-goto="${escapeHtml(d.cellId)}"
               title="${escapeHtml(firstMessageLine(d.message))}"
             >
               ${PROBLEM_ICONS[d.severity] || PROBLEM_ICONS.error}
               <span class="vn-Problem__body">
                 <span class="vn-Problem__message">${escapeHtml(firstMessageLine(d.message))}</span>
                 <span class="vn-Problem__kind">${escapeHtml(d.kind.replace(/_/g, " "))}</span>
               </span>
             </button>`,
             )
             .join("")}
         </div>`,
      );
    }

    listEl.innerHTML = rows.join("");
  }

  // Small red dot on cells that currently have error diagnostics.
  function refreshCellProblemBadges() {
    for (const el of $all(".vn-Cell")) {
      const id = el.dataset.cellId;
      const hasError = state.diagnostics.byCell.get(id) > 0;
      el.classList.toggle("has-problem", !!hasError);
    }
  }

  // Navigate from a diagnostic to its cell.
  function gotoCellFromDiagnostic(cellId) {
    const id = String(cellId);

    if (!findCell(id)) {
      setMessage("That cell no longer exists.", "warning");
      return;
    }

    selectCell(id, { edit: false });

    const el = cellElById(id);

    if (el && typeof el.scrollIntoView === "function") {
      el.scrollIntoView({ block: "center", behavior: "smooth" });
    }
  }

  // Compatibility shim: older call sites invoke renderOpenTabs() after tab
  // changes. The Open Tabs panel is gone (folded into the main tab bar), so
  // this now only refreshes the tab bar. Kept as a named function so we don't
  // have to touch every call site.
  function renderOpenTabs() {
    renderTabsBar();
  }

  /* ==========================================================
   * Activity bar (Explorer / Problems)
   * ======================================================== */
  function renderActivityBar() {
    for (const b of $all("[data-activity]")) {
      const activity = b.dataset.activity;
      b.classList.toggle(
        "is-active",
        !state.sidebarCollapsed && state.activePanel === activity,
      );
    }
  }

  function setPanel(panel) {
    state.activePanel = panel;
    state.sidebarCollapsed = false;
    for (const p of $all("[data-panel]")) {
      p.hidden = p.dataset.panel !== panel;
    }
    app.classList.remove("is-sidebar-collapsed");
    renderActivityBar();
  }

  /* ==========================================================
   * Sidebar: collapse + resize
   * ======================================================== */
  function applySidebarWidth(w) {
    state.sidebarWidth = Math.max(
      MIN_SIDEBAR_WIDTH,
      Math.min(MAX_SIDEBAR_WIDTH, w),
    );
    document.documentElement.style.setProperty(
      "--vn-sidebar-w",
      `${state.sidebarWidth}px`,
    );
  }
  function toggleExplorerSidebar() {
    if (state.sidebarCollapsed) {
      setPanel("explorer");
      return;
    }
    if (state.activePanel === "explorer") {
      state.sidebarCollapsed = true;
      app.classList.add("is-sidebar-collapsed");
      renderActivityBar();
      return;
    }
    setPanel("explorer");
  }
  // Alias kept for the keyboard shortcut and menu command.
  function toggleSidebar(forceCollapsed) {
    if (forceCollapsed === true) {
      state.sidebarCollapsed = true;
      app.classList.add("is-sidebar-collapsed");
      renderActivityBar();
      return;
    }
    toggleExplorerSidebar();
  }
  function toggleFocus() {
    state.focusMode = !state.focusMode;
    app.classList.toggle("is-focus", state.focusMode);
    setMessage(state.focusMode ? "Focus mode on." : "Focus mode off.", "info");
  }

  function bindSidebarResize() {
    const resizer = $(sel.sidebarResizer);
    if (!resizer) return;
    let startX = 0,
      startW = 0,
      dragging = false;

    const onMove = (e) => {
      if (!dragging) return;
      const x = e.touches ? e.touches[0].clientX : e.clientX;
      applySidebarWidth(startW + (x - startX));
    };
    const onUp = () => {
      if (!dragging) return;
      dragging = false;
      app.classList.remove("is-resizing");
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
      window.removeEventListener("touchmove", onMove);
      window.removeEventListener("touchend", onUp);
    };
    const onDown = (e) => {
      dragging = true;
      startX = e.touches ? e.touches[0].clientX : e.clientX;
      startW = state.sidebarWidth;
      app.classList.add("is-resizing");
      window.addEventListener("mousemove", onMove);
      window.addEventListener("mouseup", onUp);
      window.addEventListener("touchmove", onMove, { passive: false });
      window.addEventListener("touchend", onUp);
      e.preventDefault();
    };

    resizer.addEventListener("mousedown", onDown);
    resizer.addEventListener("touchstart", onDown, { passive: false });
    resizer.addEventListener("dblclick", () =>
      applySidebarWidth(DEFAULT_SIDEBAR_WIDTH),
    );
    resizer.addEventListener("keydown", (e) => {
      if (e.key === "ArrowLeft") {
        applySidebarWidth(state.sidebarWidth - 16);
        e.preventDefault();
      }
      if (e.key === "ArrowRight") {
        applySidebarWidth(state.sidebarWidth + 16);
        e.preventDefault();
      }
    });
  }

  /* ==========================================================
   * Header dropdown menus
   * ======================================================== */
  function closeAllMenus() {
    for (const m of $all(".vn-Menu")) {
      m.classList.remove("is-open");
      const d = $(".vn-Menu__dropdown", m);
      if (d) d.hidden = true;
    }
  }

  function bindMenus() {
    const bar = $("[data-menubar]");
    if (!bar) return;

    bar.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-menu-button]");
      if (btn) {
        const menu = btn.closest(".vn-Menu");
        const isOpen = menu.classList.contains("is-open");
        closeAllMenus();
        if (!isOpen) {
          menu.classList.add("is-open");
          const d = $(".vn-Menu__dropdown", menu);
          if (d) d.hidden = false;
        }
        e.stopPropagation();
        return;
      }
      const item = e.target.closest("[data-command]");
      if (item) {
        closeAllMenus();
        runCommand(item.getAttribute("data-command"));
      }
    });

    bar.addEventListener("mouseover", (e) => {
      const anyOpen = bar.querySelector(".vn-Menu.is-open");
      if (!anyOpen) return;
      const btn = e.target.closest("[data-menu-button]");
      if (!btn) return;
      const menu = btn.closest(".vn-Menu");
      if (menu.classList.contains("is-open")) return;
      closeAllMenus();
      menu.classList.add("is-open");
      const d = $(".vn-Menu__dropdown", menu);
      if (d) d.hidden = false;
    });

    document.addEventListener("click", () => closeAllMenus());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeAllMenus();
    });
  }

  /* ==========================================================
   * Command registry
   * ======================================================== */
  function currentToolbarKind() {
    const button = $(sel.toolbarKind);
    const value = button ? button.dataset.kind : "cpp";
    const kind = normalizeKind(value);

    return ["cpp", "reply", "markdown", "html"].includes(kind) ? kind : "cpp";
  }
  function targetId() {
    return state.selectedId;
  }

  const COMMANDS = {
    "new-note": { label: "New note", run: () => newNote() },
    "open-note": { label: "Open note", run: () => openNote() },
    "new-folder": { label: "New folder", run: () => newFolder() },
    save: { label: "Save note", hint: "⌘S", run: () => saveNote() },
    reload: { label: "Reload from disk", run: () => loadDocument() },
    "add-cpp": { label: "Add C++ cell", run: () => addCell("cpp") },
    "add-reply": { label: "Add Reply cell", run: () => addCell("reply") },
    "add-markdown": {
      label: "Add Markdown cell",
      run: () => addCell("markdown"),
    },
    "add-html": { label: "Add HTML cell", run: () => addCell("html") },
    "insert-below": {
      label: "Insert cell below",
      hint: "B",
      run: () => addCell(currentToolbarKind(), { afterId: targetId() }),
    },
    "run-cell": {
      label: "Run selected cell",
      hint: "⌘↵",
      run: () => targetId() && runCellById(targetId()),
    },
    "run-advance": {
      label: "Run and advance",
      hint: "⇧↵",
      run: async () => {
        if (targetId()) {
          await runCellById(targetId());
          selectAdjacent(1);
        }
      },
    },
    "run-all": { label: "Run all cells", run: () => runAll() },
    "cut-cell": {
      label: "Delete selected cell",
      hint: "D D",
      run: () => targetId() && deleteCellById(targetId()),
    },
    duplicate: {
      label: "Duplicate selected cell",
      run: () => targetId() && duplicateCell(targetId()),
    },
    "move-up": {
      label: "Move cell up",
      run: () => targetId() && moveCellById(targetId(), "up"),
    },
    "move-down": {
      label: "Move cell down",
      run: () => targetId() && moveCellById(targetId(), "down"),
    },
    "to-cpp": {
      label: "Change cell to C++",
      hint: "Y",
      run: () => targetId() && changeKind(targetId(), "cpp"),
    },
    "to-markdown": {
      label: "Change cell to Markdown",
      hint: "M",
      run: () => targetId() && changeKind(targetId(), "markdown"),
    },
    "to-reply": {
      label: "Change cell to Reply",
      hint: "R",
      run: () => targetId() && changeKind(targetId(), "reply"),
    },
    "to-html": {
      label: "Change cell to HTML",
      run: () => targetId() && changeKind(targetId(), "html"),
    },
    "clear-cell": {
      label: "Clear selected output",
      run: () => targetId() && clearCellOutput(targetId()),
    },
    "clear-all": { label: "Clear all outputs", run: () => clearAllOutputs() },
    restart: { label: "Restart kernel", run: () => restartKernel(false) },
    "restart-run": {
      label: "Restart kernel and run all",
      run: () => restartKernel(true),
    },
    "toggle-sidebar": {
      label: "Toggle Explorer",
      hint: "⌘B",
      run: () => toggleExplorerSidebar(),
    },
    "toggle-focus": { label: "Toggle focus mode", run: () => toggleFocus() },
    "show-explorer": {
      label: "Show Explorer",
      run: () => setPanel("explorer"),
    },
    "show-problems": {
      label: "Show Problems",
      run: () => setPanel("problems"),
    },
    refresh: { label: "Refresh explorer", run: () => refreshExplorer() },
    shortcuts: {
      label: "Keyboard shortcuts",
      hint: "?",
      run: () => showShortcuts(),
    },
    about: { label: "About Vix Note", run: () => showAbout() },
  };

  function runCommand(name) {
    const cmd = COMMANDS[name];
    if (cmd && typeof cmd.run === "function") cmd.run();
  }

  /* ==========================================================
   * Modal system (forms + confirm + info)
   * Kept for Open note, confirmations, shortcuts and about only.
   * ======================================================== */
  function modalEls() {
    return {
      root: $("[data-modal]"),
      title: $("[data-modal-title]"),
      body: $("[data-modal-body]"),
      foot: $("[data-modal-foot]"),
    };
  }

  function openModalShell(title) {
    const m = modalEls();
    if (!m.root) return null;
    if (m.title) m.title.textContent = title;
    m.root.hidden = false;
    return m;
  }
  function closeModal() {
    const m = modalEls();
    if (m.root) m.root.hidden = true;
    if (m.body) m.body.innerHTML = "";
    if (m.foot) m.foot.innerHTML = "";
  }

  function showModalForm({ title, fields, confirm = "OK" }) {
    return new Promise((resolve) => {
      const m = openModalShell(title);
      if (!m) {
        resolve(null);
        return;
      }
      m.body.innerHTML = `<form class="vn-Form" data-modal-form>
  ${fields
    .map((f) => {
      const hint = f.hint
        ? `<span class="vn-Form__hint">${escapeHtml(f.hint)}</span>`
        : "";
      if (f.type === "select") {
        return `<label class="vn-Form__field">
          <span class="vn-Form__label">${escapeHtml(f.label)}</span>
          <select class="vn-Form__input" name="${escapeHtml(f.name)}">
            ${(f.options || [])
              .map((option) => {
                const value = String(option.value ?? option);
                const label = String(option.label ?? option);
                const selected =
                  value === String(f.value || "") ? " selected" : "";
                return `<option value="${escapeHtml(value)}"${selected}>${escapeHtml(label)}</option>`;
              })
              .join("")}
          </select>
          ${hint}
        </label>`;
      }
      return `<label class="vn-Form__field">
        <span class="vn-Form__label">${escapeHtml(f.label)}</span>
        <input class="vn-Form__input" name="${escapeHtml(f.name)}" value="${escapeHtml(f.value || "")}" placeholder="${escapeHtml(f.placeholder || "")}" autocomplete="off" spellcheck="false" />
        ${hint}
      </label>`;
    })
    .join("")}
</form>`;
      m.foot.innerHTML = `
        <button type="button" class="vn-Btn vn-Btn--ghost" data-modal-cancel>Cancel</button>
        <button type="button" class="vn-Btn vn-Btn--primary" data-modal-ok>${escapeHtml(confirm)}</button>`;

      const form = $("[data-modal-form]", m.body);
      const firstInput =
        $('input[name="path"]', form) || $("input, textarea, select", form);
      if (firstInput) {
        firstInput.focus();
        if (typeof firstInput.select === "function") firstInput.select();
      }

      const done = (val) => {
        cleanup();
        closeModal();
        resolve(val);
      };
      const collect = () => {
        const data = {};
        for (const input of $all("input, select", form)) {
          data[input.name] = input.value;
        }
        return data;
      };
      const onOk = () => done(collect());
      const onCancel = () => done(null);
      const onKey = (e) => {
        if (e.key === "Enter") {
          e.preventDefault();
          onOk();
        } else if (e.key === "Escape") {
          e.preventDefault();
          onCancel();
        }
      };
      function cleanup() {
        $("[data-modal-ok]", m.foot)?.removeEventListener("click", onOk);
        for (const c of $all("[data-modal-cancel], [data-modal-close]"))
          c.removeEventListener("click", onCancel);
        form.removeEventListener("keydown", onKey);
      }
      $("[data-modal-ok]", m.foot).addEventListener("click", onOk);
      for (const c of $all("[data-modal-cancel], [data-modal-close]"))
        c.addEventListener("click", onCancel);
      form.addEventListener("keydown", onKey);
    });
  }

  function showModalConfirm({ title, body, confirm = "OK", danger = false }) {
    return new Promise((resolve) => {
      const m = openModalShell(title);
      if (!m) {
        resolve(false);
        return;
      }
      m.body.innerHTML = `<p class="vn-Modal__text">${body}</p>`;
      m.foot.innerHTML = `
        <button type="button" class="vn-Btn vn-Btn--ghost" data-modal-cancel>Cancel</button>
        <button type="button" class="vn-Btn ${danger ? "vn-Btn--danger" : "vn-Btn--primary"}" data-modal-ok>${escapeHtml(confirm)}</button>`;
      const okBtn = $("[data-modal-ok]", m.foot);
      okBtn.focus();
      const done = (val) => {
        cleanup();
        closeModal();
        resolve(val);
      };
      const onOk = () => done(true);
      const onCancel = () => done(false);
      function cleanup() {
        okBtn.removeEventListener("click", onOk);
        for (const c of $all("[data-modal-cancel], [data-modal-close]"))
          c.removeEventListener("click", onCancel);
      }
      okBtn.addEventListener("click", onOk);
      for (const c of $all("[data-modal-cancel], [data-modal-close]"))
        c.addEventListener("click", onCancel);
    });
  }

  function showModalInfo(title, html) {
    const m = openModalShell(title);
    if (!m) return;
    m.body.innerHTML = html;
    m.foot.innerHTML = `<button type="button" class="vn-Btn vn-Btn--primary" data-modal-cancel>Close</button>`;
    const close = () => closeModal();
    for (const c of $all("[data-modal-cancel], [data-modal-close]"))
      c.addEventListener("click", close, { once: true });
  }

  function showShortcuts() {
    const rows = [
      ["Command mode", ""],
      ["Run cell", "Ctrl/⌘ + Enter"],
      ["Run and advance", "Shift + Enter"],
      ["Enter edit mode", "Enter"],
      ["Select cell above / below", "↑ / ↓ · K / J"],
      ["Insert cell above / below", "A / B"],
      ["Delete cell", "D D"],
      ["Change to Markdown / C++ / Reply", "M / Y / R"],
      ["Edit mode", ""],
      ["Leave edit mode", "Esc"],
      ["Indent / Outdent", "Tab · Shift + Tab"],
      ["Toggle comment", "Ctrl/⌘ + /"],
      ["Move line", "Alt + ↑ · Alt + ↓"],
      ["Run cell", "Ctrl/⌘ + Enter"],
      ["Run and advance", "Shift + Enter"],
      ["Explorer", ""],
      ["New note (inline)", "Ctrl/⌘ + N"],
      ["New folder (inline)", "Ctrl/⌘ + Shift + N"],
      ["Rename selected", "F2"],
      ["Commit / cancel inline name", "Enter · Esc"],
      ["Global", ""],
      ["Save note", "Ctrl/⌘ + S"],
      ["Toggle sidebar", "Ctrl/⌘ + B"],
      ["Show this dialog", "?"],
    ];
    const html = `<div class="vn-Shortcuts">${rows
      .map(([label, keys]) =>
        keys === ""
          ? `<div class="vn-Shortcuts__group">${escapeHtml(label)}</div>`
          : `<div>${escapeHtml(label)}</div><div>${keys
              .split(" · ")
              .map((k) =>
                k
                  .split(" + ")
                  .map((p) => `<kbd>${escapeHtml(p)}</kbd>`)
                  .join(" + "),
              )
              .join(" · ")}</div>`,
      )
      .join("")}</div>`;
    showModalInfo("Keyboard shortcuts", html);
  }
  function showAbout() {
    showModalInfo(
      "About Vix Note",
      `<div class="vn-About">
      <div class="vn-About__hero">
        <div class="vn-About__logo" aria-hidden="true">V</div>
        <div>
          <h2>Vix Note</h2>
          <p>Visual executable notes for learning C++ and Vix.cpp faster.</p>
        </div>
      </div>

      <p>
        Vix Note is a local notebook workspace for writing notes, running C++
        examples, testing Vix.cpp code, and keeping the result in the same
        document.
      </p>

      <div class="vn-About__section">
        <h3>What you can do</h3>
        <ul>
          <li>Write explanations with Markdown cells.</li>
          <li>Run C++ cells through <code>vix run</code>.</li>
          <li>Use Reply cells for small scripts and quick tests.</li>
          <li>Add HTML cells when you need rendered content.</li>
          <li>See outputs, errors, and diagnostics directly in the note.</li>
        </ul>
      </div>

      <div class="vn-About__section">
        <h3>Workspace</h3>
        <ul>
          <li>Create notes and folders from the explorer.</li>
          <li>Open multiple notes with tabs.</li>
          <li>Use the Problems panel to find failed cells faster.</li>
          <li>Run notes in project context when local headers or dependencies are needed.</li>
        </ul>
      </div>

      <p class="vn-About__meta">
        Vix Note is part of the Vix.cpp ecosystem.
        Built for local C++ learning and development.
        MIT License. Copyright 2026, Gaspard Kirira.
      </p>
    </div>`,
    );
  }

  /* ==========================================================
   * Context menu (custom, not the browser's)
   * ======================================================== */
  let contextMenuEl = null;
  function closeContextMenu() {
    if (contextMenuEl) {
      contextMenuEl.remove();
      contextMenuEl = null;
    }
  }
  function showContextMenu(x, y, items) {
    closeContextMenu();
    const menu = document.createElement("div");
    menu.className = "vn-Context";
    menu.innerHTML = items
      .map((it) =>
        it.sep
          ? `<div class="vn-Context__sep"></div>`
          : `<button type="button" class="vn-Context__item${it.danger ? " is-danger" : ""}${it.disabled ? " is-disabled" : ""}" data-ctx="${escapeHtml(it.id)}">${escapeHtml(it.label)}</button>`,
      )
      .join("");
    document.body.appendChild(menu);
    contextMenuEl = menu;

    const rect = menu.getBoundingClientRect();
    const px = Math.min(x, window.innerWidth - rect.width - 8);
    const py = Math.min(y, window.innerHeight - rect.height - 8);
    menu.style.left = `${Math.max(8, px)}px`;
    menu.style.top = `${Math.max(8, py)}px`;

    menu.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-ctx]");
      if (!btn || btn.classList.contains("is-disabled")) return;
      const id = btn.getAttribute("data-ctx");
      closeContextMenu();
      const found = items.find((i) => i.id === id);
      if (found && found.run) found.run();
    });
  }

  function fileContextItems(path) {
    return [
      {
        id: "open",
        label: "Open",
        run: () => openNotePath(path, { silent: true }),
      },
      { sep: true },
      {
        id: "rename",
        label: "Rename…",
        run: () => startInlineRename(path, "file"),
      },
      {
        id: "delete",
        label: "Delete",
        danger: true,
        run: async () => {
          if (await confirmDelete(baseName(path), "file")) {
            await deletePath(path);
          }
        },
      },
    ];
  }
  function dirContextItems(path) {
    return [
      { id: "new-note", label: "New note", run: () => newNote(path) },
      { id: "new-folder", label: "New folder", run: () => newFolder(path) },
      { sep: true },
      {
        id: "rename",
        label: "Rename…",
        run: () => startInlineRename(path, "dir"),
      },
      {
        id: "delete",
        label: "Delete",
        danger: true,
        run: async () => {
          const ok = await showModalConfirm({
            title: "Delete folder",
            body: `Delete folder “${escapeHtml(baseName(path))}” and everything inside it from disk? This cannot be undone from Vix Note.`,
            confirm: "Delete",
            danger: true,
          });

          if (ok) {
            await deletePath(path, { recursive: true });
          }
        },
      },
    ];
  }

  /* ==========================================================
   * Wiring: header toolbar actions
   * ======================================================== */
  function bindActions() {
    document.addEventListener("click", (event) => {
      const target = event.target instanceof Element ? event.target : null;

      const kindOption = target ? target.closest("[data-kind-option]") : null;
      if (kindOption) {
        event.preventDefault();
        event.stopPropagation();

        setToolbarKind(kindOption.dataset.kindOption || "cpp", {
          applyToCell: true,
        });

        closeToolbarKindMenu();
        return;
      }

      const kindButton = target
        ? target.closest('[data-action="toolbar-kind"]')
        : null;

      if (kindButton) {
        event.preventDefault();
        event.stopPropagation();
        toggleToolbarKindMenu();
        return;
      }

      if (target && !target.closest("[data-cell-type-select]")) {
        closeToolbarKindMenu();
      }

      const t =
        event.target instanceof Element
          ? event.target.closest("[data-action]")
          : null;
      if (!t) return;
      const action = t.getAttribute("data-action");

      switch (action) {
        case "toggle-sidebar":
          toggleExplorerSidebar();
          break;
        case "save":
          saveNote();
          break;
        case "run-cell":
          if (targetId()) runCellById(targetId());
          break;
        case "run-all":
          runAll();
          break;
        case "restart":
          restartKernel(false);
          break;
        case "insert-below":
          addCell(currentToolbarKind(), { afterId: targetId() });
          break;
        case "cut-cell":
          if (targetId()) deleteCellById(targetId());
          break;
        case "duplicate":
          if (targetId()) duplicateCell(targetId());
          break;
        case "move-up":
          if (targetId()) moveCellById(targetId(), "up");
          break;
        case "move-down":
          if (targetId()) moveCellById(targetId(), "down");
          break;
        case "new-note":
          newNote(state.explorer.selectedDirPath || ".");
          break;
        case "open-note":
          openNote();
          break;
        case "new-folder":
          newFolder(state.explorer.selectedDirPath || ".");
          break;
        case "refresh":
          refreshExplorer();
          break;
        case "clear-problems":
          clearDiagnostics();
          setMessage("Problems cleared.", "info");
          break;
        case "shortcuts":
          showShortcuts();
          break;
        default:
          break;
      }
    });
  }

  /* ==========================================================
   * Wiring: activity bar
   * ======================================================== */
  function bindActivityBar() {
    for (const b of $all("[data-activity]")) {
      b.addEventListener("click", () => {
        const activity = b.dataset.activity;
        if (activity === "explorer") {
          toggleExplorerSidebar();
          return;
        }
        setPanel(activity);
      });
    }
  }

  /* ==========================================================
   * Wiring: explorer + open tabs + tabs bar
   * Includes the inline-create and inline-rename input handling.
   * ======================================================== */
  function bindExplorer() {
    const listEl = $(sel.explorerList);
    if (listEl) {
      // --- Inline create input ---
      listEl.addEventListener("keydown", (e) => {
        const input = e.target.closest("[data-tree-input]");
        if (input) {
          if (e.key === "Enter") {
            e.preventDefault();
            commitInlineCreate(input.value);
          } else if (e.key === "Escape") {
            e.preventDefault();
            cancelInlineCreate();
          }
          return;
        }

        const renameInput = e.target.closest("[data-tree-rename-input]");
        if (renameInput) {
          if (e.key === "Enter") {
            e.preventDefault();
            commitInlineRename(renameInput.value);
          } else if (e.key === "Escape") {
            e.preventDefault();
            cancelInlineRename();
          }
          return;
        }

        // Row-level keyboard (Enter to open/toggle, F2 to rename).
        const row = e.target.closest("[data-tree-path]");

        if (!row) {
          /*
           * Clicking empty space in the explorer means:
           * create next files/folders at workspace root.
           *
           * This matches the expected VS Code-like behavior:
           * selected folder = target folder
           * empty explorer area = root target
           */
          state.explorer.selectedDirPath = ".";
          state.explorer.currentPath = ".";
          renderExplorer();
          return;
        }

        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");

        if (e.key === "F2") {
          e.preventDefault();
          startInlineRename(path, type === "dir" ? "dir" : "file");
          return;
        }
        if (e.key === "Enter") {
          if (type === "dir") {
            toggleDirectory(path);
          } else if (type === "file") {
            openFileRowIfAllowed(path);
          }
        }
      });

      // Commit-on-blur for the inline inputs (VS Code commits on blur).
      listEl.addEventListener(
        "focusout",
        (e) => {
          const input = e.target.closest("[data-tree-input]");
          if (input && state.explorer.draft) {
            // Defer so an Enter/Escape keydown handler runs first.
            setTimeout(() => {
              if (state.explorer.draft) commitInlineCreate(input.value);
            }, 0);
            return;
          }
          const renameInput = e.target.closest("[data-tree-rename-input]");
          if (renameInput && state.explorer.rename) {
            setTimeout(() => {
              if (state.explorer.rename) commitInlineRename(renameInput.value);
            }, 0);
          }
        },
        true,
      );

      listEl.addEventListener("click", (e) => {
        // Clicks inside an inline input must not bubble to row handlers.
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          return;
        }

        const menuBtn = e.target.closest("[data-tree-menu]");
        if (menuBtn) {
          e.stopPropagation();
          const path = menuBtn.getAttribute("data-tree-menu");
          const entry = state.explorer.entries.get(path);
          const rect = menuBtn.getBoundingClientRect();
          showContextMenu(
            rect.left,
            rect.bottom,
            entry && entry.type === "dir"
              ? dirContextItems(path)
              : fileContextItems(path),
          );
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");

        // Track selection so New note / New folder target the right place.
        if (type === "dir") {
          state.explorer.selectedDirPath = normalizeExplorerPath(path);
          toggleDirectory(path);
          return;
        }
        if (type === "file") {
          state.explorer.selectedDirPath = parentPath(path);
          openFileRowIfAllowed(path);
        }
      });

      listEl.addEventListener("contextmenu", (e) => {
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        e.preventDefault();
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");
        showContextMenu(
          e.clientX,
          e.clientY,
          type === "dir" ? dirContextItems(path) : fileContextItems(path),
        );
      });
    }

    const search = $(sel.explorerSearch);
    if (search) search.addEventListener("input", renderExplorer);

    // Problems panel: clicking a diagnostic (or its group header) jumps to
    // the owning cell.
    const problemsList = $(sel.problemsList);
    if (problemsList) {
      problemsList.addEventListener("click", (e) => {
        const goto = e.target.closest("[data-problem-goto]");
        if (goto) {
          gotoCellFromDiagnostic(goto.getAttribute("data-problem-goto"));
        }
      });
    }

    const tabsBar = $(sel.tabsBar);
    if (tabsBar) {
      tabsBar.addEventListener("click", (e) => {
        const close = e.target.closest("[data-tab-close]");
        if (close) {
          e.stopPropagation();
          closeTab(close.getAttribute("data-tab-close"));
          return;
        }
        const tab = e.target.closest("[data-tab-path]");
        if (tab) switchTab(tab.getAttribute("data-tab-path"));
      });
    }

    document.addEventListener("click", () => closeContextMenu());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeContextMenu();
    });
    window.addEventListener("blur", closeContextMenu);
    window.addEventListener("resize", closeContextMenu);
  }

  function openFileRowIfAllowed(path) {
    const entry = state.explorer.entries.get(path);
    if (entry && entry.openable === false && !path.endsWith(".vixnote")) {
      setMessage("Only .vixnote files can be opened in Vix Note.", "warning");
      return;
    }
    openNotePath(path, { silent: true });
  }

  /* ==========================================================
   * Wiring: cell interactions
   * ======================================================== */
  function bindCellInteractions() {
    const container = $(sel.cells);
    if (!container) return;

    container.addEventListener("click", (event) => {
      const target = event.target;
      if (!(target instanceof Element)) return;

      const insertBtn = target.closest("[data-insert-after]");
      if (insertBtn) {
        addCell(currentToolbarKind(), {
          afterId: insertBtn.getAttribute("data-insert-after"),
        });
        return;
      }
      const actionBtn = target.closest("[data-cell-action]");
      const cellEl = target.closest(".vn-Cell");
      if (!cellEl) return;
      const id = cellEl.dataset.cellId;

      if (actionBtn) {
        const a = actionBtn.getAttribute("data-cell-action");
        if (a === "run") return void runCellById(id);
        if (a === "edit") return void toggleCellEdit(id);
        if (a === "duplicate") return void duplicateCell(id);
        if (a === "up") return void moveCellById(id, "up");
        if (a === "down") return void moveCellById(id, "down");
        if (a === "delete") return void deleteCellById(id);
        if (a === "select") return void selectCell(id, { edit: false });
      }
      const inEditor = target.closest(".vn-Editor, textarea");
      selectCell(id, { edit: !!inEditor, focus: !!inEditor });
    });

    container.addEventListener("dblclick", (event) => {
      const target = event.target;
      if (!(target instanceof Element)) return;
      if (!target.closest("[data-rendered]")) return;
      const cellEl = target.closest(".vn-Cell");
      if (cellEl) selectCell(cellEl.dataset.cellId, { edit: true });
    });

    container.addEventListener("input", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      markTextareaChanged(ta);
    });

    container.addEventListener("click", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener("keyup", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener("select", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener(
      "scroll",
      (event) => {
        if (event.target instanceof HTMLTextAreaElement)
          syncScroll(event.target);
      },
      true,
    );

    container.addEventListener(
      "focusout",
      async (event) => {
        const ta = event.target;
        if (!(ta instanceof HTMLTextAreaElement)) return;
        if (ta.getAttribute("data-action") !== "edit-source") return;
        const cellEl = ta.closest(".vn-Cell");
        if (!cellEl) return;
        try {
          await pushCell(cellEl);
        } catch (_) {}
      },
      true,
    );
  }

  /* ==========================================================
   * Keyboard
   * ======================================================== */
  let lastDTime = 0;
  function handleDoubleD() {
    const now = Date.now();
    if (now - lastDTime < 500) {
      lastDTime = 0;
      if (state.selectedId) deleteCellById(state.selectedId);
    } else lastDTime = now;
  }
  async function insertAbove(id) {
    const idx = cellIndex(id);
    if (idx < 0) return;
    await addCell(currentToolbarKind(), { atIndex: idx });
  }

  function inlineInputActive() {
    return !!(state.explorer.draft || state.explorer.rename);
  }

  function bindKeyboard() {
    document.addEventListener("keydown", async (event) => {
      const inField =
        event.target instanceof HTMLTextAreaElement ||
        event.target instanceof HTMLInputElement;
      const inTextarea = event.target instanceof HTMLTextAreaElement;
      const meta = event.ctrlKey || event.metaKey;

      // Explorer global shortcuts: New note / New folder (VS Code-like).
      if (meta && event.key.toLowerCase() === "n") {
        // Don't hijack while typing into a textarea cell.
        if (!inTextarea) {
          event.preventDefault();
          if (event.shiftKey) newFolder();
          else newNote();
          return;
        }
      }

      // While an inline tree input is active, let it own its own keys.
      if (inlineInputActive() && inField) {
        return;
      }

      if (meta && event.key === "Enter") {
        event.preventDefault();
        if (state.selectedId) await runCellById(state.selectedId);
        if (inTextarea) enterCommandMode();
        return;
      }
      if (event.shiftKey && event.key === "Enter") {
        if (inField && !inTextarea) return;
        event.preventDefault();
        if (state.selectedId) {
          await runCellById(state.selectedId);
          selectAdjacent(1);
        }
        return;
      }
      if (meta && event.key.toLowerCase() === "s") {
        event.preventDefault();
        await saveNote();
        return;
      }
      if (meta && event.key.toLowerCase() === "b") {
        event.preventDefault();
        toggleSidebar();
        return;
      }

      if (state.editing && inTextarea) {
        const ta = event.target;

        if (event.key === "Escape") {
          event.preventDefault();
          if (state.selectedId) {
            const cellEl = cellElById(state.selectedId);
            if (cellEl) localUpdateFromDom(cellEl);
          }
          exitEditMode();
          return;
        }
        if (event.key === "Tab") {
          event.preventDefault();
          if (event.shiftKey) outdentTextarea(ta);
          else indentTextarea(ta);
          return;
        }
        if (meta && event.key === "/") {
          event.preventDefault();
          toggleCommentTextarea(ta);
          return;
        }
        if (event.altKey && event.key === "ArrowUp") {
          event.preventDefault();
          moveCurrentLine(ta, -1);
          return;
        }
        if (event.altKey && event.key === "ArrowDown") {
          event.preventDefault();
          moveCurrentLine(ta, 1);
          return;
        }
        if (meta && event.key.toLowerCase() === "s") {
          event.preventDefault();
          await saveNote();
          return;
        }
        updateLineFocus(ta);
        updateCursorStatus(ta);
        return;
      }

      if (inField) return;

      if (event.key === "?") {
        event.preventDefault();
        showShortcuts();
        return;
      }

      if (!state.selectedId) return;

      switch (event.key) {
        case "Enter":
          event.preventDefault();
          enterEditMode();
          break;
        case "ArrowUp":
        case "k":
          event.preventDefault();
          selectAdjacent(-1);
          break;
        case "ArrowDown":
        case "j":
          event.preventDefault();
          selectAdjacent(1);
          break;
        case "a":
          event.preventDefault();
          insertAbove(state.selectedId);
          break;
        case "b":
          event.preventDefault();
          addCell(currentToolbarKind(), { afterId: state.selectedId });
          break;
        case "d":
          handleDoubleD();
          break;
        case "m":
          event.preventDefault();
          changeKind(state.selectedId, "markdown");
          break;
        case "y":
          event.preventDefault();
          changeKind(state.selectedId, "cpp");
          break;
        case "r":
          event.preventDefault();
          changeKind(state.selectedId, "reply");
          break;
        default:
          break;
      }
    });
  }

  /* ==========================================================
   * Init
   * ======================================================== */
  function init() {
    applySidebarWidth(DEFAULT_SIDEBAR_WIDTH);
    if (window.matchMedia("(max-width: 900px)").matches) toggleSidebar(true);

    bindActions();
    bindActivityBar();
    bindMenus();
    bindSidebarResize();
    bindCellInteractions();
    bindExplorer();
    bindKeyboard();

    // Status-bar problems indicator opens the Problems panel.
    const statusProblems = $(sel.statusProblems);
    if (statusProblems) {
      statusProblems.addEventListener("click", () => setPanel("problems"));
    }

    for (const c of $all("[data-modal-close]"))
      c.addEventListener("click", () => closeModal());

    setPanel("explorer");
    setKernel("idle");
    renderExplorer();
    renderOpenTabs();
    renderTabsBar();
    renderProblems();
    loadDocument();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
