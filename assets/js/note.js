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
 *
 * Goals: one cell = one DOM node (no duplication), stable status bar,
 * custom modals (no native prompt()), VS Code-style activity bar +
 * explorer + open tabs, a notebook toolbar scoped to the editor zone.
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
    activePanel: "explorer", // explorer | tabs
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
    },
    tabs: [], // [{ path, title, dirty, doc? }]
    activeTabPath: null,
  };

  const DEFAULT_SIDEBAR_WIDTH = 260;
  const MIN_SIDEBAR_WIDTH = 190;
  const MAX_SIDEBAR_WIDTH = 520;
  const TABS_STORAGE_KEY = "vix-note:tabs:v1";

  const app = document.querySelector("[data-note-app]");

  const sel = {
    cells: "[data-note-cells]",
    title: "[data-note-title]",
    document: "[data-note-document]",
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
    openTabsList: "[data-open-tabs-list]",
    openTabsCount: "[data-open-tabs-count]",
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

  function suggestedNotePath(dir = null) {
    const d = new Date();

    const stamp =
      `${d.getFullYear()}${pad2(d.getMonth() + 1)}${pad2(d.getDate())}` +
      `_${pad2(d.getHours())}${pad2(d.getMinutes())}${pad2(d.getSeconds())}`;

    const folder = normalizeExplorerPath(
      dir ||
        state.explorer.selectedDirPath ||
        state.explorer.currentPath ||
        parentPath(currentDocPath()) ||
        ".",
    );

    if (!folder || folder === ".") {
      return `note_${stamp}.vixnote`;
    }

    return `${folder}/note_${stamp}.vixnote`;
  }
  function baseName(path) {
    return (
      String(path || "")
        .split(/[\\/]/)
        .pop() || String(path || "")
    );
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

    if (normalized && normalized.endsWith(".vixnote")) {
      touchExplorerEntry(normalized, "file", baseName(normalized), {
        modified: Date.now(),
        openable: true,
        extension: ".vixnote",
      });
    }

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
        buildExplorerTreeRows(path, depth + 1, rows);
      }
    }

    return rows;
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

    setText(sel.title, "No open note");
    setText(sel.document, "No open notes");
    setText(sel.cellCount, "0");
    setText(sel.execCount, "0");
    setText(sel.statusPosition, "Cell 0 of 0");
    setText(sel.statusKind, "—");
    setText(sel.statusMode, "Command");

    document.title = "Vix Note";

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
        // Editing this cell: keep its textarea, just refresh outputs.
        patchCellOutputs(prev, cell);
        prev.dataset.sig = sig;
        desired.push([insertBarOf(prev), prev]);
        existing.delete(id);
      } else {
        const nodes = buildCellNodes(cell);
        const bar = nodes[0];
        const cellEl = nodes[1];
        if (cellEl && cellEl.dataset) cellEl.dataset.sig = sig;
        // Remove the stale node (and its bar) before re-inserting fresh.
        if (prev) {
          const oldBar = insertBarOf(prev);
          if (oldBar) oldBar.remove();
          prev.remove();
          existing.delete(id);
        }
        desired.push([bar, cellEl]);
      }
    }

    // Remove any leftover nodes that are no longer in the model.
    for (const [, el] of existing) {
      const bar = insertBarOf(el);
      if (bar) bar.remove();
      el.remove();
    }

    // Re-order to match desired sequence. appendChild moves existing nodes
    // (no clone), preserving textarea + caret on reused cells.
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

    setText(sel.title, title);
    setText(sel.document, baseName(doc.path) || `${title}.vixnote`);
    setText(sel.project, projectLabel(project));
    setText(sel.cellCount, String(count));
    setText(sel.execCount, String(executedCount()));
    document.title = `${title} · Vix Note`;

    // Keep tab + explorer state in sync with the now-active document.
    if (doc.path) {
      syncActiveTab({
        ...doc,
        title,
      });

      touchExplorerEntry(doc.path, "file", baseName(doc.path), {
        openable: true,
        extension: ".vixnote",
      });

      renderExplorer();
      renderOpenTabs();
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

    if (!wrap) {
      return;
    }

    const hl = $("[data-highlight]", wrap);

    if (!hl) {
      return;
    }

    const cellEl = textarea.closest(".vn-Cell");
    const kind = cellEl ? cellEl.dataset.kind : "cpp";

    hl.innerHTML = tokenizeCode(textarea.value, kind);
  }

  function syncScroll(textarea) {
    const wrap = textarea.closest(".vn-Editor__wrap");

    if (!wrap) {
      return;
    }

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

    if (!wrap) {
      return;
    }

    const focus = $("[data-line-focus]", wrap);

    if (!focus) {
      return;
    }

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

    if (!cellEl) {
      return;
    }

    const cell = findCell(cellEl.dataset.cellId);

    if (cell) {
      cell.source = textarea.value;
    }

    setDirty(true);

    const kind = cellEl.dataset.kind;

    if (kind === "markdown") {
      const rendered = $("[data-rendered]", cellEl);

      if (rendered) {
        rendered.innerHTML = renderMarkdown(textarea.value);
      }
    } else if (kind === "html") {
      const rendered = $("[data-rendered]", cellEl);

      if (rendered) {
        rendered.innerHTML = String(textarea.value || "");
      }
    }
  }

  function selectedLineRange(textarea) {
    const value = textarea.value;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;

    let lineStart = value.lastIndexOf("\n", Math.max(0, start - 1)) + 1;
    let lineEnd = value.indexOf("\n", end);

    if (lineEnd === -1) {
      lineEnd = value.length;
    }

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

        if (line.startsWith(EDITOR_INDENT)) {
          remove = EDITOR_INDENT.length;
        } else if (line.startsWith("\t")) {
          remove = 1;
        } else if (line.startsWith(" ")) {
          remove = 1;
        }

        if (index === 0) {
          removedBeforeStart = remove;
        }

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

  function lineBoundsAtCursor(textarea) {
    const value = textarea.value;
    const cursor = textarea.selectionStart;

    const start = value.lastIndexOf("\n", Math.max(0, cursor - 1)) + 1;
    let end = value.indexOf("\n", cursor);

    if (end === -1) {
      end = value.length;
    }

    return { start, end };
  }

  function toggleCommentTextarea(textarea) {
    const cellEl = textarea.closest(".vn-Cell");
    const kind = normalizeKind(cellEl ? cellEl.dataset.kind : "cpp");

    const marker = kind === "html" ? null : kind === "markdown" ? "> " : "// ";

    if (!marker) {
      return;
    }

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
        if (line.trim() === "") {
          return line;
        }

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

      if (cursor < nextPos) {
        break;
      }

      pos = nextPos;
    }

    const target = lineIndex + direction;

    if (target < 0 || target >= lines.length) {
      return;
    }

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
        {
          ok: true,
          document: state.document,
        },
        {
          fullRepaint: true,
        },
      );
    }
  }

  function toggleCellEdit(cellId) {
    const id = String(cellId || "");

    if (!id) {
      return;
    }

    if (state.editing && state.selectedId === id) {
      const cellEl = cellElById(id);

      if (cellEl) {
        localUpdateFromDom(cellEl);
      }

      exitEditMode();
      return;
    }

    selectCell(id, {
      edit: true,
      focus: true,
    });
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

  function syncToolbarKind() {
    const select = $(sel.toolbarKind);
    if (!select) return;
    const cell = findCell(state.selectedId);
    if (!cell) return;
    const k = normalizeKind(cell.kind);
    select.value = ["cpp", "reply", "markdown", "html"].includes(k) ? k : "";
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

    if (tab) {
      tab.dirty = !!dirty;
    }

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
    // Already the same kind? Nothing to do (avoids needless repaint).
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
      // Force a clean repaint of the affected cell only. We pass the updated
      // document and let reconcile rebuild the single cell whose signature
      // changed (kind flips the signature), so no duplication can occur.
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
      await api("/api/document/save", { method: "POST" });
      setDirty(false);
      setMessage("Note saved.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to save note.", "error");
      setDirty(true);
    } finally {
      setBusy(false);
    }
  }

  /* ==========================================================
   * File actions (modals instead of prompt())
   * ======================================================== */
  async function newNote(dir = null) {
    const initialFolder = normalizeExplorerPath(
      dir ||
        state.explorer.selectedDirPath ||
        state.explorer.currentPath ||
        parentPath(currentDocPath()) ||
        ".",
    );

    const data = await showModalForm({
      title: "New note",
      fields: [
        {
          name: "folder",
          label: "Folder",
          type: "select",
          value: initialFolder,
          options: knownDirectories().map((path) => ({
            value: path,
            label: path === "." ? ". / current directory" : path,
          })),
          hint: "Choose where the note will be created",
        },
        {
          name: "filename",
          label: "File name",
          value: baseName(suggestedNotePath(initialFolder)),
          placeholder: "intro.vixnote",
          hint: "Only the file name, not the full path",
        },
        {
          name: "title",
          label: "Title",
          value: "New Note",
          placeholder: "Intro to C++",
        },
      ],
      confirm: "Create",
    });

    if (!data) {
      return;
    }

    const folder = normalizeExplorerPath(data.folder || ".");
    let filename = String(data.filename || "").trim();

    if (!filename) {
      return;
    }

    filename = filename.replaceAll("\\", "/");

    // Important: the filename field must stay a filename.
    // If the user pasted a full path, keep only the last segment.
    filename = baseName(filename);

    if (!filename.endsWith(".vixnote")) {
      setMessage("Note file name must end with .vixnote", "error");
      return;
    }

    const path = folder === "." ? filename : `${folder}/${filename}`;

    const title = String(data.title || "").trim() || "New Note";

    setBusy(true);
    setMessage("");

    try {
      const doc = await api("/api/document/new", {
        method: "POST",
        body: JSON.stringify({ path, title }),
      });

      const d = unwrapDocument(doc);

      openTab(d.path, d.title);
      state.selectedId = null;
      renderDocument(doc, { fullRepaint: true });
      setDirty(false);

      touchExplorerEntry(d.path, "file", baseName(d.path), {
        modified: Date.now(),
        openable: true,
        extension: ".vixnote",
      });

      await loadDirectory(parentPath(d.path), {
        silent: true,
        force: true,
      });

      setMessage(`Note created: ${d.path}`, "success");
    } catch (error) {
      setMessage(error.message || "Failed to create note.", "error");
    } finally {
      setBusy(false);
    }
  }

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

  async function openNotePath(path) {
    if (!path) return;
    if (!path.endsWith(".vixnote")) {
      setMessage("Note path must end with .vixnote", "error");
      return;
    }
    setBusy(true);
    setMessage("");
    try {
      const doc = await api("/api/document/open", {
        method: "POST",
        body: JSON.stringify({ path }),
      });
      const d = unwrapDocument(doc);

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

      setMessage("Note opened.", "success");
    } catch (error) {
      if (isMissingNoteError(error)) {
        removeTabState(path);

        setMessage(`Removed missing note from tabs: ${path}`, "warning");

        if (state.activeTabPath) {
          await openNotePath(state.activeTabPath);
        } else {
          clearEditorNoOpenNote();

          await loadDirectory(".", {
            silent: true,
            force: true,
          });
        }

        return;
      }

      setMessage(error.message || "Failed to open note.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function newFolder(parentDir = null) {
    const initialFolder = normalizeExplorerPath(
      parentDir ||
        state.explorer.selectedDirPath ||
        state.explorer.currentPath ||
        ".",
    );

    const data = await showModalForm({
      title: "New folder",
      fields: [
        {
          name: "folder",
          label: "Parent folder",
          type: "select",
          value: initialFolder,
          options: knownDirectories().map((path) => ({
            value: path,
            label: path === "." ? ". / project root" : path,
          })),
          hint: "Choose where the folder will be created",
        },
        {
          name: "name",
          label: "Folder name",
          value: "",
          placeholder: "phase1",
          hint: "Only the folder name, not the full path",
        },
      ],
      confirm: "Create",
    });

    if (!data) {
      return;
    }

    const folder = normalizeExplorerPath(data.folder || ".");
    const name = String(data.name || "").trim();

    if (!name) {
      return;
    }

    const path = joinExplorerPath(folder, name);

    if (!path) {
      return;
    }

    setBusy(true);
    setMessage("");

    try {
      await api("/api/directory/create", {
        method: "POST",
        body: JSON.stringify({ path }),
      });

      touchExplorerEntry(path, "dir", baseName(path), {
        modified: Date.now(),
        openable: false,
      });

      state.explorer.expandedDirs.add(folder);
      state.explorer.selectedDirPath = path;
      state.explorer.currentPath = folder;

      await loadDirectory(folder, {
        silent: true,
        force: true,
      });

      setMessage(`Folder created: ${path}`, "success");
    } catch (error) {
      setMessage(error.message || "Failed to create folder.", "error");
    } finally {
      setBusy(false);
    }
  }
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

      await loadDirectory(parentPath(path), {
        silent: true,
        force: true,
      });

      if (deletedActiveDocument) {
        state.tabs = state.tabs.filter((tab) => {
          return (
            normalizeExplorerPath(tab.path) !== normalizeExplorerPath(path)
          );
        });

        state.activeTabPath = state.tabs.length ? state.tabs[0].path : null;

        if (state.activeTabPath) {
          await openNotePath(state.activeTabPath);
        } else {
          state.document = null;
          state.selectedId = null;
          await loadDocument();
        }
      }

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();

      setMessage(`Deleted: ${path}`, "success");
    } catch (error) {
      setMessage(error.message || "Failed to delete path.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function renamePath(path, type = "file") {
    const oldPath = normalizeExplorerPath(path);
    const oldName = baseName(oldPath);

    const data = await showModalForm({
      title: type === "dir" ? "Rename folder" : "Rename file",
      fields: [
        {
          name: "newName",
          label: "New name",
          value: oldName,
          placeholder: oldName,
          hint:
            type === "file"
              ? "Keep the .vixnote extension for note files"
              : "Only the folder name, not the full path",
        },
      ],
      confirm: "Rename",
    });

    if (!data) {
      return;
    }

    let newName = String(data.newName || "").trim();

    if (!newName) {
      return;
    }

    newName = newName.replaceAll("\\", "/");
    newName = baseName(newName);

    if (!newName) {
      return;
    }

    if (
      type === "file" &&
      oldPath.endsWith(".vixnote") &&
      !newName.endsWith(".vixnote")
    ) {
      setMessage("Renamed note must keep the .vixnote extension.", "error");
      return;
    }

    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/rename", {
        method: "POST",
        body: JSON.stringify({
          path: oldPath,
          newName,
        }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to rename path.");
      }

      const newPath = normalizeExplorerPath(
        result.newPath || joinExplorerPath(parentPath(oldPath), newName),
      );

      // Remove old entry.
      const oldEntry = state.explorer.entries.get(oldPath);
      state.explorer.entries.delete(oldPath);

      // Rename children in frontend state when a directory is renamed.
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
            title:
              entry.type === "file"
                ? baseName(renamedChildPath)
                : baseName(renamedChildPath),
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

      // Update expanded/loading state.
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

      // Update tabs if an opened note was renamed.
      for (const tab of state.tabs) {
        if (tab.path === oldPath) {
          tab.path = newPath;
          tab.title = baseName(newPath);
        }
      }

      if (state.activeTabPath === oldPath) {
        state.activeTabPath = newPath;
      }

      if (state.document && state.document.path === oldPath) {
        state.document.path = newPath;
        setText(sel.document, baseName(newPath));
      }

      await loadDirectory(parentPath(newPath), {
        silent: true,
        force: true,
      });

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();

      setMessage(`Renamed: ${oldPath} → ${newPath}`, "success");
    } catch (error) {
      setMessage(error.message || "Failed to rename path.", "error");
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
  }
  function clearAllOutputs() {
    for (const cell of cells()) cell.outputs = [];
    for (const oa of $all(".vn-OutputArea")) oa.remove();
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
        await openNotePath(path);
        await loadExplorerForDocumentPath(path);
        return true;
      } catch (error) {
        removeTabState(path);
      }
    }

    clearEditorNoOpenNote();

    await loadDirectory(".", {
      silent: true,
      force: true,
    });

    return false;
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

        if (ok) {
          setKernel("idle");
        }

        return;
      }

      clearEditorNoOpenNote();

      await loadDirectory(".", {
        silent: true,
        force: true,
      });

      return;
    }

    try {
      const doc = await api("/api/document");
      const d = unwrapDocument(doc);

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

      await loadDirectory(".", {
        silent: true,
        force: true,
      });
    }
  }

  /* ==========================================================
   * Explorer (backend-backed model)
   * ======================================================== */
  function touchExplorerEntry(path, type, title, options = {}) {
    const normalized = normalizeExplorerPath(path);

    if (!normalized) {
      return;
    }

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

    // Ensure parent folders appear too.
    if (normalized !== ".") {
      const parts = normalized.split("/");
      parts.pop();

      let acc = "";

      for (const part of parts) {
        if (!part) {
          continue;
        }

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

      if (!name) {
        continue;
      }

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

      if (!path || path === dirPath) {
        continue;
      }

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

    if (!silent) {
      setMessage(`Loading ${dirPath}…`, "info");
    }

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

      if (!silent) {
        setMessage("Explorer refreshed.", "success");
      }
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

    state.explorer.entries.clear();
    state.explorer.loadedDirs.clear();
    state.explorer.expandedDirs.clear();
    state.explorer.expandedDirs.add(".");

    await loadDirectory(dirPath, {
      force: true,
      silent: false,
    });
  }

  function explorerEntries() {
    const filter = (
      ($(sel.explorerSearch) && $(sel.explorerSearch).value) ||
      ""
    )
      .trim()
      .toLowerCase();

    let list = Array.from(state.explorer.entries.values()).filter(
      shouldShowEntry,
    );

    if (filter) {
      list = list.filter((e) => {
        return (
          String(e.path || "")
            .toLowerCase()
            .includes(filter) ||
          String(e.title || "")
            .toLowerCase()
            .includes(filter)
        );
      });
    }

    // Folders first, then files; alpha within.
    list.sort((a, b) => {
      if (a.type !== b.type) {
        return a.type === "dir" ? -1 : 1;
      }

      return String(a.path || "").localeCompare(String(b.path || ""));
    });

    return list;
  }

  function fileIcon() {
    // file / note sheet
    return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V9l-6-6z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M13 3v6h6"/></svg>';
  }

  function dirIcon(entry) {
    const path = normalizeExplorerPath(entry && entry.path);
    const loaded = state.explorer.loadedDirs.has(path);
    const expanded = state.explorer.expandedDirs.has(path);

    if (loaded && expanded) {
      // open folder
      return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1H3V7z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 10h18l-2 8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-.8L3 10z"/></svg>';
    }

    // closed folder
    return '<svg viewBox="0 0 24 24" class="vn-Tree__icon" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" d="M3 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6z"/></svg>';
  }

  function renderExplorer() {
    const listEl = $(sel.explorerList);

    if (!listEl) {
      return;
    }

    const entries = explorerTreeRows();
    const loadingPath = state.explorer.loadingPath;

    setText(sel.explorerCount, String(entries.length));

    if (loadingPath && !entries.length) {
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        Loading ${escapeHtml(loadingPath)}…
      </p>
    `;
      return;
    }

    if (!entries.length) {
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        No notes found. Create one with <strong>New note</strong> or refresh the explorer.
      </p>
    `;
      return;
    }

    listEl.innerHTML = entries
      .map((e) => {
        const path = normalizeExplorerPath(e.path);
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
        </div>
      `;
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
    } catch (_) {
      // Ignore storage failures. Tabs still work in memory.
    }
  }

  function restorePersistedTabs() {
    try {
      const raw = localStorage.getItem(TABS_STORAGE_KEY);

      if (!raw) {
        return false;
      }

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
        JSON.stringify({
          activeTabPath: null,
          tabs: [],
        }),
      );
    } catch (_) {
      // Ignore storage failures.
    }
  }

  /* ==========================================================
   * Tabs
   * ======================================================== */
  function activeTab() {
    return state.tabs.find((t) => t.path === state.activeTabPath) || null;
  }

  function openTab(path, title) {
    if (!path) {
      return;
    }

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
    if (!doc || !doc.path) {
      return;
    }

    const title = documentDisplayTitle(doc);

    if (state.activeTabPath !== doc.path) {
      openTab(doc.path, title);
    } else {
      const tab = activeTab();

      if (tab) {
        tab.title = title;
      }
    }
  }

  async function switchTab(path) {
    if (!path || path === state.activeTabPath) return;
    // Warn if leaving an unsaved tab.
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

    if (idx < 0) {
      return;
    }

    const tab = state.tabs[idx];

    if (tab.dirty) {
      const proceed = await showModalConfirm({
        title: "Close tab",
        body: `“${escapeHtml(tab.title)}” has unsaved changes. Close without saving?`,
        confirm: "Close tab",
        danger: true,
      });

      if (!proceed) {
        return;
      }
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

  function renderOpenTabs() {
    const listEl = $(sel.openTabsList);
    setText(sel.openTabsCount, String(state.tabs.length));
    if (!listEl) return;
    if (!state.tabs.length) {
      listEl.innerHTML = `<p class="vn-Tree__empty">No open notes.</p>`;
      return;
    }
    listEl.innerHTML = state.tabs
      .map((t) => {
        const active = t.path === state.activeTabPath ? " is-active" : "";
        return `<div class="vn-Tree__row${active}" data-tab-path="${escapeHtml(t.path)}" tabindex="0">
            ${fileIcon()}
            <span class="vn-Tree__label" title="${escapeHtml(t.path)}">${escapeHtml(t.title)}</span>
            ${t.dirty ? '<span class="vn-Tab__dot"></span>' : ""}
            <button class="vn-Tree__menuBtn" type="button" data-tab-close="${escapeHtml(t.path)}" title="Close" aria-label="Close">×</button>
          </div>`;
      })
      .join("");
  }

  /* ==========================================================
   * Activity bar (Explorer / Open tabs)
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
    const s = $(sel.toolbarKind);
    const v = s ? s.value : "";
    return v || "cpp";
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
    "show-tabs": { label: "Show Open Tabs", run: () => setPanel("tabs") },
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
        $('input[name="filename"]', form) ||
        $('input[name="path"]', form) ||
        $("input, textarea, select", form);

      if (firstInput) {
        firstInput.focus();

        if (typeof firstInput.select === "function") {
          firstInput.select();
        }
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
      `<p><strong>Vix Note</strong> — visual executable notes for learning C++ and Vix.cpp faster.</p>
       <p>Explanations, C++ cells, Reply cells, HTML cells, outputs and project context live together in one document.</p>
       <p style="color:var(--vn-text3)">Part of the Vix.cpp ecosystem · MIT License · © 2026 Gaspard Kirira.</p>`,
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

    // position within viewport
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
      { id: "open", label: "Open", run: () => openNotePath(path) },
      { id: "rename", label: "Rename", run: () => renamePath(path, "file") },
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
      {
        id: "download",
        label: "Download",
        run: () => notAvailable("Download"),
      },
    ];
  }
  function dirContextItems(path) {
    return [
      { id: "new-note", label: "New note inside", run: () => newNote(path) },
      {
        id: "new-folder",
        label: "New folder inside",
        run: () => newFolder(path),
      },
      { id: "rename", label: "Rename", run: () => renamePath(path, "dir") },
      {
        id: "delete",
        label: "Delete empty folder",
        danger: true,
        run: async () => {
          if (await confirmDelete(baseName(path), "dir")) {
            await deletePath(path, { recursive: false });
          }
        },
      },
      {
        id: "delete-recursive",
        label: "Delete folder recursively",
        danger: true,
        run: async () => {
          const ok = await showModalConfirm({
            title: "Delete folder recursively",
            body: `Delete “${escapeHtml(path)}” and everything inside it from disk? This cannot be undone from Vix Note.`,
            confirm: "Delete everything",
            danger: true,
          });

          if (ok) {
            await deletePath(path, { recursive: true });
          }
        },
      },
    ];
  }

  function notAvailable(what) {
    setMessage(`${what} is not available yet.`, "warning");
  }

  /* ==========================================================
   * Wiring: header toolbar actions
   * ======================================================== */
  function bindActions() {
    document.addEventListener("click", (event) => {
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
          newNote();
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
        case "shortcuts":
          showShortcuts();
          break;
        default:
          break;
      }
    });

    const kindSelect = $(sel.toolbarKind);
    if (kindSelect) {
      kindSelect.addEventListener("change", () => {
        if (state.selectedId)
          changeKind(state.selectedId, currentToolbarKind());
      });
    }
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
   * ======================================================== */
  function bindExplorer() {
    const listEl = $(sel.explorerList);
    if (listEl) {
      listEl.addEventListener("click", (e) => {
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
        if (type === "dir") {
          toggleDirectory(path);
          return;
        }

        if (type === "file") {
          const entry = state.explorer.entries.get(path);

          if (entry && entry.openable === false && !path.endsWith(".vixnote")) {
            setMessage(
              "Only .vixnote files can be opened in Vix Note.",
              "warning",
            );
            return;
          }

          openNotePath(path);
        }
      });
      listEl.addEventListener("contextmenu", (e) => {
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
      listEl.addEventListener("keydown", (e) => {
        if (e.key !== "Enter") return;
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");
        if (type === "dir") {
          toggleDirectory(path);
          return;
        }

        if (type === "file") {
          const entry = state.explorer.entries.get(path);

          if (entry && entry.openable === false && !path.endsWith(".vixnote")) {
            setMessage(
              "Only .vixnote files can be opened in Vix Note.",
              "warning",
            );
            return;
          }

          openNotePath(path);
        }
      });
    }

    const search = $(sel.explorerSearch);
    if (search) search.addEventListener("input", renderExplorer);

    const openTabsList = $(sel.openTabsList);
    if (openTabsList) {
      openTabsList.addEventListener("click", (e) => {
        const close = e.target.closest("[data-tab-close]");
        if (close) {
          e.stopPropagation();
          closeTab(close.getAttribute("data-tab-close"));
          return;
        }
        const row = e.target.closest("[data-tab-path]");
        if (row) switchTab(row.getAttribute("data-tab-path"));
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

      if (!(ta instanceof HTMLTextAreaElement)) {
        return;
      }

      if (ta.getAttribute("data-action") !== "edit-source") {
        return;
      }

      markTextareaChanged(ta);
    });

    container.addEventListener("click", (event) => {
      const ta = event.target;

      if (!(ta instanceof HTMLTextAreaElement)) {
        return;
      }

      if (ta.getAttribute("data-action") !== "edit-source") {
        return;
      }

      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener("keyup", (event) => {
      const ta = event.target;

      if (!(ta instanceof HTMLTextAreaElement)) {
        return;
      }

      if (ta.getAttribute("data-action") !== "edit-source") {
        return;
      }

      updateLineFocus(ta);
      updateCursorStatus(ta);
    });

    container.addEventListener("select", (event) => {
      const ta = event.target;

      if (!(ta instanceof HTMLTextAreaElement)) {
        return;
      }

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

  function bindKeyboard() {
    document.addEventListener("keydown", async (event) => {
      const inField =
        event.target instanceof HTMLTextAreaElement ||
        event.target instanceof HTMLInputElement;
      const inTextarea = event.target instanceof HTMLTextAreaElement;
      const meta = event.ctrlKey || event.metaKey;

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

            if (cellEl) {
              localUpdateFromDom(cellEl);
            }
          }

          exitEditMode();
          return;
        }

        if (event.key === "Tab") {
          event.preventDefault();

          if (event.shiftKey) {
            outdentTextarea(ta);
          } else {
            indentTextarea(ta);
          }

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

    // close modal on backdrop / × click
    for (const c of $all("[data-modal-close]"))
      c.addEventListener("click", () => closeModal());

    setPanel("explorer");
    setKernel("idle");
    renderExplorer();
    renderOpenTabs();
    renderTabsBar();
    loadDocument();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
