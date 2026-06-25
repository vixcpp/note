/*
 * Vix Note — notebook frontend
 *
 * Talks to the existing Vix Note HTTP API (NoteRoutes.cpp):
 *   GET    /api/document
 *   POST   /api/cells                 { kind, source, index? }
 *   PUT    /api/cells/<id>            { kind, source }
 *   DELETE /api/cells/<id>
 *   POST   /api/cells/<id>/run
 *   POST   /api/cells/<id>/move       { index }
 *   POST   /api/run-all
 *   POST   /api/document/save
 *
 * Provides command/edit modes, In[ ]/Out[ ] prompts, cell selection,
 * keyboard shortcuts, syntax highlighting, outline, dropdown menus,
 * a command palette, a resizable/collapsible sidebar and a status bar.
 *
 * Copyright 2026, Gaspard Kirira. MIT License.
 */
(() => {
  "use strict";

  /* ----------------------------------------------------------
   * State
   * -------------------------------------------------------- */
  const state = {
    document: null,
    selectedId: null,
    editing: false,
    kernel: "idle",
    busy: false,
    sidebarCollapsed: false,
    sidebarWidth: 250,
    focusMode: false,
  };

  const DEFAULT_SIDEBAR_WIDTH = 250;
  const MIN_SIDEBAR_WIDTH = 180;
  const MAX_SIDEBAR_WIDTH = 520;

  const app = document.querySelector("[data-note-app]");

  const sel = {
    cells: "[data-note-cells]",
    title: "[data-note-title]",
    document: "[data-note-document]",
    project: "[data-note-project]",
    project2: "[data-note-project-2]",
    cellCount: "[data-note-cell-count]",
    execCount: "[data-note-exec-count]",
    kernel: "[data-note-kernel]",
    kernel2: "[data-note-kernel-2]",
    state: "[data-note-state]",
    message: "[data-note-message]",
    toc: "[data-note-toc]",
    tocCount: "[data-toc-count]",
    toolbarKind: '[data-action="toolbar-kind"]',
    statusMode: "[data-status-mode]",
    statusPosition: "[data-status-position]",
    statusKind: "[data-status-kind]",
    statusKernel: "[data-status-kernel]",
    sidebar: "[data-sidebar]",
    sidebarResizer: "[data-sidebar-resizer]",
  };

  const $ = (s, root = document) => root.querySelector(s);
  const $all = (s, root = document) => Array.from(root.querySelectorAll(s));

  /* ----------------------------------------------------------
   * Helpers
   * -------------------------------------------------------- */
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
    setText(sel.kernel2, label);
    setText(sel.statusKernel, label);
  }

  function setDirty(dirty) {
    const el = $(sel.state);
    if (el) el.hidden = !dirty;
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
    for (const b of $all(".vn-ToolbarButton, .vn-Sidebar__buttons button")) {
      b.disabled = busy;
    }
  }

  /* ----------------------------------------------------------
   * API layer
   * -------------------------------------------------------- */
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

  /* ----------------------------------------------------------
   * Model accessors
   * -------------------------------------------------------- */
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

  /* ----------------------------------------------------------
   * Markdown renderer
   * -------------------------------------------------------- */
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

  /* ----------------------------------------------------------
   * Syntax highlighter
   * -------------------------------------------------------- */
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

  /* ----------------------------------------------------------
   * Output rendering
   * -------------------------------------------------------- */
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

  /* ----------------------------------------------------------
   * Cell rendering
   * -------------------------------------------------------- */
  const ICONS = {
    run: '<svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>',
    up: '<svg viewBox="0 0 24 24"><path d="M12 7l6 6-1.4 1.4L12 9.8l-4.6 4.6L6 13l6-6z"/></svg>',
    down: '<svg viewBox="0 0 24 24"><path d="M12 17l-6-6 1.4-1.4L12 14.2l4.6-4.6L18 11l-6 6z"/></svg>',
    del: '<svg viewBox="0 0 24 24"><path d="M6 7h12l-1 14H7L6 7zm3-3h6l1 2H8l1-2z"/></svg>',
    copy: '<svg viewBox="0 0 24 24"><path d="M16 1H4a2 2 0 00-2 2v14h2V3h12V1zm3 4H8a2 2 0 00-2 2v14a2 2 0 002 2h11a2 2 0 002-2V7a2 2 0 00-2-2zm0 16H8V7h11v14z"/></svg>',
    edit: '<svg viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 000-1.41l-2.34-2.34a1 1 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
  };

  // Icons used inside dropdown menus + command palette, keyed by command id.
  const MENU_ICONS = {
    save: '<svg viewBox="0 0 24 24"><path d="M5 3h11l3 3v15H5V3zm2 2v4h8V5H7zm0 7v6h10v-6H7z"/></svg>',
    reload:
      '<svg viewBox="0 0 24 24"><path d="M12 6V3L8 7l4 4V8a4 4 0 11-4 4H6a6 6 0 106-6z"/></svg>',
    "add-cpp":
      '<svg viewBox="0 0 24 24"><path d="M11 5h2v6h6v2h-6v6h-2v-6H5v-2h6V5z"/></svg>',
    "add-reply":
      '<svg viewBox="0 0 24 24"><path d="M11 5h2v6h6v2h-6v6h-2v-6H5v-2h6V5z"/></svg>',
    "add-markdown":
      '<svg viewBox="0 0 24 24"><path d="M11 5h2v6h6v2h-6v6h-2v-6H5v-2h6V5z"/></svg>',
    "add-html":
      '<svg viewBox="0 0 24 24"><path d="M11 5h2v6h6v2h-6v6h-2v-6H5v-2h6V5z"/></svg>',
    "insert-below":
      '<svg viewBox="0 0 24 24"><path d="M11 5h2v6h6v2h-6v6h-2v-6H5v-2h6V5z"/></svg>',
    "run-cell": '<svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>',
    "run-advance":
      '<svg viewBox="0 0 24 24"><path d="M4 5v14l8-7-8-7zm9 0v14l8-7-8-7z"/></svg>',
    "run-all":
      '<svg viewBox="0 0 24 24"><path d="M5 5v14l8-7-8-7zm9 0v14l8-7-8-7z"/></svg>',
    "cut-cell":
      '<svg viewBox="0 0 24 24"><path d="M6 7h12l-1 14H7L6 7zm3-3h6l1 2H8l1-2z"/></svg>',
    duplicate:
      '<svg viewBox="0 0 24 24"><path d="M16 1H4a2 2 0 00-2 2v14h2V3h12V1zm3 4H8a2 2 0 00-2 2v14a2 2 0 002 2h11a2 2 0 002-2V7a2 2 0 00-2-2zm0 16H8V7h11v14z"/></svg>',
    "move-up":
      '<svg viewBox="0 0 24 24"><path d="M12 7l6 6-1.4 1.4L12 9.8l-4.6 4.6L6 13l6-6z"/></svg>',
    "move-down":
      '<svg viewBox="0 0 24 24"><path d="M12 17l-6-6 1.4-1.4L12 14.2l4.6-4.6L18 11l-6 6z"/></svg>',
    "to-cpp":
      '<svg viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 000-1.41l-2.34-2.34a1 1 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
    "to-markdown":
      '<svg viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 000-1.41l-2.34-2.34a1 1 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
    "to-reply":
      '<svg viewBox="0 0 24 24"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 000-1.41l-2.34-2.34a1 1 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>',
    "clear-cell": '<svg viewBox="0 0 24 24"><path d="M5 13h14v-2H5v2z"/></svg>',
    "clear-all": '<svg viewBox="0 0 24 24"><path d="M5 13h14v-2H5v2z"/></svg>',
    "collapse-all":
      '<svg viewBox="0 0 24 24"><path d="M7 14l5-5 5 5H7z"/></svg>',
    "expand-all": '<svg viewBox="0 0 24 24"><path d="M7 10l5 5 5-5H7z"/></svg>',
    restart:
      '<svg viewBox="0 0 24 24"><path d="M12 6V3L8 7l4 4V8a4 4 0 11-4 4H6a6 6 0 106-6z"/></svg>',
    "restart-run":
      '<svg viewBox="0 0 24 24"><path d="M12 6V3L8 7l4 4V8a4 4 0 11-4 4H6a6 6 0 106-6z"/></svg>',
    "toggle-sidebar":
      '<svg viewBox="0 0 24 24"><path d="M3 4h18v16H3V4zm2 2v12h5V6H5z"/></svg>',
    "toggle-focus":
      '<svg viewBox="0 0 24 24"><path d="M3 7V3h4v2H5v2H3zm14-4h4v4h-2V5h-2V3zM3 17h2v2h2v2H3v-4zm16 0h2v4h-4v-2h2v-2z"/></svg>',
    shortcuts:
      '<svg viewBox="0 0 24 24"><path d="M4 5h16a1 1 0 011 1v12a1 1 0 01-1 1H4a1 1 0 01-1-1V6a1 1 0 011-1zm2 3v2h2V8H6zm4 0v2h2V8h-2zm4 0v2h2V8h-2zm-8 4v2h2v-2H6zm4 0v2h6v-2h-6z"/></svg>',
    palette:
      '<svg viewBox="0 0 24 24"><path d="M21 21l-4.35-4.35M11 18a7 7 0 100-14 7 7 0 000 14z" fill="none" stroke="currentColor" stroke-width="2"/></svg>',
    about:
      '<svg viewBox="0 0 24 24"><path d="M12 2a10 10 0 100 20 10 10 0 000-20zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>',
  };
  function menuIcon(id) {
    return MENU_ICONS[id] || '<svg viewBox="0 0 24 24"></svg>';
  }

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
    // Markdown / HTML render live while typing — no run button, just an
    // explicit "Edit" toggle so the source is always reachable again.
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

  function renderTOC() {
    const toc = $(sel.toc);
    if (!toc) return;
    const items = [];
    for (const cell of cells()) {
      if (normalizeKind(cell.kind) !== "markdown") continue;
      for (const line of String(cell.source || "").split(/\r?\n/)) {
        const t = line.trim();
        let level = 0,
          text = "";
        if (t.startsWith("### ")) {
          level = 3;
          text = t.slice(4);
        } else if (t.startsWith("## ")) {
          level = 2;
          text = t.slice(3);
        } else if (t.startsWith("# ")) {
          level = 1;
          text = t.slice(2);
        }
        if (level) items.push({ level, text, id: cell.id });
      }
    }
    setText(sel.tocCount, String(items.length));
    if (!items.length) {
      toc.innerHTML = `<p class="vn-Toc__empty">No headings yet.</p>`;
      return;
    }
    toc.innerHTML = items
      .map(
        (it) =>
          `<button class="vn-Toc__item vn-Toc__item--h${it.level}" data-toc-cell="${escapeHtml(it.id)}" title="${escapeHtml(it.text)}">${escapeHtml(it.text)}</button>`,
      )
      .join("");
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

  /* ----------------------------------------------------------
   * Rendering abstraction
   *
   * Instead of rebuilding the whole notebook on every change we keep a
   * keyed list of cell nodes (by data-cell-id) and reconcile:
   *   - reuse a node when its render signature is unchanged
   *   - patch only the output/prompt region when a code cell runs
   *   - never touch the textarea of the cell being edited
   * This removes layout jumps and makes updates fast.
   * -------------------------------------------------------- */

  // A cheap signature: if it changes, the cell node is rebuilt.
  function cellSignature(cell) {
    const kind = normalizeKind(cell.kind);
    const exec = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs.length : 0;
    const sel = state.selectedId === String(cell.id);
    const ed = sel && state.editing;
    // source length is enough to detect structural source changes we didn't
    // originate locally; the textarea itself is preserved when editing.
    return `${kind}|${exec}|${outs}|${sel ? 1 : 0}|${ed ? 1 : 0}`;
  }

  // Build a detached DOM fragment for a cell (insert bar + cell element).
  function buildCellNodes(cell) {
    const tpl = document.createElement("template");
    tpl.innerHTML = renderCell(cell).trim();
    // Only element nodes: [insertBar, cellEl]. Whitespace text nodes between
    // the two divs would otherwise misalign the pair and the cell wouldn't show.
    return Array.from(tpl.content.children);
  }

  // Replace only the output area of a code cell, in place.
  function patchCellOutputs(cellEl, cell) {
    const codeCell = $(".vn-CodeCell", cellEl);
    if (!codeCell) return;
    // refresh In[] prompt number
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

  // Keyed reconcile of the cell list against the DOM.
  function reconcileCells(container, list) {
    // Map existing cell elements by id.
    const existing = new Map();
    for (const el of $all(".vn-Cell", container))
      existing.set(el.dataset.cellId, el);

    // Build the desired order of [insertBar, cellEl] pairs, reusing nodes.
    const desired = [];
    for (const cell of list) {
      const id = String(cell.id);
      const sig = cellSignature(cell);
      const prev = existing.get(id);
      if (prev && prev.dataset.sig === sig) {
        // reuse as-is; its insert bar is its previousSibling
        const bar =
          prev.previousElementSibling &&
          prev.previousElementSibling.classList.contains("vn-CellInsert")
            ? prev.previousElementSibling
            : null;
        desired.push([bar, prev]);
        existing.delete(id);
      } else if (prev && state.editing && state.selectedId === id) {
        // editing this cell: don't rebuild the textarea, just patch outputs
        patchCellOutputs(prev, cell);
        prev.dataset.sig = sig;
        const bar =
          prev.previousElementSibling &&
          prev.previousElementSibling.classList.contains("vn-CellInsert")
            ? prev.previousElementSibling
            : null;
        desired.push([bar, prev]);
        existing.delete(id);
      } else {
        const nodes = buildCellNodes(cell);
        const bar = nodes[0];
        const cellEl = nodes[1];
        if (cellEl && cellEl.dataset) cellEl.dataset.sig = sig;
        desired.push([bar, cellEl]);
        if (prev) existing.delete(id);
      }
    }

    // Remove leftover nodes no longer present.
    for (const [, el] of existing) {
      const bar =
        el.previousElementSibling &&
        el.previousElementSibling.classList.contains("vn-CellInsert")
          ? el.previousElementSibling
          : null;
      if (bar) bar.remove();
      el.remove();
    }

    // Re-order / insert to match the desired sequence. appendChild moves an
    // existing node (no clone), so reused cells keep their textarea + caret.
    // Building order this way also discards stray whitespace text nodes.
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

    const title = doc.title || "Untitled note";
    const count = Number(doc.cellCount || (doc.cells ? doc.cells.length : 0));
    const project = doc.project || null;

    setText(sel.title, title);
    setText(
      sel.document,
      (doc.path && doc.path.split(/[\\/]/).pop()) || `${title}.vixnote`,
    );
    setText(sel.project, projectLabel(project));
    setText(sel.project2, projectLabel(project));
    setText(sel.cellCount, String(count));
    setText(sel.execCount, String(executedCount()));
    document.title = `${title} · Vix Note`;

    const container = $(sel.cells);
    if (!container) return;
    const list = cells();
    if (!list.length) {
      renderEmpty(
        "This note has no cells yet. Use the toolbar + or the sidebar to add a cell.",
      );
      renderTOC();
      updateStatusBar();
      return;
    }
    if (!state.selectedId || !findCell(state.selectedId)) {
      state.selectedId = String(list[0].id);
    }

    // If the container is empty (first paint) do a single innerHTML write.
    const firstPaint = !$(".vn-Cell", container);
    const scroller = container.closest(".vn-NotebookPanel");
    const prevScroll = scroller ? scroller.scrollTop : 0;

    if (firstPaint) {
      container.innerHTML = list
        .map((c) => {
          const html = renderCell(c);
          return html;
        })
        .join("");
      // tag signatures after first paint
      for (const el of $all(".vn-Cell", container)) {
        const cell = findCell(el.dataset.cellId);
        if (cell) el.dataset.sig = cellSignature(cell);
      }
    } else {
      reconcileCells(container, list);
    }

    if (scroller) scroller.scrollTop = prevScroll;
    renderTOC();
    autosizeAll();
    syncToolbarKind();
    updateStatusBar();
  }

  /* ----------------------------------------------------------
   * Editor autosize + highlight
   * -------------------------------------------------------- */
  function autosize(textarea) {
    if (!textarea) return;
    textarea.style.height = "auto";
    textarea.style.height = `${textarea.scrollHeight}px`;
    const wrap = textarea.closest(".vn-Editor__wrap");
    const hl = wrap ? $("[data-highlight]", wrap) : null;
    if (hl) hl.style.height = `${textarea.scrollHeight}px`;
  }
  function autosizeAll() {
    for (const ta of $all('textarea[data-action="edit-source"]')) autosize(ta);
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
  }

  /* ----------------------------------------------------------
   * Selection / modes
   * -------------------------------------------------------- */
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
    select.value =
      k === "cpp"
        ? "cpp"
        : k === "reply"
          ? "reply"
          : k === "markdown"
            ? "markdown"
            : k === "html"
              ? "html"
              : "";
  }

  /* ----------------------------------------------------------
   * Cell sync
   * -------------------------------------------------------- */
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
    if (result.document) state.document = result.document;
  }

  /* ----------------------------------------------------------
   * Actions
   * -------------------------------------------------------- */
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

    // Markdown / HTML cells are never "executed": they preview live as you
    // type. Save the source silently and stay in edit mode so the code is
    // always reachable.
    if (!isCodeKind(cell.kind)) {
      localUpdateFromDom(cellEl);
      try {
        await pushCell(cellEl);
      } catch (_) {}
      setDirty(false);
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

      // Update model, then patch only this cell's outputs (no full re-render).
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
    localUpdateFromDom(cellEl);
    cell.kind = newKind || "cpp";
    setBusy(true);
    try {
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}`, {
        method: "PUT",
        body: JSON.stringify({ kind: cell.kind, source: cell.source }),
      });
      if (result.document) renderDocument(result.document);
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

  async function newNote() {
    const path = prompt("New note path:", "lessons/new_note.vixnote");

    if (!path) {
      return;
    }

    const title = prompt("Note title:", "New Note") || "New Note";

    setBusy(true);
    setMessage("");

    try {
      const doc = await api("/api/document/new", {
        method: "POST",
        body: JSON.stringify({ path, title }),
      });

      renderDocument(doc);
      setDirty(false);
      setMessage("Note created.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to create note.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function openNote() {
    const path = prompt("Open note path:", "lessons/new_note.vixnote");

    if (!path) {
      return;
    }

    setBusy(true);
    setMessage("");

    try {
      const doc = await api("/api/document/open", {
        method: "POST",
        body: JSON.stringify({ path }),
      });

      renderDocument(doc);
      setDirty(false);
      setMessage("Note opened.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to open note.", "error");
    } finally {
      setBusy(false);
    }
  }

  async function newFolder() {
    const path = prompt("New folder path:", "lessons");

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

      setMessage("Folder created.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to create folder.", "error");
    } finally {
      setBusy(false);
    }
  }

  /* clear outputs (client-side display only — server outputs persist until re-run) */
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
  function collapseAllOutputs(collapsed) {
    for (const oa of $all(".vn-OutputArea"))
      oa.classList.toggle("is-collapsed", collapsed);
  }

  async function restartKernel(runAfter = false) {
    setKernel("busy");
    setMessage("Restarting kernel…", "info");
    setTimeout(async () => {
      setKernel("idle");
      if (runAfter) {
        await runAll();
      } else setMessage("Kernel restarted.", "info");
    }, 400);
  }

  /* ----------------------------------------------------------
   * Load
   * -------------------------------------------------------- */
  async function loadDocument() {
    setKernel("idle");
    setMessage("");
    try {
      const doc = await api("/api/document");
      renderDocument(doc);
      setDirty(false);
    } catch (error) {
      setKernel("error");
      setMessage(error.message || "Failed to load note document.", "error");
      renderEmpty("Unable to load the note document.");
    }
  }

  /* ----------------------------------------------------------
   * Command registry (shared by menus + palette + toolbar)
   * -------------------------------------------------------- */
  function currentToolbarKind() {
    const s = $(sel.toolbarKind);
    const v = s ? s.value : "";
    return v || "cpp";
  }
  function targetId() {
    return state.selectedId;
  }

  const COMMANDS = {
    "new-note": {
      label: "New note",
      run: () => newNote(),
    },
    "open-note": {
      label: "Open note",
      run: () => openNote(),
    },
    "new-folder": {
      label: "New folder",
      run: () => newFolder(),
    },
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
    "clear-cell": {
      label: "Clear selected output",
      run: () => targetId() && clearCellOutput(targetId()),
    },
    "clear-all": { label: "Clear all outputs", run: () => clearAllOutputs() },
    "collapse-all": {
      label: "Collapse all outputs",
      run: () => collapseAllOutputs(true),
    },
    "expand-all": {
      label: "Expand all outputs",
      run: () => collapseAllOutputs(false),
    },
    restart: { label: "Restart kernel", run: () => restartKernel(false) },
    "restart-run": {
      label: "Restart kernel and run all",
      run: () => restartKernel(true),
    },
    "toggle-sidebar": {
      label: "Toggle sidebar",
      hint: "⌘B",
      run: () => toggleSidebar(),
    },
    "toggle-focus": { label: "Toggle focus mode", run: () => toggleFocus() },
    shortcuts: {
      label: "Show keyboard shortcuts",
      hint: "?",
      run: () => showShortcuts(),
    },
    palette: {
      label: "Open command palette",
      hint: "⌘⇧P",
      run: () => openPalette(),
    },
    about: { label: "About Vix Note", run: () => showAbout() },
  };

  function runCommand(name) {
    const cmd = COMMANDS[name];
    if (cmd && typeof cmd.run === "function") cmd.run();
  }

  /* ----------------------------------------------------------
   * Sidebar: collapse + resize
   * -------------------------------------------------------- */
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
  function toggleSidebar(force) {
    state.sidebarCollapsed = force != null ? force : !state.sidebarCollapsed;
    app.classList.toggle("is-sidebar-collapsed", state.sidebarCollapsed);
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

  /* ----------------------------------------------------------
   * Dropdown menus
   * -------------------------------------------------------- */
  function closeAllMenus() {
    for (const m of $all(".vn-Menu")) {
      m.classList.remove("is-open");
      const d = $(".vn-Menu__dropdown", m);
      if (d) d.hidden = true;
    }
  }
  function injectMenuIcons() {
    for (const item of $all("[data-menubar] [data-command]")) {
      if (item.querySelector(".vn-Menu__icon")) continue;
      const id = item.getAttribute("data-command");
      const span = document.createElement("span");
      span.className = "vn-Menu__icon";
      span.innerHTML = menuIcon(id);
      item.insertBefore(span, item.firstChild);
    }
  }

  function bindMenus() {
    const bar = $("[data-menubar]");
    if (!bar) return;

    injectMenuIcons();

    bar.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-menu-button]");
      if (!btn) return;
      const menu = btn.closest(".vn-Menu");
      const isOpen = menu.classList.contains("is-open");
      closeAllMenus();
      if (!isOpen) {
        menu.classList.add("is-open");
        const d = $(".vn-Menu__dropdown", menu);
        if (d) d.hidden = false;
      }
      e.stopPropagation();
    });

    // hover-switch when a menu is already open
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

    // menu item -> command
    bar.addEventListener("click", (e) => {
      const item = e.target.closest("[data-command]");
      if (!item) return;
      closeAllMenus();
      runCommand(item.getAttribute("data-command"));
    });

    document.addEventListener("click", () => closeAllMenus());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeAllMenus();
    });
  }

  /* ----------------------------------------------------------
   * Command palette
   * -------------------------------------------------------- */
  let paletteActiveIndex = 0;
  let paletteResults = [];

  function paletteAllItems() {
    return Object.keys(COMMANDS).map((id) => ({ id, ...COMMANDS[id] }));
  }
  function renderPaletteList(query) {
    const listEl = $("[data-palette-list]");
    const q = query.trim().toLowerCase();
    paletteResults = paletteAllItems().filter(
      (c) => !q || c.label.toLowerCase().includes(q),
    );
    paletteActiveIndex = 0;
    if (!paletteResults.length) {
      listEl.innerHTML = `<p class="vn-Palette__empty">No matching command.</p>`;
      return;
    }
    listEl.innerHTML = paletteResults
      .map(
        (c, i) =>
          `<button type="button" class="vn-Palette__item ${i === 0 ? "is-active" : ""}" data-palette-cmd="${c.id}" data-index="${i}">
        <span class="vn-Menu__icon">${menuIcon(c.id)}</span>
        <span>${escapeHtml(c.label)}</span>
        ${c.hint ? `<span class="vn-Menu__hint">${c.hint}</span>` : ""}
      </button>`,
      )
      .join("");
  }
  function setPaletteActive(i) {
    const items = $all("[data-palette-cmd]");
    if (!items.length) return;
    paletteActiveIndex = (i + items.length) % items.length;
    items.forEach((el, idx) =>
      el.classList.toggle("is-active", idx === paletteActiveIndex),
    );
    items[paletteActiveIndex].scrollIntoView({ block: "nearest" });
  }
  function openPalette() {
    const p = $("[data-palette]");
    const input = $("[data-palette-input]");
    if (!p || !input) return;
    p.hidden = false;
    input.value = "";
    renderPaletteList("");
    input.focus();
  }
  function closePalette() {
    const p = $("[data-palette]");
    if (p) p.hidden = true;
  }
  function bindPalette() {
    const p = $("[data-palette]");
    if (!p) return;
    const input = $("[data-palette-input]");
    const listEl = $("[data-palette-list]");

    input.addEventListener("input", () => renderPaletteList(input.value));
    input.addEventListener("keydown", (e) => {
      if (e.key === "ArrowDown") {
        e.preventDefault();
        setPaletteActive(paletteActiveIndex + 1);
      } else if (e.key === "ArrowUp") {
        e.preventDefault();
        setPaletteActive(paletteActiveIndex - 1);
      } else if (e.key === "Enter") {
        e.preventDefault();
        const item = paletteResults[paletteActiveIndex];
        if (item) {
          closePalette();
          runCommand(item.id);
        }
      } else if (e.key === "Escape") {
        e.preventDefault();
        closePalette();
      }
    });
    listEl.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-palette-cmd]");
      if (!btn) return;
      closePalette();
      runCommand(btn.getAttribute("data-palette-cmd"));
    });
    for (const c of $all("[data-palette-close]"))
      c.addEventListener("click", closePalette);
  }

  /* ----------------------------------------------------------
   * Modal (shortcuts / about)
   * -------------------------------------------------------- */
  function openModal(title, bodyHtml) {
    const modal = $("[data-modal]");
    if (!modal) return;
    setText("[data-modal-title]", title);
    const body = $("[data-modal-body]");
    if (body) body.innerHTML = bodyHtml;
    modal.hidden = false;
  }
  function closeModal() {
    const modal = $("[data-modal]");
    if (modal) modal.hidden = true;
  }
  function bindModal() {
    for (const c of $all("[data-modal-close]"))
      c.addEventListener("click", closeModal);
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
      ["Indent", "Tab"],
      ["Global", ""],
      ["Save note", "Ctrl/⌘ + S"],
      ["Toggle sidebar", "Ctrl/⌘ + B"],
      ["Command palette", "Ctrl/⌘ + Shift + P"],
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
    openModal("Keyboard shortcuts", html);
  }
  function showAbout() {
    openModal(
      "About Vix Note",
      `
      <p><strong>Vix Note</strong> — visual executable notes for learning C++ and Vix.cpp faster.</p>
      <p>Explanations, C++ cells, Reply cells, HTML cells, outputs and project context live together in one document.</p>
      <p style="color:var(--vn-text3)">Part of the Vix.cpp ecosystem · MIT License · © 2026 Gaspard Kirira.</p>
    `,
    );
  }

  /* ----------------------------------------------------------
   * Toolbar + sidebar wiring
   * -------------------------------------------------------- */
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
          toggleSidebar();
          break;
        case "palette":
          openPalette();
          break;
        case "shortcuts":
          showShortcuts();
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
        case "add-cpp":
          addCell("cpp");
          break;
        case "add-reply":
          addCell("reply");
          break;
        case "add-markdown":
          addCell("markdown");
          break;
        case "add-html":
          addCell("html");
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

  /* ----------------------------------------------------------
   * Cell interactions
   * -------------------------------------------------------- */
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
        if (a === "run") {
          runCellById(id);
          return;
        }
        if (a === "edit") {
          selectCell(id, { edit: true });
          return;
        }
        if (a === "duplicate") {
          duplicateCell(id);
          return;
        }
        if (a === "up") {
          moveCellById(id, "up");
          return;
        }
        if (a === "down") {
          moveCellById(id, "down");
          return;
        }
        if (a === "delete") {
          deleteCellById(id);
          return;
        }
        if (a === "select") {
          selectCell(id, { edit: false });
          return;
        }
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
      autosize(ta);
      updateHighlight(ta);
      const cellEl = ta.closest(".vn-Cell");
      if (!cellEl) return;
      const cell = findCell(cellEl.dataset.cellId);
      if (cell) cell.source = ta.value;
      setDirty(true);
      const kind = cellEl.dataset.kind;
      if (kind === "markdown") {
        const r = $("[data-rendered]", cellEl);
        if (r) r.innerHTML = renderMarkdown(ta.value);
      } else if (kind === "html") {
        const r = $("[data-rendered]", cellEl);
        if (r) r.innerHTML = String(ta.value || "");
      }
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

  /* ----------------------------------------------------------
   * Keyboard
   * -------------------------------------------------------- */
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

      // command palette
      if (meta && event.shiftKey && event.key.toLowerCase() === "p") {
        event.preventDefault();
        openPalette();
        return;
      }
      if (meta && event.key === "Enter") {
        event.preventDefault();
        if (state.selectedId) await runCellById(state.selectedId);
        if (inTextarea) enterCommandMode();
        return;
      }
      if (
        event.shiftKey &&
        event.key === "Enter" &&
        !($("[data-palette-input]") === event.target)
      ) {
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

      // edit mode
      if (state.editing && inTextarea) {
        if (event.key === "Escape") {
          event.preventDefault();
          enterCommandMode();
          return;
        }
        if (event.key === "Tab") {
          event.preventDefault();
          const ta = event.target;
          const s = ta.selectionStart,
            e = ta.selectionEnd;
          ta.value = ta.value.slice(0, s) + "  " + ta.value.slice(e);
          ta.selectionStart = ta.selectionEnd = s + 2;
          autosize(ta);
          updateHighlight(ta);
        }
        return;
      }

      if (inField) return; // don't hijack palette/input typing

      // '?' opens shortcuts
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

  function bindSidebarTOC() {
    const toc = $(sel.toc);
    if (!toc) return;
    toc.addEventListener("click", (event) => {
      const btn =
        event.target instanceof Element
          ? event.target.closest("[data-toc-cell]")
          : null;
      if (!btn) return;
      const id = btn.getAttribute("data-toc-cell");
      selectCell(id, { edit: false });
      const el = cellElById(id);
      if (el) el.scrollIntoView({ block: "start", behavior: "smooth" });
      // auto-close drawer on small screens
      if (window.matchMedia("(max-width: 900px)").matches) toggleSidebar(true);
    });
  }

  /* ----------------------------------------------------------
   * Init
   * -------------------------------------------------------- */
  function init() {
    applySidebarWidth(DEFAULT_SIDEBAR_WIDTH);
    // collapse sidebar by default on small screens
    if (window.matchMedia("(max-width: 900px)").matches) toggleSidebar(true);

    bindActions();
    bindMenus();
    bindPalette();
    bindModal();
    bindSidebarResize();
    bindCellInteractions();
    bindKeyboard();
    bindSidebarTOC();
    setKernel("idle");
    loadDocument();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
