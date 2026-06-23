(() => {
  "use strict";

  const state = {
    document: null,
    kernelStatus: "Idle",
    running: false,
    lastRunAt: null,
    lastSaveAt: null,
  };

  const selectors = {
    cells: "[data-note-cells]",
    title: "[data-note-title]",
    document: "[data-note-document]",
    cellCount: "[data-note-cell-count]",
    kernel: "[data-note-kernel]",
    message: "[data-note-message]",
    runAll: '[data-action="run-all"]',
    save: '[data-action="save"]',
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
    element.className = `note-message note-message--${kind}`;
  }

  function setBusy(busy) {
    state.running = busy;

    for (const button of $all("button")) {
      button.disabled = busy;
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

  async function api(path, options = {}) {
    const response = await fetch(path, {
      headers: {
        Accept: "application/json",
        ...(options.headers || {}),
      },
      ...options,
    });

    const contentType = response.headers.get("content-type") || "";
    const body = contentType.includes("application/json")
      ? await response.json()
      : { ok: false, error: await response.text() };

    if (!response.ok) {
      const message =
        body.error ||
        body.message ||
        body?.result?.message ||
        `Request failed with status ${response.status}`;

      throw new Error(message);
    }

    return body;
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
        const content = String(output.content ?? "");

        if (kind === "html") {
          return `<div class="note-output note-output--html">${content}</div>`;
        }

        return `<pre class="note-output note-output--${escapeHtml(kind)}">${escapeHtml(
          content,
        )}</pre>`;
      })
      .join("");

    return `<div class="note-outputs" id="outputs">${rendered}</div>`;
  }

  function renderCellBody(cell) {
    const kind = normalizeKind(cell.kind);
    const source = String(cell.source ?? "");

    if (kind === "markdown") {
      return `<div class="note-cell__body">${renderMarkdown(source)}</div>`;
    }

    if (kind === "html") {
      return `<div class="note-html-preview">${source}</div>`;
    }

    return `<pre class="note-code"><code>${escapeHtml(source)}</code></pre>`;
  }

  function renderCell(cell) {
    const kind = normalizeKind(cell.kind);
    const index = Number(cell.index ?? 0);
    const executable = Boolean(cell.executable);
    const executionCount = Number(cell.executionCount || 0);
    const outputs = Array.isArray(cell.outputs) ? cell.outputs : [];
    const hasFailure = outputs.some((output) => {
      const outputKind = normalizeKind(output.kind);
      return outputKind === "error" || outputKind === "stderr";
    });

    const classes = [
      "note-cell",
      `note-cell--${kind}`,
      hasFailure ? "note-cell--failure" : "",
    ]
      .filter(Boolean)
      .join(" ");

    const title = cell.title
      ? escapeHtml(cell.title)
      : `${kindLabel(kind)} cell`;
    const count = executionCount > 0 ? `In [${executionCount}]` : "Not run";

    const runButton = executable
      ? `<button class="note-cell__run" type="button" data-action="run-cell" data-cell-index="${index}">Run</button>`
      : "";

    return `
      <article class="${classes}" data-cell-index="${index}" data-cell-id="${escapeHtml(
        cell.id || "",
      )}">
        <div class="note-cell__bar">
          <div class="note-cell__info">
            <span class="note-cell__kind">${escapeHtml(kindLabel(kind))}</span>
            <span class="note-cell__meta">${title}</span>
            ${
              executable
                ? `<span class="note-cell__count">${escapeHtml(count)}</span>`
                : ""
            }
          </div>

          ${runButton}
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

  function renderDocument(documentData) {
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
      renderEmpty("This note has no cells yet.");
      return;
    }

    container.innerHTML = cells.map(renderCell).join("");
  }

  function updateCell(cell) {
    if (!cell) {
      return;
    }

    const current = $(`.note-cell[data-cell-index="${Number(cell.index)}"]`);

    if (!current) {
      renderDocument(state.document);
      return;
    }

    current.outerHTML = renderCell(cell);

    if (state.document && Array.isArray(state.document.cells)) {
      state.document.cells[cell.index] = cell;
    }
  }

  async function loadDocument() {
    setKernelStatus("Loading");
    setMessage("");

    try {
      const documentData = await api("/api/document");
      renderDocument(documentData);
      setKernelStatus("Idle");
    } catch (error) {
      setKernelStatus("Error");
      setMessage(error.message || "Failed to load note document.", "error");
      renderEmpty("Unable to load the note document.");
    }
  }

  async function runCell(button) {
    const index = Number(button.dataset.cellIndex);

    if (!Number.isFinite(index)) {
      return;
    }

    const cellElement = button.closest(".note-cell");

    if (cellElement) {
      cellElement.classList.add("note-cell--running");
    }

    setMessage("");
    setKernelStatus("Running");
    setButtonBusy(button, true, "Running...");

    try {
      const result = await api(`/api/cells/${index}/run`, {
        method: "POST",
      });

      updateCell(result.cell);

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

      if (cellElement) {
        cellElement.classList.remove("note-cell--running");
      }
    }
  }

  async function runAll(button) {
    setMessage("");
    setKernelStatus("Running");
    setBusy(true);
    setButtonBusy(button, true, "Running...");

    try {
      const result = await api("/api/run-all", {
        method: "POST",
      });

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

  function saveNote(button) {
    state.lastSaveAt = new Date().toISOString();

    setButtonBusy(button, true, "Saving...");
    setKernelStatus("Save unavailable");

    window.setTimeout(() => {
      setButtonBusy(button, false);
      setKernelStatus("Idle");
      setMessage("Save will be available in Vix Note v0.3.0.", "warning");
    }, 180);
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
