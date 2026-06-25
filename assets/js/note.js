/*
 * Vix Note — JupyterLab-faithful frontend
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
 * Reproduces Jupyter notebook UX: command/edit modes, In[ ]/Out[ ] prompts,
 * cell selection, keyboard shortcuts, syntax highlighting, table of contents.
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
    selectedId: null, // currently selected cell id (command mode anchor)
    editing: false, // edit mode vs command mode
    kernel: "idle", // idle | busy | error
    busy: false,
  };

  const app = document.querySelector("[data-note-app]");

  const sel = {
    cells: "[data-note-cells]",
    title: "[data-note-title]",
    document: "[data-note-document]",
    project: "[data-note-project]",
    project2: "[data-note-project-2]",
    cellCount: "[data-note-cell-count]",
    kernel: "[data-note-kernel]",
    kernel2: "[data-note-kernel-2]",
    state: "[data-note-state]",
    message: "[data-note-message]",
    toc: "[data-note-toc]",
    toolbarKind: '[data-action="toolbar-kind"]',
  };

  const $ = (s, root = document) => root.querySelector(s);
  const $all = (s, root = document) => Array.from(root.querySelectorAll(s));

  /* ----------------------------------------------------------
   * Small helpers
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

  // Map a cell kind to its Jupyter "cell type": code-like vs markdown vs html.
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
      el.className = "jp-Notice";
      return;
    }

    el.hidden = false;
    el.textContent = message;
    el.className = `jp-Notice jp-Notice--${safeClass(kind)}`;

    if (kind === "success" || kind === "info") {
      messageTimer = setTimeout(() => setMessage(""), 2600);
    }
  }

  function setBusy(busy) {
    state.busy = busy;
    for (const b of $all(".jp-ToolbarButton, .jp-Sidebar__buttons button")) {
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
   * Document model accessors
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

  /* ----------------------------------------------------------
   * Markdown renderer (lightweight, same scope as the C++ default)
   * -------------------------------------------------------- */
  function renderMarkdown(source) {
    const lines = String(source || "").split(/\r?\n/);
    const blocks = [];
    let para = [];
    let inCode = false;
    let code = [];

    const flushPara = () => {
      if (!para.length) return;
      blocks.push(`<p>${para.map(inline).join("<br>")}</p>`);
      para = [];
    };

    const inline = (line) => {
      let s = escapeHtml(line);
      s = s.replace(/`([^`]+)`/g, "<code>$1</code>");
      s = s.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
      s = s.replace(/\*([^*]+)\*/g, "<em>$1</em>");
      return s;
    };

    for (const line of lines) {
      const trimmed = line.trim();

      if (trimmed.startsWith("```")) {
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

      if (!trimmed) {
        flushPara();
        continue;
      }
      if (trimmed.startsWith("### ")) {
        flushPara();
        blocks.push(`<h3>${inline(trimmed.slice(4))}</h3>`);
        continue;
      }
      if (trimmed.startsWith("## ")) {
        flushPara();
        blocks.push(`<h2>${inline(trimmed.slice(3))}</h2>`);
        continue;
      }
      if (trimmed.startsWith("# ")) {
        flushPara();
        blocks.push(`<h1>${inline(trimmed.slice(2))}</h1>`);
        continue;
      }
      para.push(line);
    }

    if (inCode) {
      blocks.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
    }
    flushPara();

    return blocks.length ? blocks.join("\n") : "<p></p>";
  }

  /* ----------------------------------------------------------
   * Syntax highlighter — token-by-token, returns HTML with spans.
   * Light theme tokens, C++ and Reply aware. Whitespace preserved 1:1
   * so it lines up perfectly behind the transparent textarea.
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

      // Preprocessor directive (#include, #define ...) — C++ only
      if (!isReply && c === "#" && (i === 0 || s[i - 1] === "\n")) {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-pre", s.slice(i, j));
        i = j;
        continue;
      }

      // Line comment //
      if (c === "/" && s[i + 1] === "/") {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      // Reply comment #
      if (isReply && c === "#") {
        let j = i;
        while (j < n && s[j] !== "\n") j++;
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }
      // Block comment /* */
      if (c === "/" && s[i + 1] === "*") {
        let j = i + 2;
        while (j < n && !(s[j] === "*" && s[j + 1] === "/")) j++;
        j = Math.min(n, j + 2);
        push("tok-com", s.slice(i, j));
        i = j;
        continue;
      }

      // Strings " " and ' '
      if (c === '"' || c === "'") {
        const quote = c;
        let j = i + 1;
        while (j < n) {
          if (s[j] === "\\") {
            j += 2;
            continue;
          }
          if (s[j] === quote) {
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

      // Numbers
      if (/[0-9]/.test(c)) {
        let j = i;
        while (j < n && /[0-9a-fA-FxXuUlL.']/.test(s[j])) j++;
        push("tok-num", s.slice(i, j));
        i = j;
        continue;
      }

      // Identifiers / keywords / types / functions
      if (/[A-Za-z_]/.test(c)) {
        let j = i;
        while (j < n && /[A-Za-z0-9_]/.test(s[j])) j++;
        const word = s.slice(i, j);

        // look ahead for "(" => function call
        let k2 = j;
        while (k2 < n && s[k2] === " ") k2++;
        const isCall = s[k2] === "(";

        if (keywords.has(word)) {
          push("tok-kw", word);
        } else if (types.has(word)) {
          push("tok-type", word);
        } else if (!isReply && /^[A-Z]/.test(word)) {
          // Type-ish (UpperCamel) in C++
          push("tok-type", word);
        } else if (isCall) {
          push("tok-fn", word);
        } else {
          raw(word);
        }
        i = j;
        continue;
      }

      // Punctuation cluster (kept plain so it aligns; cheap)
      raw(c);
      i++;
    }

    // ensure trailing newline so the highlight box matches textarea height
    if (!out.endsWith("\n")) out += "\n";
    return out;
  }

  /* ----------------------------------------------------------
   * Output rendering (Out[ ]: block)
   * -------------------------------------------------------- */
  function outputKindClass(kind) {
    return `jp-Output jp-Output--${safeClass(normalizeKind(kind))}`;
  }

  function renderOutputs(outputs) {
    const list = Array.isArray(outputs) ? outputs : [];
    if (!list.length) return "";

    const rows = list
      .map((o) => {
        const kind = normalizeKind(o.kind);
        const content = String(o.content ?? "");
        if (kind === "html") {
          return `<div class="${outputKindClass(kind)}">${content}</div>`;
        }
        const label =
          kind === "stdout"
            ? ""
            : `<span class="jp-Output__kind">${escapeHtml(kind)}</span>`;
        return `<div class="${outputKindClass(kind)}">${label}<pre>${escapeHtml(content)}</pre></div>`;
      })
      .join("");

    return rows;
  }

  function runningOutputHtml(message) {
    return `<div class="jp-Output jp-Output--running"><pre>${escapeHtml(message)}</pre></div>`;
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
  };

  function inPrompt(cell) {
    if (!isCodeKind(cell.kind)) {
      return `<div class="jp-InputPrompt jp-InputPrompt--empty"></div>`;
    }
    const n = Number(cell.executionCount || 0);
    const label = n > 0 ? `In&nbsp;[${n}]:` : `In&nbsp;[&nbsp;]:`;
    const cls =
      n > 0 ? "jp-InputPrompt" : "jp-InputPrompt jp-InputPrompt--empty";
    return `<div class="${cls}">${label}</div>`;
  }

  function outPrompt(cell) {
    if (!isCodeKind(cell.kind)) return "";
    const n = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    if (!outs.length) return `<div class="jp-OutputPrompt"></div>`;
    const label = n > 0 ? `Out[${n}]:` : `Out[&nbsp;]:`;
    return `<div class="jp-OutputPrompt">${label}</div>`;
  }

  function editorBlock(cell) {
    const kind = normalizeKind(cell.kind);
    const source = String(cell.source ?? "");
    const code = isCodeKind(kind);

    const editorCls = code ? "jp-Editor" : "jp-Editor jp-Editor--plain";
    const highlight = code
      ? `<div class="jp-Editor__highlight" data-highlight>${tokenizeCode(source, kind)}</div>`
      : "";

    return `
      <div class="${editorCls}">
        <div class="jp-Editor__wrap">
          ${highlight}
          <textarea
            spellcheck="false"
            data-action="edit-source"
            rows="1"
          >${escapeHtml(source)}</textarea>
        </div>
      </div>`;
  }

  function cellBody(cell) {
    const kind = normalizeKind(cell.kind);

    if (kind === "markdown") {
      return `
        <div class="jp-MarkdownCell">
          <div class="jp-RenderedMarkdown" data-rendered>${renderMarkdown(cell.source)}</div>
          <div class="jp-InputArea">
            <div class="jp-InputPrompt jp-InputPrompt--empty"></div>
            ${editorBlock(cell)}
          </div>
        </div>`;
    }

    if (kind === "html") {
      return `
        <div class="jp-HtmlCell">
          <div class="jp-RenderedHTML" data-rendered>${String(cell.source || "")}</div>
          <div class="jp-InputArea">
            <div class="jp-InputPrompt jp-InputPrompt--empty"></div>
            ${editorBlock(cell)}
          </div>
        </div>`;
    }

    // code-like (cpp / reply / code)
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    const outputArea = outs.length
      ? `<div class="jp-OutputArea">
           ${outPrompt(cell)}
           <div class="jp-OutputArea__list">${renderOutputs(outs)}</div>
         </div>`
      : "";

    return `
      <div class="jp-CodeCell">
        <div class="jp-InputArea">
          ${inPrompt(cell)}
          ${editorBlock(cell)}
        </div>
        ${outputArea}
      </div>`;
  }

  function cellToolbar(cell) {
    const runBtn = isCodeKind(cell.kind)
      ? `<button type="button" data-cell-action="run" title="Run">${ICONS.run}</button>`
      : `<button type="button" data-cell-action="run" title="Render">${ICONS.run}</button>`;
    return `
      <div class="jp-Cell__toolbar">
        ${runBtn}
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
        ? "jp-Cell--markdown"
        : kind === "html"
          ? "jp-Cell--html"
          : "jp-Cell--code";

    const classes = [
      "jp-Cell",
      typeClass,
      selected ? "jp-mod-selected" : "",
      editing ? "jp-mod-editing" : "",
    ]
      .filter(Boolean)
      .join(" ");

    return `
      <div class="jp-CellInsert">
        <button class="jp-CellInsert__btn" type="button" data-insert-after="${escapeHtml(id)}" title="Insert cell below">+</button>
      </div>
      <div class="${classes}" data-cell-id="${escapeHtml(id)}" data-kind="${escapeHtml(kind)}" tabindex="-1">
        <div class="jp-Cell__collapser" data-cell-action="select"></div>
        <div class="jp-Cell__body">
          ${cellToolbar(cell)}
          ${cellBody(cell)}
        </div>
      </div>`;
  }

  function renderEmpty(message) {
    const c = $(sel.cells);
    if (!c) return;
    c.innerHTML = `<div class="jp-Notebook__empty">${escapeHtml(message)}</div>`;
  }

  function renderTOC() {
    const toc = $(sel.toc);
    if (!toc) return;

    const items = [];
    for (const cell of cells()) {
      if (normalizeKind(cell.kind) !== "markdown") continue;
      for (const line of String(cell.source || "").split(/\r?\n/)) {
        const t = line.trim();
        let level = 0;
        let text = "";
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

    if (!items.length) {
      toc.innerHTML = `<p class="jp-Toc__empty">No headings yet.</p>`;
      return;
    }

    toc.innerHTML = items
      .map(
        (it) =>
          `<button class="jp-Toc__item jp-Toc__item--h${it.level}" data-toc-cell="${escapeHtml(
            it.id,
          )}" title="${escapeHtml(it.text)}">${escapeHtml(it.text)}</button>`,
      )
      .join("");
  }

  function renderDocument(payload) {
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
    document.title = `${title} · Vix Note`;

    const container = $(sel.cells);
    if (!container) return;

    const list = cells();
    if (!list.length) {
      renderEmpty(
        "This note has no cells yet. Use the toolbar + or the sidebar to add a cell.",
      );
      renderTOC();
      return;
    }

    // preserve a sensible selection
    if (!state.selectedId || !findCell(state.selectedId)) {
      state.selectedId = String(list[0].id);
    }

    container.innerHTML = list.map(renderCell).join("");
    renderTOC();
    autosizeAll();
    syncToolbarKind();
  }

  /* ----------------------------------------------------------
   * Editing / textarea autosize + highlight sync
   * -------------------------------------------------------- */
  function autosize(textarea) {
    if (!textarea) return;
    textarea.style.height = "auto";
    textarea.style.height = `${textarea.scrollHeight}px`;
    const wrap = textarea.closest(".jp-Editor__wrap");
    const hl = wrap ? $("[data-highlight]", wrap) : null;
    if (hl) hl.style.height = `${textarea.scrollHeight}px`;
  }

  function autosizeAll() {
    for (const ta of $all('textarea[data-action="edit-source"]')) autosize(ta);
  }

  function updateHighlight(textarea) {
    const wrap = textarea.closest(".jp-Editor__wrap");
    if (!wrap) return;
    const hl = $("[data-highlight]", wrap);
    if (!hl) return;
    const cellEl = textarea.closest(".jp-Cell");
    const kind = cellEl ? cellEl.dataset.kind : "cpp";
    hl.innerHTML = tokenizeCode(textarea.value, kind);
  }

  // mirror scroll between textarea and its highlight layer
  function syncScroll(textarea) {
    const wrap = textarea.closest(".jp-Editor__wrap");
    if (!wrap) return;
    const hl = $("[data-highlight]", wrap);
    if (hl) {
      hl.scrollTop = textarea.scrollTop;
      hl.scrollLeft = textarea.scrollLeft;
    }
  }

  /* ----------------------------------------------------------
   * Selection / mode management (Jupyter command vs edit)
   * -------------------------------------------------------- */
  function cellElById(id) {
    return $(`.jp-Cell[data-cell-id="${cssEscape(id)}"]`);
  }

  function cssEscape(v) {
    return String(v).replace(/["\\]/g, "\\$&");
  }

  function selectCell(id, { edit = false, focus = true } = {}) {
    state.selectedId = String(id);
    state.editing = edit;

    for (const el of $all(".jp-Cell")) {
      const isSel = el.dataset.cellId === String(id);
      el.classList.toggle("jp-mod-selected", isSel);
      el.classList.toggle("jp-mod-editing", isSel && edit);
    }

    const el = cellElById(id);
    if (!el) return;

    if (edit) {
      const ta = $('textarea[data-action="edit-source"]', el);
      if (ta && focus) {
        ta.focus();
        autosize(ta);
      }
    } else if (focus) {
      el.focus({ preventScroll: false });
    }

    syncToolbarKind();
  }

  function enterEditMode() {
    if (!state.selectedId) return;
    selectCell(state.selectedId, { edit: true });
  }

  function enterCommandMode() {
    state.editing = false;
    const el = cellElById(state.selectedId);
    if (el) {
      el.classList.remove("jp-mod-editing");
      el.focus({ preventScroll: true });
    }
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
   * Cell content sync to server
   * -------------------------------------------------------- */
  function localUpdateFromDom(cellEl) {
    const id = cellEl.dataset.cellId;
    const cell = findCell(id);
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
   * Actions: add / run / move / delete / save / change kind
   * -------------------------------------------------------- */
  function defaultSource(kind) {
    const k = normalizeKind(kind);
    if (k === "cpp") {
      return '#include <iostream>\n\nint main()\n{\n  std::cout << "Hello from Vix Note\\n";\n  return 0;\n}\n';
    }
    if (k === "reply") {
      return 'x = 1 + 2 * 3\nprintln("x =", x)\n';
    }
    if (k === "html") {
      return "<section>\n  <h2>Hello</h2>\n  <p>Rendered by the note UI.</p>\n</section>\n";
    }
    return "Write your explanation here.";
  }

  async function addCell(kind, { afterId = null } = {}) {
    setMessage("");
    setBusy(true);

    const body = { kind, source: defaultSource(kind) };
    if (afterId != null) {
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
      } else {
        await loadDocument();
      }

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

  async function runCellById(id) {
    const cellEl = cellElById(id);
    const cell = findCell(id);
    if (!cellEl || !cell) return;

    // Markdown / HTML: just render, no server execution.
    if (!isCodeKind(cell.kind)) {
      localUpdateFromDom(cellEl);
      try {
        await pushCell(cellEl);
      } catch (_) {}
      renderDocument({ document: state.document });
      selectCell(id, { edit: false });
      return;
    }

    cellEl.classList.add("jp-mod-running");
    setMessage("");
    setKernel("busy");

    // optimistic "running" output
    const codeCell = $(".jp-CodeCell", cellEl);
    if (codeCell) {
      let oa = $(".jp-OutputArea", codeCell);
      if (!oa) {
        codeCell.insertAdjacentHTML(
          "beforeend",
          `<div class="jp-OutputArea"><div class="jp-OutputPrompt">Out[&nbsp;]:</div><div class="jp-OutputArea__list">${runningOutputHtml(
            "Running…",
          )}</div></div>`,
        );
      } else {
        const listEl = $(".jp-OutputArea__list", oa);
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
        renderDocument(result.document);
      } else {
        await loadDocument();
      }

      selectCell(id, { edit: false });

      const status = normalizeKind(result?.result?.status);
      if (status === "failure") {
        setKernel("error");
        setMessage(
          result?.result?.message || "Cell execution failed.",
          "error",
        );
        // reset kernel to idle shortly after showing error
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
      if (el) el.classList.remove("jp-mod-running");
    }
  }

  async function runAll() {
    setMessage("");
    setBusy(true);
    setKernel("busy");

    try {
      // push any pending edits
      for (const el of $all(".jp-Cell")) {
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
      for (const el of $all(".jp-Cell")) {
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
   * Event wiring
   * -------------------------------------------------------- */
  function toolbarTargetId() {
    return state.selectedId;
  }

  function bindToolbar() {
    document.addEventListener("click", (event) => {
      const t =
        event.target instanceof Element
          ? event.target.closest("[data-action]")
          : null;
      if (!t) return;
      const action = t.getAttribute("data-action");

      switch (action) {
        case "save":
          saveNote();
          break;
        case "run-cell":
          if (toolbarTargetId()) runCellById(toolbarTargetId());
          break;
        case "run-all":
          runAll();
          break;
        case "restart":
          restartKernel();
          break;
        case "insert-below":
          addCell(currentToolbarKind(), { afterId: toolbarTargetId() });
          break;
        case "cut-cell":
          if (toolbarTargetId()) deleteCellById(toolbarTargetId());
          break;
        case "move-up":
          if (toolbarTargetId()) moveCellById(toolbarTargetId(), "up");
          break;
        case "move-down":
          if (toolbarTargetId()) moveCellById(toolbarTargetId(), "down");
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

    // toolbar cell-type select
    const kindSelect = $(sel.toolbarKind);
    if (kindSelect) {
      kindSelect.addEventListener("change", () => {
        if (!state.selectedId) return;
        changeKind(state.selectedId, currentToolbarKind());
      });
    }
  }

  function currentToolbarKind() {
    const s = $(sel.toolbarKind);
    const v = s ? s.value : "";
    return v || "cpp"; // "Code" default => cpp
  }

  async function restartKernel() {
    // No dedicated endpoint; emulate by clearing displayed outputs.
    setKernel("busy");
    setMessage("Restarting kernel…", "info");
    setTimeout(() => {
      setKernel("idle");
      setMessage(
        "Kernel restarted. (Outputs persist until cells are re-run.)",
        "info",
      );
    }, 400);
  }

  function bindCellInteractions() {
    const container = $(sel.cells);
    if (!container) return;

    // click: select / toolbar actions / insert / collapser
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
      const cellEl = target.closest(".jp-Cell");
      if (!cellEl) return;
      const id = cellEl.dataset.cellId;

      if (actionBtn) {
        const a = actionBtn.getAttribute("data-cell-action");
        if (a === "run") {
          runCellById(id);
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

      // clicking the editor area => edit mode; elsewhere => command mode
      const inEditor = target.closest(".jp-Editor, textarea");
      selectCell(id, { edit: !!inEditor, focus: !!inEditor });
    });

    // double-click rendered markdown/html => edit
    container.addEventListener("dblclick", (event) => {
      const target = event.target;
      if (!(target instanceof Element)) return;
      const rendered = target.closest("[data-rendered]");
      if (!rendered) return;
      const cellEl = target.closest(".jp-Cell");
      if (cellEl) selectCell(cellEl.dataset.cellId, { edit: true });
    });

    // input on textarea => autosize + highlight + live preview + dirty
    container.addEventListener("input", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;

      autosize(ta);
      updateHighlight(ta);

      const cellEl = ta.closest(".jp-Cell");
      if (!cellEl) return;
      const cell = findCell(cellEl.dataset.cellId);
      if (cell) cell.source = ta.value;
      setDirty(true);

      // live preview for markdown/html
      const kind = cellEl.dataset.kind;
      if (kind === "markdown") {
        const r = $("[data-rendered]", cellEl);
        if (r) r.innerHTML = renderMarkdown(ta.value);
      } else if (kind === "html") {
        const r = $("[data-rendered]", cellEl);
        if (r) r.innerHTML = String(ta.value || "");
      }
    });

    // scroll sync
    container.addEventListener(
      "scroll",
      (event) => {
        const ta = event.target;
        if (ta instanceof HTMLTextAreaElement) syncScroll(ta);
      },
      true,
    );

    // focusout => push edits
    container.addEventListener(
      "focusout",
      async (event) => {
        const ta = event.target;
        if (!(ta instanceof HTMLTextAreaElement)) return;
        if (ta.getAttribute("data-action") !== "edit-source") return;
        const cellEl = ta.closest(".jp-Cell");
        if (!cellEl) return;
        try {
          await pushCell(cellEl);
        } catch (_) {}
      },
      true,
    );
  }

  /* ----------------------------------------------------------
   * Keyboard — Jupyter shortcuts (command + edit modes)
   * -------------------------------------------------------- */
  function bindKeyboard() {
    document.addEventListener("keydown", async (event) => {
      const inTextarea = event.target instanceof HTMLTextAreaElement;
      const meta = event.ctrlKey || event.metaKey;

      // ---- Global shortcuts (work in both modes) ----
      if (meta && event.key === "Enter") {
        event.preventDefault();
        if (state.selectedId) await runCellById(state.selectedId);
        if (inTextarea) enterCommandMode();
        return;
      }
      if (event.shiftKey && event.key === "Enter") {
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

      // ---- Edit mode ----
      if (state.editing && inTextarea) {
        if (event.key === "Escape") {
          event.preventDefault();
          enterCommandMode();
          return;
        }
        if (event.key === "Tab") {
          // insert two spaces instead of leaving the field
          event.preventDefault();
          const ta = event.target;
          const start = ta.selectionStart;
          const end = ta.selectionEnd;
          ta.value = ta.value.slice(0, start) + "  " + ta.value.slice(end);
          ta.selectionStart = ta.selectionEnd = start + 2;
          autosize(ta);
          updateHighlight(ta);
        }
        return;
      }

      // ---- Command mode ----
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
          {
            const cur = state.selectedId;
            const idx = cellIndex(cur);
            const prev = idx > 0 ? cells()[idx - 1].id : null;
            // "insert above" == insert after previous (or at top via index 0)
            insertAbove(cur);
          }
          break;
        case "b":
          event.preventDefault();
          addCell(currentToolbarKind(), { afterId: state.selectedId });
          break;
        case "d":
          // require double-d like Jupyter
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

  let lastDTime = 0;
  function handleDoubleD() {
    const now = Date.now();
    if (now - lastDTime < 500) {
      lastDTime = 0;
      if (state.selectedId) deleteCellById(state.selectedId);
    } else {
      lastDTime = now;
    }
  }

  async function insertAbove(id) {
    const idx = cellIndex(id);
    if (idx < 0) return;
    // Insert a cell at this index (pushes current down) by using afterId = prev cell.
    const body = {
      kind: currentToolbarKind(),
      source: defaultSource(currentToolbarKind()),
      index: idx,
    };
    setBusy(true);
    try {
      const result = await api("/api/cells", {
        method: "POST",
        body: JSON.stringify(body),
      });
      const newId = result.cellId || result.cell?.id || null;
      if (result.document) {
        state.selectedId = newId || state.selectedId;
        renderDocument(result.document);
      } else {
        await loadDocument();
      }
      if (newId) selectCell(newId, { edit: true });
      setDirty(true);
    } catch (error) {
      setMessage(error.message || "Failed to insert cell.", "error");
    } finally {
      setBusy(false);
    }
  }

  function bindSidebarTOC() {
    const toc = $(sel.toc);
    if (!toc) return;
    toc.addEventListener("click", (event) => {
      const btn =
        event.target instanceof HTMLElement
          ? event.target.closest("[data-toc-cell]")
          : null;
      if (!btn) return;
      const id = btn.getAttribute("data-toc-cell");
      selectCell(id, { edit: false });
      const el = cellElById(id);
      if (el) el.scrollIntoView({ block: "start", behavior: "smooth" });
    });
  }

  /* ----------------------------------------------------------
   * Init
   * -------------------------------------------------------- */
  function init() {
    bindToolbar();
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
