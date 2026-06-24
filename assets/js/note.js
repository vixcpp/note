(() => {
  "use strict";

  const state = {
    document: null,
    kernelStatus: "Idle",
    running: false,
    dirty: false,
    lastRunAt: null,
    lastSaveAt: null,
  };

  const selectors = {
    cells: "[data-note-cells]",
    title: "[data-note-title]",
    document: "[data-note-document]",
    cellCount: "[data-note-cell-count]",
    kernel: "[data-note-kernel]",
    noteState: "[data-note-state]",
    message: "[data-note-message]",
    runAll: '[data-action="run-all"]',
    save: '[data-action="save"]',
    addMarkdown: '[data-action="add-markdown"]',
    addCpp: '[data-action="add-cpp"]',
  };

  function $(selector, root = document) {
    return root.querySelector(selector);
  }

  function $all(selector, root = document) {
    return Array.from(root.querySelectorAll(selector));
  }

  function escapeHtml(value) {
    return String(value ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function safeClassName(value) {
    return String(value || "unknown")
      .toLowerCase()
      .replace(/[^a-z0-9_-]/g, "-");
  }

  function normalizeKind(kind) {
    return String(kind || "unknown").toLowerCase();
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
        return "Unknown";
    }
  }

  function setText(selector, value) {
    const element = $(selector);

    if (element) {
      element.textContent = value;
    }
  }

  function setKernelStatus(value) {
    state.kernelStatus = value;
    setText(selectors.kernel, value);
  }

  function setNoteState(value) {
    setText(selectors.noteState, value);
  }

  function setDirty(value) {
    state.dirty = Boolean(value);
    setNoteState(state.dirty ? "Unsaved" : "Saved");
  }

  function setMessage(message, kind = "warning") {
    const element = $(selectors.message);

    if (!element) {
      return;
    }

    if (!message) {
      element.hidden = true;
      element.textContent = "";
      element.className = "note-message";
      return;
    }

    element.hidden = false;
    element.textContent = message;
    element.className = `note-message note-message--${safeClassName(kind)}`;
  }

  function setBusy(busy) {
    state.running = busy;

    for (const button of $all("button")) {
      button.disabled = busy;
    }

    for (const textarea of $all("textarea")) {
      textarea.disabled = busy;
    }

    for (const select of $all("select")) {
      select.disabled = busy;
    }
  }

  function setButtonBusy(button, busy, label) {
    if (!button) {
      return;
    }

    if (busy) {
      button.dataset.previousText = button.textContent;
      button.textContent = label;
      button.disabled = true;
      return;
    }

    button.textContent = button.dataset.previousText || button.textContent;
    button.disabled = false;
    delete button.dataset.previousText;
  }

  async function api(path, options = {}, config = {}) {
    const headers = {
      Accept: "application/json",
      ...(options.body ? { "Content-Type": "application/json" } : {}),
      ...(options.headers || {}),
    };

    const response = await fetch(path, {
      ...options,
      headers,
    });

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
      const message =
        body.error ||
        body.message ||
        body?.result?.message ||
        `Request failed with status ${response.status}`;

      throw new Error(message);
    }

    return body;
  }

  function unwrapDocument(payload) {
    if (!payload) {
      return null;
    }

    if (payload.document) {
      return payload.document;
    }

    return payload;
  }

  function renderMarkdown(source) {
    const lines = String(source || "").split(/\r?\n/);
    const blocks = [];
    let paragraph = [];

    function flushParagraph() {
      if (paragraph.length === 0) {
        return;
      }

      const content = paragraph.map(escapeHtml).join("\n");
      blocks.push(`<p>${content}</p>`);
      paragraph = [];
    }

    for (const line of lines) {
      const trimmed = line.trim();

      if (!trimmed) {
        flushParagraph();
        continue;
      }

      if (trimmed.startsWith("### ")) {
        flushParagraph();
        blocks.push(`<h3>${escapeHtml(trimmed.slice(4).trim())}</h3>`);
        continue;
      }

      if (trimmed.startsWith("## ")) {
        flushParagraph();
        blocks.push(`<h2>${escapeHtml(trimmed.slice(3).trim())}</h2>`);
        continue;
      }

      if (trimmed.startsWith("# ")) {
        flushParagraph();
        blocks.push(`<h1>${escapeHtml(trimmed.slice(2).trim())}</h1>`);
        continue;
      }

      paragraph.push(line);
    }

    flushParagraph();

    if (blocks.length === 0) {
      return "<p></p>";
    }

    return blocks.join("\n");
  }

  function renderOutputs(outputs = []) {
    if (!outputs.length) {
      return "";
    }

    const rendered = outputs
      .map((output) => {
        const kind = normalizeKind(output.kind);
        const cssKind = safeClassName(kind);
        const content = String(output.content ?? "");

        if (kind === "html") {
          return `
            <section class="note-output note-output--html">
              <span class="note-output__kind">html</span>
              <div>${content}</div>
            </section>
          `;
        }

        return `
          <section class="note-output note-output--${cssKind}">
            <span class="note-output__kind">${escapeHtml(kind)}</span>
            <pre>${escapeHtml(content)}</pre>
          </section>
        `;
      })
      .join("");

    return `<div class="note-outputs" id="outputs">${rendered}</div>`;
  }

  function renderCellBody(cell) {
    const kind = normalizeKind(cell.kind);
    const source = String(cell.source ?? "");

    if (kind === "markdown") {
      return `
        <div class="note-cell__editor">
          <textarea spellcheck="false" data-action="edit-source">${escapeHtml(source)}</textarea>
        </div>

        <div class="note-preview">
          ${renderMarkdown(source)}
        </div>
      `;
    }

    if (kind === "html") {
      return `
        <div class="note-cell__editor">
          <textarea spellcheck="false" data-action="edit-source">${escapeHtml(source)}</textarea>
        </div>

        <div class="note-html-preview">
          ${source}
        </div>
      `;
    }

    return `
      <div class="note-cell__editor">
        <textarea spellcheck="false" data-action="edit-source">${escapeHtml(source)}</textarea>
      </div>
    `;
  }

  function renderCell(cell) {
    const kind = normalizeKind(cell.kind);
    const index = Number(cell.index ?? 0);
    const id = String(cell.id || `cell-${index + 1}`);
    const executable = Boolean(cell.executable);
    const executionCount = Number(cell.executionCount || 0);
    const outputs = Array.isArray(cell.outputs) ? cell.outputs : [];

    const hasFailure = outputs.some((output) => {
      const outputKind = normalizeKind(output.kind);

      return (
        outputKind === "error" ||
        outputKind === "stderr" ||
        outputKind === "compiler_error" ||
        outputKind === "runtime_error"
      );
    });

    const classes = [
      "note-cell",
      `note-cell--${safeClassName(kind)}`,
      hasFailure ? "note-cell--failure" : "",
    ]
      .filter(Boolean)
      .join(" ");

    const title = cell.title
      ? escapeHtml(cell.title)
      : `${kindLabel(kind)} cell`;

    const count = executionCount > 0 ? `In [${executionCount}]` : "Not run";

    const runButton = executable
      ? '<button class="note-cell__run" type="button" data-action="run-cell">Run</button>'
      : "";

    return `
      <article
        class="${classes}"
        data-cell-index="${index}"
        data-cell-id="${escapeHtml(cell.id || "")}"
        data-dirty="false"
      >
        <div class="note-cell__bar">
          <div class="note-cell__info">
            <select class="note-cell__kind-select" data-action="change-kind" aria-label="Cell kind">
              <option value="markdown"${kind === "markdown" ? " selected" : ""}>Markdown</option>
              <option value="cpp"${kind === "cpp" ? " selected" : ""}>C++</option>
              <option value="reply"${kind === "reply" ? " selected" : ""}>Reply</option>
              <option value="html"${kind === "html" ? " selected" : ""}>HTML</option>
            </select>

            <span class="note-cell__kind">${escapeHtml(kindLabel(kind))}</span>
            <span class="note-cell__id">${escapeHtml(id)}</span>
            <span class="note-cell__meta">${title}</span>

            ${
              executable
                ? `<span class="note-cell__count">${escapeHtml(count)}</span>`
                : ""
            }
          </div>

          <div class="note-cell__actions">
            ${runButton}
            <button type="button" data-action="move-up">Up</button>
            <button type="button" data-action="move-down">Down</button>
            <button type="button" data-action="delete-cell">Delete</button>
          </div>
        </div>

        ${renderCellBody(cell)}
        ${renderOutputs(outputs)}
      </article>
    `;
  }

  function renderEmpty(message) {
    const container = $(selectors.cells);

    if (!container) {
      return;
    }

    container.innerHTML = `
      <article class="note-empty">
        <p>${escapeHtml(message)}</p>
      </article>
    `;
  }

  function renderDocument(payload) {
    const documentData = unwrapDocument(payload);

    if (!documentData) {
      renderEmpty("Unable to load the note document.");
      return;
    }

    state.document = documentData;

    const title = documentData.title || "Untitled note";
    const cellCount = Number(documentData.cellCount || 0);
    const cells = Array.isArray(documentData.cells) ? documentData.cells : [];

    setText(selectors.title, title);
    setText(selectors.document, title);
    setText(selectors.cellCount, String(cellCount));

    document.title = `${title} · Vix Note`;

    const container = $(selectors.cells);

    if (!container) {
      return;
    }

    if (cells.length === 0) {
      renderEmpty(
        "This note has no cells yet. Add a Markdown or C++ cell to start.",
      );
      return;
    }

    container.innerHTML = cells.map(renderCell).join("");
  }

  function findCellElement(target) {
    return target.closest(".note-cell");
  }

  function findCellById(id) {
    if (!state.document || !Array.isArray(state.document.cells)) {
      return null;
    }

    return (
      state.document.cells.find((cell) => String(cell.id) === String(id)) ||
      null
    );
  }

  function cellIndexById(id) {
    if (!state.document || !Array.isArray(state.document.cells)) {
      return -1;
    }

    return state.document.cells.findIndex(
      (cell) => String(cell.id) === String(id),
    );
  }

  function cellRouteKey(cellElement) {
    const id = cellElement?.dataset?.cellId;

    if (id) {
      return encodeURIComponent(id);
    }

    return encodeURIComponent(cellElement?.dataset?.cellIndex || "0");
  }

  function updateLocalCell(cellElement) {
    const id = cellElement.dataset.cellId;
    const cell = findCellById(id);

    if (!cell) {
      return null;
    }

    const textarea = $('[data-action="edit-source"]', cellElement);
    const select = $('[data-action="change-kind"]', cellElement);

    if (textarea) {
      cell.source = textarea.value;
    }

    if (select) {
      cell.kind = select.value;
    }

    return cell;
  }

  async function syncCell(cellElement) {
    if (!cellElement || cellElement.dataset.dirty !== "true") {
      return;
    }

    const cell = updateLocalCell(cellElement);

    if (!cell) {
      return;
    }

    const key = cellRouteKey(cellElement);

    const result = await api(`/api/cells/${key}`, {
      method: "PUT",
      body: JSON.stringify({
        kind: cell.kind,
        source: cell.source,
      }),
    });

    if (result.document) {
      state.document = result.document;
    }

    cellElement.dataset.dirty = "false";
    setDirty(hasDirtyCells());
  }

  async function syncDirtyCells() {
    const dirtyCells = $all('.note-cell[data-dirty="true"]');

    for (const cellElement of dirtyCells) {
      await syncCell(cellElement);
    }
  }

  function hasDirtyCells() {
    return $all('.note-cell[data-dirty="true"]').length > 0;
  }

  async function loadDocument() {
    setKernelStatus("Loading");
    setNoteState("Loading");
    setMessage("");

    try {
      const documentData = await api("/api/document");
      renderDocument(documentData);
      setKernelStatus("Idle");
      setDirty(false);
    } catch (error) {
      setKernelStatus("Error");
      setNoteState("Error");
      setMessage(error.message || "Failed to load note document.", "error");
      renderEmpty("Unable to load the note document.");
    }
  }

  async function addCell(kind) {
    setMessage("");
    setKernelStatus("Editing");
    setBusy(true);

    const source =
      normalizeKind(kind) === "cpp"
        ? '#include <iostream>\n\nint main()\n{\n  std::cout << "Hello from Vix Note\\n";\n  return 0;\n}\n'
        : "Write your explanation here.";

    try {
      await syncDirtyCells();

      const result = await api("/api/cells", {
        method: "POST",
        body: JSON.stringify({
          kind,
          source,
        }),
      });

      if (result.document) {
        renderDocument(result.document);
      } else {
        await loadDocument();
      }

      setDirty(true);
      setMessage("Cell added.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to add cell.", "error");
    } finally {
      setBusy(false);
      setKernelStatus("Idle");
    }
  }

  async function runCell(button) {
    const cellElement = findCellElement(button);

    if (!cellElement) {
      return;
    }

    const key = cellRouteKey(cellElement);

    cellElement.classList.add("note-cell--running");

    setMessage("");
    setKernelStatus("Running");
    setButtonBusy(button, true, "Running...");

    try {
      await syncCell(cellElement);

      const result = await api(
        "/api/run-all",
        {
          method: "POST",
        },
        {
          allowErrorResponse: true,
        },
      );
      if (result.document) {
        renderDocument(result.document);
      } else if (result.cell) {
        await loadDocument();
      }

      const status = normalizeKind(result?.result?.status);

      if (status === "failure") {
        setMessage(
          result?.result?.message || "Cell execution failed.",
          "error",
        );
      } else if (status === "skipped") {
        setMessage(result?.result?.message || "Cell was skipped.", "warning");
      } else {
        setMessage("Cell executed.", "success");
      }

      state.lastRunAt = new Date().toISOString();
    } catch (error) {
      setMessage(error.message || "Failed to run cell.", "error");
    } finally {
      setKernelStatus("Idle");
      setButtonBusy(button, false);

      cellElement.classList.remove("note-cell--running");
    }
  }

  async function runAll(button) {
    setMessage("");
    setKernelStatus("Running");
    setBusy(true);
    setButtonBusy(button, true, "Running...");

    try {
      await syncDirtyCells();

      const result = await api(
        "/api/run-all",
        {
          method: "POST",
        },
        {
          allowErrorResponse: true,
        },
      );

      if (result.document) {
        renderDocument(result.document);
      }

      if (result.ok) {
        setMessage(
          `Run completed. ${result.executed || 0} cell(s) executed.`,
          "success",
        );
      } else if (result.stopped) {
        setMessage("Run stopped after a failed cell.", "error");
      } else {
        setMessage("Run completed with failures.", "error");
      }

      state.lastRunAt = new Date().toISOString();
    } catch (error) {
      setMessage(error.message || "Failed to run all cells.", "error");
    } finally {
      setBusy(false);
      setButtonBusy(button, false);
      setKernelStatus("Idle");
    }
  }

  async function moveCell(button, direction) {
    const cellElement = findCellElement(button);

    if (!cellElement) {
      return;
    }

    const id = cellElement.dataset.cellId;
    const index = cellIndexById(id);

    if (index < 0) {
      return;
    }

    const targetIndex = direction === "up" ? index - 1 : index + 1;

    if (
      !state.document ||
      !Array.isArray(state.document.cells) ||
      targetIndex < 0 ||
      targetIndex >= state.document.cells.length
    ) {
      return;
    }

    setMessage("");
    setKernelStatus("Editing");
    setBusy(true);

    try {
      await syncCell(cellElement);

      const result = await api(`/api/cells/${cellRouteKey(cellElement)}/move`, {
        method: "POST",
        body: JSON.stringify({
          index: targetIndex,
        }),
      });

      if (result.document) {
        renderDocument(result.document);
      } else {
        await loadDocument();
      }

      setDirty(true);
      setMessage("Cell moved.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to move cell.", "error");
    } finally {
      setBusy(false);
      setKernelStatus("Idle");
    }
  }

  async function deleteCell(button) {
    const cellElement = findCellElement(button);

    if (!cellElement) {
      return;
    }

    setMessage("");
    setKernelStatus("Editing");
    setBusy(true);

    try {
      const result = await api(`/api/cells/${cellRouteKey(cellElement)}`, {
        method: "DELETE",
      });

      if (result.document) {
        renderDocument(result.document);
      } else {
        await loadDocument();
      }

      setDirty(true);
      setMessage("Cell deleted.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to delete cell.", "error");
    } finally {
      setBusy(false);
      setKernelStatus("Idle");
    }
  }

  async function saveNote(button) {
    setMessage("");
    setKernelStatus("Saving");
    setBusy(true);
    setButtonBusy(button, true, "Saving...");

    try {
      await syncDirtyCells();

      await api("/api/document/save", {
        method: "POST",
      });

      state.lastSaveAt = new Date().toISOString();
      setDirty(false);
      setMessage("Note saved.", "success");
    } catch (error) {
      setMessage(error.message || "Failed to save note.", "error");
      setNoteState("Unsaved");
    } finally {
      setBusy(false);
      setButtonBusy(button, false);
      setKernelStatus("Idle");
    }
  }

  function markCellDirty(element) {
    const cellElement = findCellElement(element);

    if (!cellElement) {
      return;
    }

    updateLocalCell(cellElement);
    cellElement.dataset.dirty = "true";
    setDirty(true);
  }

  function bindActions() {
    document.addEventListener("click", (event) => {
      const target = event.target;

      if (!(target instanceof HTMLElement)) {
        return;
      }

      const action = target.getAttribute("data-action");

      if (action === "run-cell") {
        runCell(target);
        return;
      }

      if (action === "run-all") {
        runAll(target);
        return;
      }

      if (action === "save") {
        saveNote(target);
        return;
      }

      if (action === "add-markdown") {
        addCell("markdown");
        return;
      }

      if (action === "add-cpp") {
        addCell("cpp");
        return;
      }

      if (action === "move-up") {
        moveCell(target, "up");
        return;
      }

      if (action === "move-down") {
        moveCell(target, "down");
        return;
      }

      if (action === "delete-cell") {
        deleteCell(target);
      }
    });

    document.addEventListener("input", (event) => {
      const target = event.target;

      if (!(target instanceof HTMLElement)) {
        return;
      }

      if (target.getAttribute("data-action") === "edit-source") {
        markCellDirty(target);
      }
    });

    document.addEventListener("change", (event) => {
      const target = event.target;

      if (!(target instanceof HTMLElement)) {
        return;
      }

      if (target.getAttribute("data-action") === "change-kind") {
        markCellDirty(target);
      }
    });

    document.addEventListener("focusout", async (event) => {
      const target = event.target;

      if (!(target instanceof HTMLElement)) {
        return;
      }

      const action = target.getAttribute("data-action");

      if (action !== "edit-source" && action !== "change-kind") {
        return;
      }

      const cellElement = findCellElement(target);

      if (!cellElement || cellElement.dataset.dirty !== "true") {
        return;
      }

      try {
        await syncCell(cellElement);
      } catch (error) {
        setMessage(error.message || "Failed to update cell.", "error");
      }
    });

    document.addEventListener("keydown", async (event) => {
      const target = event.target;

      if (!(target instanceof HTMLTextAreaElement)) {
        return;
      }

      const cellElement = findCellElement(target);

      if (!cellElement) {
        return;
      }

      if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
        event.preventDefault();

        const runButton = $('[data-action="run-cell"]', cellElement);

        if (runButton) {
          await runCell(runButton);
        }

        return;
      }

      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();

        const saveButton = $(selectors.save);

        if (saveButton) {
          await saveNote(saveButton);
        }
      }
    });
  }

  function init() {
    bindActions();
    loadDocument();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
