(() => {
  "use strict";

  const state = {
    documentTitle: "Untitled note",
    cellCount: 2,
    kernelStatus: "Idle",
    lastRunAt: null,
    lastSaveAt: null,
  };

  const selectors = {
    workspace: "#cells",
    output: ".note-output",
    statusCards: ".note-status__card",
    runCell: '[data-action="run-cell"]',
    runAll: '[data-action="run-all"]',
    save: '[data-action="save"]',
  };

  function $(selector, root = document) {
    return root.querySelector(selector);
  }

  function $all(selector, root = document) {
    return Array.from(root.querySelectorAll(selector));
  }

  function setKernelStatus(value) {
    state.kernelStatus = value;

    const cards = $all(selectors.statusCards);

    for (const card of cards) {
      const label = $(".note-status__label", card);
      const statusValue = $(".note-status__value", card);

      if (!label || !statusValue) {
        continue;
      }

      if (label.textContent.trim().toLowerCase() === "kernel") {
        statusValue.textContent = value;
      }
    }
  }

  function setCellCount(value) {
    state.cellCount = value;

    const cards = $all(selectors.statusCards);

    for (const card of cards) {
      const label = $(".note-status__label", card);
      const statusValue = $(".note-status__value", card);

      if (!label || !statusValue) {
        continue;
      }

      if (label.textContent.trim().toLowerCase() === "cells") {
        statusValue.textContent = String(value);
      }
    }
  }

  function setDocumentTitle(value) {
    state.documentTitle = value || "Untitled note";

    const cards = $all(selectors.statusCards);

    for (const card of cards) {
      const label = $(".note-status__label", card);
      const statusValue = $(".note-status__value", card);

      if (!label || !statusValue) {
        continue;
      }

      if (label.textContent.trim().toLowerCase() === "document") {
        statusValue.textContent = state.documentTitle;
      }
    }
  }

  function showOutput(cell, message, kind = "info") {
    if (!cell) {
      return;
    }

    const output = $(selectors.output, cell);

    if (!output) {
      return;
    }

    output.hidden = false;
    output.dataset.kind = kind;
    output.textContent = message;
  }

  function clearOutput(cell) {
    if (!cell) {
      return;
    }

    const output = $(selectors.output, cell);

    if (!output) {
      return;
    }

    output.hidden = true;
    output.textContent = "";
    delete output.dataset.kind;
  }

  function allCells() {
    return $all(".note-cell");
  }

  function executableCells() {
    return allCells().filter((cell) => {
      return (
        cell.classList.contains("note-cell--cpp") ||
        cell.classList.contains("note-cell--reply")
      );
    });
  }

  function markButtonBusy(button, busy, label) {
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

  function simulateCellRun(cell) {
    setKernelStatus("Running");

    clearOutput(cell);

    window.setTimeout(() => {
      showOutput(
        cell,
        "Execution will be connected to the Vix Note kernel API.",
        "info",
      );

      state.lastRunAt = new Date().toISOString();
      setKernelStatus("Idle");
    }, 220);
  }

  function runSingleCell(button) {
    const cell = button.closest(".note-cell");

    if (!cell) {
      return;
    }

    markButtonBusy(button, true, "Running...");

    setKernelStatus("Running");

    window.setTimeout(() => {
      simulateCellRun(cell);
      markButtonBusy(button, false);
    }, 120);
  }

  function runAll(button) {
    const cells = executableCells();

    markButtonBusy(button, true, "Running...");

    if (cells.length === 0) {
      setKernelStatus("Idle");
      markButtonBusy(button, false);
      return;
    }

    setKernelStatus("Running");

    let index = 0;

    const runNext = () => {
      const cell = cells[index];

      if (!cell) {
        state.lastRunAt = new Date().toISOString();
        setKernelStatus("Idle");
        markButtonBusy(button, false);
        return;
      }

      showOutput(
        cell,
        "Execution will be connected to the Vix Note kernel API.",
        "info",
      );

      index += 1;
      window.setTimeout(runNext, 160);
    };

    runNext();
  }

  function saveNote(button) {
    markButtonBusy(button, true, "Saving...");

    window.setTimeout(() => {
      state.lastSaveAt = new Date().toISOString();
      setKernelStatus("Saved");
      markButtonBusy(button, false);

      window.setTimeout(() => {
        setKernelStatus("Idle");
      }, 800);
    }, 220);
  }

  function bindActions() {
    document.addEventListener("click", (event) => {
      const target = event.target;

      if (!(target instanceof HTMLElement)) {
        return;
      }

      const action = target.getAttribute("data-action");

      if (action === "run-cell") {
        runSingleCell(target);
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

  function hydrateInitialState() {
    setDocumentTitle(state.documentTitle);
    setCellCount(allCells().length);
    setKernelStatus(state.kernelStatus);
  }

  function init() {
    hydrateInitialState();
    bindActions();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
