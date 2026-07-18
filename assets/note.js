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
 *   POST   /api/path/move             { path, directory }
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
    extensions: { ok: true, extensions: [], cellTypes: [] },
    extensionWorkbench: {
      query: "",
      loading: false,
      error: "",
      selectedId: null,
      pendingActions: new Map(),

      marketplace: [],
      recommended: [],
      installed: [],
      builtins: [],
      updates: [],
      registry: {
        source: "none",
        syncedAt: null,
        stale: false,
        syncing: false,
        error: "",
      },
      searchTimer: 0,
      searchSerial: 0,

      sections: {
        search: true,
        updates: true,
        installed: true,
        recommended: false,
        builtin: false,
      },
    },
    selectedId: null,
    editing: false,
    kernel: "idle",
    busy: false,
    sidebarCollapsed: false,
    sidebarWidth: 260,
    bottomPanelHeight: 220,
    bottomPanelVisible: false,
    focusMode: false,
    activePanel: "explorer", // explorer | problems | extensions
    focus: {
      area: "editor",
      previousElement: null,
      activeCellId: null,
    },
    pending: {
      saving: false,
      runningCells: new Set(),
      loadingDocument: false,
      explorerPaths: new Set(),
    },
    notifications: [],
    cellClipboard: { mode: "copy", cell: null },
    autoSave: { mode: "off", delay: 1000, timer: null },
    explorer: {
      rootPath: ".",
      currentPath: ".",
      selectedDirPath: ".",
      loadingPath: null,
      entries: new Map(),
      loadedDirs: new Set(),
      expandedDirs: new Set(["."]),
      draft: null,
    },
    tabs: [], // [{ kind, path, extensionId, title, dirty, preview, icon }]
    activeTabPath: null,
    activeEditorTabId: null,
    closedTabs: [],

    diagnostics: {
      status: "idle",
      items: [],
      byCell: new Map(),
      filter: "",
      severity: "all",
    },
    commandPalette: { open: false, query: "", selected: 0, mode: "commands" },
    quickOpen: { open: false, query: "", selected: 0 },
    find: {
      open: false,
      query: "",
      caseSensitive: false,
      matches: [],
      index: 0,
    },

    // Drag and drop. Explorer and tab drags never mix.
    drag: {
      explorer: null, // { path, type }
      tab: null, // { path, fromIndex }
    },
  };

  const DEFAULT_SIDEBAR_WIDTH = 260;
  const MIN_SIDEBAR_WIDTH = 190;
  const MAX_SIDEBAR_WIDTH = 520;
  const TABS_STORAGE_KEY = "vix-note:tabs:v1";
  const UI_STATE_KEY = "vix-note:ui-state:v2";
  const SESSION_STATE_KEY = "vix-note:session:v2";
  const MAX_CLOSED_TABS = 20;
  const THEME_STORAGE_KEY = "vix.note.theme";
  const LEGACY_THEME_STORAGE_KEY = "vix-note:theme:v1";
  const THEME_TOKEN_TO_CSS = {
    "editor.background": "--vn-editor-bg",
    "editor.foreground": "--vn-text1",
    "editor.activeBackground": "--vn-editor-active-bg",
    "sidebar.background": "--vn-sidebar-bg",
    "activityBar.background": "--vn-activity-bg",
    "tabs.background": "--vn-tabsbar-bg",
    "appbar.background": "--vn-appbar-bg",
    "toolbar.background": "--vn-toolbar-bg",
    "surface.background": "--vn-color1",
    "surface.subtle": "--vn-color2",
    "surface.hover": "--vn-color3",
    "border.color": "--vn-border1",
    "border.subtle": "--vn-border2",
    "border.muted": "--vn-border3",
    "text.primary": "--vn-text1",
    "text.secondary": "--vn-text2",
    "text.muted": "--vn-text3",
    "brand.primary": "--vn-brand1",
    "brand.strong": "--vn-brand0",
    "brand.soft": "--vn-brand2",
    "accent.color": "--vn-accent1",
    "status.error": "--vn-error",
    "status.success": "--vn-problem-ok",
    "status.warning": "--vn-warning",
  };

  const VIX_LOGO_SVG = `<svg viewBox="0 0 120 120" fill="none" xmlns="http://www.w3.org/2000/svg">
  <circle cx="60" cy="60" r="58" fill="#07110c"/>
  <circle cx="60" cy="60" r="57" stroke="#22c55e" stroke-opacity="0.22" stroke-width="2"/>
  <defs>
    <linearGradient id="left" x1="28" y1="24" x2="58" y2="96" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#d4fcd4"/>
      <stop offset="55%" stop-color="#4ade80"/>
      <stop offset="100%" stop-color="#22c55e"/>
    </linearGradient>
    <linearGradient id="right" x1="92" y1="24" x2="62" y2="96" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#22c55e"/>
      <stop offset="100%" stop-color="#15803d"/>
    </linearGradient>
  </defs>
  <polygon points="28,24 45,24 60,96 50,96" fill="url(#left)"/>
  <polygon points="92,24 75,24 60,96 70,96" fill="url(#right)"/>
  <line x1="38" y1="50" x2="51" y2="96" stroke="#bbf7d0" stroke-width="3" stroke-linecap="round" opacity="0.65"/>
</svg>`;

  function svgToDataUri(svg) {
    const text = String(svg || "").replace(/[\u0000-\u001f\u007f]/g, "");
    if (!text.trim().startsWith("<svg") || /javascript:/i.test(text)) return "";
    return `data:image/svg+xml;charset=UTF-8,${encodeURIComponent(text)}`;
  }

  const VIX_LOGO_ICON = svgToDataUri(VIX_LOGO_SVG);

  const BUILTIN_THEMES = [
    { id: "system", label: "System", kind: "system", system: true, tokens: {} },
    { id: "light", label: "Light", kind: "light", tokens: {} },
    {
      id: "dark",
      label: "Dark",
      kind: "dark",
      tokens: {
        "surface.background": "#1f2329",
        "surface.subtle": "#252a32",
        "surface.hover": "#303642",
        "editor.background": "#1f2329",
        "editor.foreground": "#e8e3d8",
        "editor.activeBackground": "#252a32",
        "sidebar.background": "#1a1d22",
        "activityBar.background": "#15181d",
        "tabs.background": "#20242b",
        "appbar.background": "#171b20",
        "toolbar.background": "#20242b",
        "border.color": "#343a44",
        "border.subtle": "#29303a",
        "border.muted": "#242a32",
        "text.primary": "#e8e3d8",
        "text.secondary": "#b8b2a8",
        "text.muted": "#7e8794",
        "brand.primary": "#f37726",
        "brand.strong": "#f9aa7c",
        "brand.soft": "#7c3f1d",
        "accent.color": "#fb923c",
      },
    },
    {
      id: "softadastra",
      label: "Softadastra",
      kind: "dark",
      tokens: {
        "surface.background": "#1a1e22",
        "surface.subtle": "#15191d",
        "surface.hover": "#20252a",
        "editor.background": "#0f1215",
        "editor.foreground": "#f1f5f9",
        "editor.activeBackground": "#15191d",
        "sidebar.background": "#1a1e22",
        "activityBar.background": "#131619",
        "tabs.background": "#15191d",
        "appbar.background": "#131619",
        "toolbar.background": "#1a1e22",
        "border.color": "#343a40",
        "border.subtle": "#282e34",
        "border.muted": "#22282e",
        "text.primary": "#f1f5f9",
        "text.secondary": "#b6c0cb",
        "text.muted": "#778390",
        "brand.primary": "#f97316",
        "brand.strong": "#fb923c",
        "brand.soft": "#fdba74",
        "accent.color": "#f97316",
        "status.error": "#f87171",
        "status.success": "#22c55e",
        "status.warning": "#fbbf24",
      },
    },
  ];

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
    problemsFilter: "[data-problems-filter]",
    problemsSeverity: "[data-problems-severity]",
    problemsBadge: "[data-activity-problems-badge]",
    extensionsList: "[data-extensions-list]",
    extensionsCount: "[data-extensions-count]",
    extensionsSearch: "[data-extensions-search]",
    statusProblems: "[data-status-problems]",
    statusProblemsCount: "[data-status-problems-count]",
  };

  const $ = (s, root = document) => root.querySelector(s);
  const $all = (s, root = document) => Array.from(root.querySelectorAll(s));

  function isTypingTarget(target) {
    if (!(target instanceof Element)) return false;
    if (
      target instanceof HTMLTextAreaElement ||
      target instanceof HTMLInputElement ||
      target instanceof HTMLSelectElement
    )
      return true;
    return !!target.closest(
      '[contenteditable="true"], [data-modal], .vn-CommandPalette, .vn-QuickOpen, .vn-FindBox',
    );
  }

  function rememberFocus(area = state.focus.area) {
    const active = document.activeElement;
    if (active instanceof HTMLElement && active !== document.body) {
      state.focus.previousElement = active;
    }
    state.focus.area = area;
  }

  function restoreFocus() {
    const el = state.focus.previousElement;
    if (el instanceof HTMLElement && document.contains(el) && !el.disabled) {
      el.focus({ preventScroll: true });
      return;
    }
    if (state.selectedId) {
      const cell = cellElById(state.selectedId);
      if (cell) cell.focus({ preventScroll: true });
    }
  }

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
    const value = String(kind || "")
      .trim()
      .toLowerCase();
    return value || "unknown";
  }

  function cellTypeOf(cell) {
    return normalizeKind(cell && (cell.type || cell.kind));
  }

  function setCellType(cell, type) {
    if (!cell) return;
    const normalized = normalizeKind(type);
    cell.type = normalized;
    cell.kind = normalized;
  }

  function normalizedCellTypes() {
    const builtins = [
      {
        id: "markdown",
        label: "Markdown",
        language: "markdown",
        executable: false,
        builtin: true,
      },
      {
        id: "cpp",
        label: "C++",
        language: "cpp",
        executable: true,
        builtin: true,
      },
      {
        id: "reply",
        label: "Reply",
        language: "reply",
        executable: true,
        builtin: true,
      },
      {
        id: "html",
        label: "HTML",
        language: "html",
        executable: false,
        builtin: true,
      },
    ];
    const byId = new Map(builtins.map((type) => [type.id, type]));
    const list = Array.isArray(state.extensions.cellTypes)
      ? state.extensions.cellTypes
      : [];
    for (const raw of list) {
      const id = normalizeKind(raw && raw.id);
      if (!id) continue;
      byId.set(id, {
        id,
        label: raw.label || id,
        language: raw.language || id,
        executable: !!raw.executable,
        builtin: !!raw.builtin,
        extension: raw.extension || raw.packageId || "",
        commentLine: raw.commentLine || "",
        commentBlock: raw.commentBlock || "",
        defaultSource: raw.defaultSource || "",
        placeholder: raw.placeholder || "",
      });
    }
    return Array.from(byId.values());
  }

  function cellTypeDescriptor(kind) {
    const k = normalizeKind(kind);
    return normalizedCellTypes().find((c) => normalizeKind(c.id) === k) || null;
  }

  function extensionDescriptorForCellType(kind) {
    const type = cellTypeDescriptor(kind);
    if (!type || !type.extension) return null;
    const list = Array.isArray(state.extensions.extensions)
      ? state.extensions.extensions
      : [];
    return list.find((ext) => ext && ext.id === type.extension) || null;
  }

  function executionAvailabilityForCell(cell) {
    const typeId = cellTypeOf(cell);
    const descriptor = cellTypeDescriptor(typeId);
    if (!descriptor) {
      return {
        executable: false,
        available: false,
        reason: `Cell type '${typeId}' is not available`,
        extension: "",
      };
    }
    if (!descriptor.executable) {
      return {
        executable: false,
        available: false,
        reason: "Cell is not executable",
        extension: descriptor.extension || "",
      };
    }
    const extension = extensionDescriptorForCellType(typeId);
    if (extension) {
      if (extension.enabled === false) {
        return {
          executable: true,
          available: false,
          reason: `${descriptor.label || typeId} extension is disabled`,
          extension: extension.id,
        };
      }
      if (extension.available === false) {
        const reason =
          extension.runtime?.error ||
          extension.diagnostics?.[0] ||
          `${descriptor.label || typeId} runtime is unavailable`;
        return {
          executable: true,
          available: false,
          reason,
          extension: extension.id,
        };
      }
      if (extension.runtime && extension.runtime.healthy === false) {
        return {
          executable: true,
          available: false,
          reason:
            extension.runtime.error ||
            `${descriptor.label || typeId} runtime is unhealthy`,
          extension: extension.id,
        };
      }
    }
    return {
      executable: true,
      available: true,
      reason: "",
      extension: descriptor.extension || "",
    };
  }

  function cellTypeDisplayLabel(kind) {
    const desc = cellTypeDescriptor(kind);
    return desc && desc.label ? desc.label : normalizeKind(kind);
  }

  function isSafeThemeColor(value) {
    return typeof value === "string" && /^#[0-9a-fA-F]{6}$/.test(value.trim());
  }

  function normalizeTheme(value) {
    const raw = String(value || "system")
      .trim()
      .toLowerCase();
    if (["system", "light", "dark", "softadastra"].includes(raw)) return raw;
    if (raw === "vix.light") return "light";
    if (raw === "vix.dark") return "dark";
    const external = Array.isArray(state.extensions.themes)
      ? state.extensions.themes
      : [];
    if (
      external.some((theme) => String(theme?.id || "").toLowerCase() === raw)
    ) {
      return raw;
    }
    return "system";
  }

  function systemTheme() {
    return window.matchMedia &&
      window.matchMedia("(prefers-color-scheme: dark)").matches
      ? "dark"
      : "light";
  }

  function normalizedThemes() {
    const external = Array.isArray(state.extensions.themes)
      ? state.extensions.themes
      : [];
    return [...BUILTIN_THEMES, ...external].filter((theme) => {
      if (!theme || !theme.id || !theme.label) return false;
      const tokens = theme.tokens || {};
      return Object.entries(tokens).every(
        ([key, value]) => THEME_TOKEN_TO_CSS[key] && isSafeThemeColor(value),
      );
    });
  }

  function themeById(themeId) {
    const normalized = normalizeTheme(themeId);
    return (
      normalizedThemes().find((item) => item.id === normalized) ||
      BUILTIN_THEMES[0]
    );
  }

  function clearThemeVariables(root) {
    for (const cssVar of Object.values(THEME_TOKEN_TO_CSS)) {
      root.style.removeProperty(cssVar);
    }
  }

  function applyTheme(themeId, options = {}) {
    const selected = normalizeTheme(themeId);
    const resolved = selected === "system" ? systemTheme() : selected;
    const theme = themeById(resolved);
    const root = document.documentElement;

    root.dataset.themeChoice = selected;
    root.dataset.theme = resolved;
    root.dataset.vnTheme = resolved;
    root.style.colorScheme =
      resolved === "dark" || resolved === "softadastra" ? "dark" : "light";

    clearThemeVariables(root);
    for (const [token, value] of Object.entries(theme.tokens || {})) {
      const cssVar = THEME_TOKEN_TO_CSS[token];
      if (cssVar && isSafeThemeColor(value)) {
        root.style.setProperty(cssVar, value);
      }
    }

    if (options.persist !== false) {
      localStorage.setItem(THEME_STORAGE_KEY, selected);
      localStorage.removeItem(LEGACY_THEME_STORAGE_KEY);
    }

    renderThemeMenu();
  }

  function setTheme(themeId) {
    applyTheme(themeId);
    showNotification({
      type: "success",
      message: `Theme: ${themeById(themeId).label}`,
    });
  }

  let systemThemeMedia = null;

  function restoreTheme() {
    const stored =
      localStorage.getItem(THEME_STORAGE_KEY) ||
      localStorage.getItem(LEGACY_THEME_STORAGE_KEY) ||
      "system";
    applyTheme(stored, { persist: false });

    if (window.matchMedia) {
      systemThemeMedia = window.matchMedia("(prefers-color-scheme: dark)");
      systemThemeMedia.addEventListener?.("change", () => {
        if (
          normalizeTheme(localStorage.getItem(THEME_STORAGE_KEY)) === "system"
        ) {
          applyTheme("system", { persist: false });
        }
      });
    }
  }

  function renderThemeMenu() {
    const menu = $("[data-theme-menu]");
    if (!menu) return;

    const active = normalizeTheme(
      document.documentElement.dataset.themeChoice ||
        localStorage.getItem(THEME_STORAGE_KEY) ||
        "system",
    );

    menu.innerHTML = normalizedThemes()
      .map((theme) => {
        const selected = normalizeTheme(theme.id) === active;
        const source =
          theme.system || BUILTIN_THEMES.some((item) => item.id === theme.id)
            ? "Built-in"
            : "Extension";
        return `
        <button
          type="button"
          class="vn-ThemeMenu__item${selected ? " is-active" : ""}"
          data-theme-option="${escapeHtml(theme.id)}"
          role="menuitemradio"
          aria-checked="${selected ? "true" : "false"}"
        >
          <span class="vn-ThemeMenu__check" aria-hidden="true">${selected ? "✓" : ""}</span>
          <span class="vn-ThemeMenu__label">
            <span>${escapeHtml(theme.label)}</span>
            <small>${escapeHtml(source)}</small>
          </span>
        </button>
      `;
      })
      .join("");
  }

  function closeThemeMenu() {
    const menu = $("[data-theme-menu]");
    const button = $("[data-theme-toggle]");
    if (menu) menu.hidden = true;
    if (button) button.setAttribute("aria-expanded", "false");
  }

  function toggleThemeMenu() {
    const menu = $("[data-theme-menu]");
    const button = $("[data-theme-toggle]");
    if (!menu || !button) return;
    renderThemeMenu();
    const open = menu.hidden;
    menu.hidden = !open;
    button.setAttribute("aria-expanded", open ? "true" : "false");
    if (open) {
      const active =
        $(".vn-ThemeMenu__item.is-active", menu) ||
        $(".vn-ThemeMenu__item", menu);
      active?.focus({ preventScroll: true });
    }
  }

  function openColorThemePicker() {
    toggleThemeMenu();
  }

  function extensionPendingAction(extensionId) {
    const id = String(extensionId || "");
    if (!id) return null;
    const pending = state.extensionWorkbench.pendingActions;
    if (pending instanceof Map) return pending.get(id) || null;
    if (pending && typeof pending.has === "function") {
      for (const key of pending) {
        if (String(key).startsWith(`${id}:`)) {
          return { action: String(key).slice(id.length + 1), stage: "working", startedAt: Date.now() };
        }
      }
    }
    return null;
  }

  function isExtensionActionPending(extensionId) {
    return !!extensionPendingAction(extensionId);
  }

  function beginExtensionAction(extensionId, action, stage = "preparing") {
    const id = String(extensionId || "");
    if (!id) return false;
    if (!(state.extensionWorkbench.pendingActions instanceof Map)) {
      state.extensionWorkbench.pendingActions = new Map();
    }
    if (state.extensionWorkbench.pendingActions.has(id)) return false;
    state.extensionWorkbench.pendingActions.set(id, {
      action: String(action || "working"),
      stage,
      startedAt: Date.now(),
    });
    return true;
  }

  function updateExtensionActionStage(extensionId, stage) {
    const current = extensionPendingAction(extensionId);
    if (!current) return;
    state.extensionWorkbench.pendingActions.set(String(extensionId), {
      ...current,
      stage,
    });
  }

  function endExtensionAction(extensionId) {
    if (state.extensionWorkbench.pendingActions instanceof Map) {
      state.extensionWorkbench.pendingActions.delete(String(extensionId || ""));
    }
  }

  function extensionActionLabel(action) {
    const labels = {
      install: "Installing…",
      update: "Updating…",
      uninstall: "Uninstalling…",
      enable: "Enabling…",
      disable: "Disabling…",
      reload: "Refreshing…",
      working: "Working…",
    };
    return labels[action] || "Working…";
  }

  function extensionActionStatus(action) {
    const labels = {
      install: "Installing",
      update: "Updating",
      uninstall: "Uninstalling",
      enable: "Enabling",
      disable: "Disabling",
      reload: "Refreshing",
      working: "Working",
    };
    return labels[action] || "Working";
  }

  function installedExtensionIds() {
    return new Set(
      uniqueExtensions(state.extensionWorkbench.installed)
        .map((ext) => extensionIdentifier(ext))
        .filter(Boolean),
    );
  }

  function filterCatalogExtensions(items) {
    const installed = installedExtensionIds();
    return uniqueExtensions(items).filter((ext) => {
      const id = extensionIdentifier(ext);
      return ext && id && !ext.builtin && ext.installed !== true && !installed.has(id);
    });
  }

  function extensionStatus(ext) {
    if (!ext) return "Unknown";
    const id = extensionIdentifier(ext);
    const pending = extensionPendingAction(id);
    if (pending) return extensionActionStatus(pending.action);
    if (ext.builtin) return "Built-in";
    const installed = ext.installed === true;
    if (installed && ext.enabled === false) return "Disabled";
    if (
      installed &&
      (ext.available === false ||
        (ext.runtime && ext.runtime.healthy === false))
    ) {
      return "Broken";
    }
    if (installed) return "Installed";
    return "Available";
  }

  function extensionIdentifier(ext) {
    if (!ext) return "";

    const fallback = `${ext.namespace || ""}/${ext.name || ""}`.replace(
      /^\/+|\/+$/g,
      "",
    );

    const explicit = String(ext.id || "").trim();
    if (explicit.includes("/")) return explicit;

    return String(fallback || explicit || "extension");
  }

  function uniqueExtensions(items) {
    const byId = new Map();

    for (const ext of Array.isArray(items) ? items : []) {
      if (!ext) continue;

      const id = extensionIdentifier(ext);
      if (!id) continue;

      byId.set(id, ext);
    }

    return Array.from(byId.values());
  }

  function findExtensionById(id) {
    const wanted = String(id || "");
    if (!wanted) return null;

    const groups = [
      state.extensionWorkbench.installed,
      state.extensionWorkbench.builtins,
      state.extensionWorkbench.marketplace,
      state.extensionWorkbench.recommended,
      Array.isArray(state.extensions.extensions) ? state.extensions.extensions : [],
    ];

    for (const group of groups) {
      const found = uniqueExtensions(group).find(
        (ext) => extensionIdentifier(ext) === wanted,
      );
      if (found) return found;
    }
    return null;
  }

  function applyExtensionsPayload(payload) {
    if (!payload || payload.ok === false) {
      throw new Error((payload && payload.error) || "Extension action failed");
    }

    state.extensions = {
      ok: true,
      extensions: Array.isArray(payload.extensions) ? payload.extensions : [],
      cellTypes: Array.isArray(payload.cellTypes) ? payload.cellTypes : [],
      themes: Array.isArray(payload.themes) ? payload.themes : [],
      registry: payload.registry || state.extensions.registry || null,
    };

    if (payload.registry) {
      state.extensionWorkbench.registry = {
        ...state.extensionWorkbench.registry,
        ...payload.registry,
      };
    }

    state.extensionWorkbench.error = "";
    refreshExtensionWorkbenchFromRegistry();
    renderToolbarKindOptions();
    renderExtensionsPanel();
  }

  async function reloadLocalExtensions() {
    const payload = await api("/api/extensions");
    applyExtensionsPayload(payload);
    return payload;
  }

  function applyCatalogPayload(payload, target = "recommended") {
    if (!payload || payload.ok === false) {
      const message = (payload && (payload.error || payload.syncError)) || "Registry catalog unavailable";
      state.extensionWorkbench.registry = {
        ...state.extensionWorkbench.registry,
        ...(payload && payload.registry ? payload.registry : {}),
        error: message,
      };
      if (target === "marketplace") state.extensionWorkbench.marketplace = [];
      if (target === "recommended") state.extensionWorkbench.recommended = [];
      throw new Error(message);
    }

    const items = filterCatalogExtensions(Array.isArray(payload.extensions) ? payload.extensions : []);
    if (target === "marketplace") {
      state.extensionWorkbench.marketplace = items;
    } else {
      state.extensionWorkbench.recommended = items;
    }

    state.extensionWorkbench.registry = {
      ...state.extensionWorkbench.registry,
      ...(payload.registry || {}),
      source: payload.source || (payload.registry && payload.registry.source) || state.extensionWorkbench.registry.source,
      syncedAt: payload.syncedAt || (payload.registry && payload.registry.syncedAt) || state.extensionWorkbench.registry.syncedAt,
      stale: !!payload.stale,
      syncing: !!payload.syncing,
      error: payload.error || payload.syncError || "",
    };
  }

  async function loadRecommendedExtensions({ silent = true } = {}) {
    try {
      const payload = await api("/api/extensions/recommended");
      applyCatalogPayload(payload, "recommended");
      if (!state.extensionWorkbench.query.trim()) renderExtensionsPanel();
    } catch (error) {
      if (!silent) reportError(error, { label: "Load recommended extensions" });
      renderExtensionsPanel();
    }
  }

  async function searchMarketplace(query) {
    const serial = ++state.extensionWorkbench.searchSerial;
    state.extensionWorkbench.loading = true;
    state.extensionWorkbench.error = "";
    renderExtensionsPanel();
    try {
      const payload = await api(`/api/extensions/marketplace?q=${encodeURIComponent(query)}`);
      if (serial !== state.extensionWorkbench.searchSerial) return;
      applyCatalogPayload(payload, "marketplace");
    } catch (error) {
      if (serial !== state.extensionWorkbench.searchSerial) return;
      state.extensionWorkbench.error = error && error.message ? error.message : "Registry catalog unavailable";
      state.extensionWorkbench.marketplace = [];
      reportError(error, { label: "Search extensions" });
    } finally {
      if (serial === state.extensionWorkbench.searchSerial) {
        state.extensionWorkbench.loading = false;
        renderExtensionsPanel();
      }
    }
  }

  function scheduleMarketplaceSearch() {
    clearTimeout(state.extensionWorkbench.searchTimer);
    const query = state.extensionWorkbench.query.trim();
    if (!query) {
      state.extensionWorkbench.marketplace = [];
      loadRecommendedExtensions({ silent: true });
      renderExtensionsPanel();
      return;
    }
    state.extensionWorkbench.searchTimer = window.setTimeout(() => {
      searchMarketplace(query);
    }, 200);
  }

  function refreshExtensionDetailAfterAction(action, id) {
    const current = findExtensionById(id);
    const activeExtension = state.activeEditorTabId === extensionTabId(id);

    if (!current) {
      if (state.extensionWorkbench.selectedId === id) {
        state.extensionWorkbench.selectedId = null;
      }
      if (activeExtension) {
        clearExtensionDetailMain();
      }
      return;
    }

    if (state.extensionWorkbench.selectedId === id || activeExtension) {
      state.extensionWorkbench.selectedId = id;
      renderExtensionDetailMain(current);
    }
  }

  function refreshExtensionWorkbenchFromRegistry() {
    const list = uniqueExtensions(
      Array.isArray(state.extensions.extensions)
        ? state.extensions.extensions
        : [],
    );

    state.extensionWorkbench.installed = list.filter(
      (ext) => !ext.builtin && ext.installed === true,
    );

    state.extensionWorkbench.builtins = list.filter((ext) => ext.builtin);

    state.extensionWorkbench.updates =
      state.extensionWorkbench.installed.filter((ext) => ext.updateAvailable);

    state.extensionWorkbench.recommended = filterCatalogExtensions(
      state.extensionWorkbench.recommended,
    );
    state.extensionWorkbench.marketplace = filterCatalogExtensions(
      state.extensionWorkbench.marketplace,
    );
  }

  function extensionCellLabels(ext) {
    const cells = Array.isArray(ext && ext.cellTypes) ? ext.cellTypes : [];

    return cells
      .map((cell) =>
        typeof cell === "string" ? cell : cell && (cell.label || cell.id),
      )
      .filter(Boolean)
      .join(", ");
  }

  function normalizeExtensionIcon(icon) {
    const value = String(icon || "").trim();
    if (
      !value ||
      /[\u0000-\u001f\u007f]/.test(value) ||
      /javascript:/i.test(value)
    ) {
      return "";
    }
    if (
      /^data:image\/(svg\+xml|png|jpeg|webp)(;charset=[^,]+)?[,;]/i.test(value)
    ) {
      if (/^data:image\/svg\+xml/i.test(value) && value.includes("#")) {
        return "";
      }
      return value;
    }
    if (value.startsWith("/assets/") && !value.includes("..")) {
      return value;
    }
    if (/^https:\/\//i.test(value)) {
      return value;
    }
    return "";
  }

  function extensionIconSource(ext) {
    const explicit = normalizeExtensionIcon(ext && ext.icon);
    if (explicit) return explicit;

    const id = extensionIdentifier(ext);
    if ((ext && ext.builtin) || id.startsWith("vix.note.")) {
      return VIX_LOGO_ICON;
    }

    return "";
  }

  function extensionIconHtml(ext, fallback = "E") {
    const safeFallback =
      String(fallback || "E")
        .trim()
        .charAt(0)
        .toUpperCase() || "E";
    const src = extensionIconSource(ext);

    return `
      ${
        src
          ? `<img src="${escapeHtml(src)}" alt="" loading="lazy" decoding="async" onload="this.nextElementSibling.hidden=true" onerror="this.hidden=true;this.nextElementSibling.hidden=false" />`
          : ""
      }
      <span class="vn-ExtensionIcon__fallback" ${src ? "hidden" : ""}>${escapeHtml(safeFallback)}</span>
    `;
  }

  function recommendedExtensions() {
    return filterCatalogExtensions(state.extensionWorkbench.recommended);
  }

  function extensionSearchText(ext) {
    return [
      extensionIdentifier(ext),
      ext && ext.name,
      ext && ext.namespace,
      ext && ext.publisher,
      ext && ext.description,
      ext && ext.version,
      extensionCellLabels(ext),
      Array.isArray(ext && ext.capabilities) ? ext.capabilities.join(" ") : "",
    ]
      .filter(Boolean)
      .join(" ")
      .toLowerCase();
  }

  function extensionMatchesQuery(ext, query) {
    const words = String(query || "")
      .trim()
      .toLowerCase()
      .split(/\s+/)
      .filter(Boolean);

    if (!words.length) return true;

    const text = extensionSearchText(ext);

    return words.every((word) => text.includes(word));
  }

  function renderExtensionItem(ext) {
    const id = extensionIdentifier(ext);
    const name = ext.name || id.split("/").pop() || id;
    const status = extensionStatus(ext);
    const cells = extensionCellLabels(ext);
    const runtimeError =
      ext.runtime && ext.runtime.error ? ext.runtime.error : "";

    const selected = state.extensionWorkbench.selectedId === id;

    const publisher =
      ext.publisher ||
      ext.namespace ||
      (id.includes("/") ? id.split("/")[0] : "Vix Registry");

    const metadata = [
      publisher,
      ext.version ? `v${ext.version}` : "",
      cells,
    ].filter(Boolean);

    const initial =
      String(name || "E")
        .trim()
        .charAt(0)
        .toUpperCase() || "E";

    const installed = !ext.builtin && ext.installed === true;
    const pendingAction = extensionPendingAction(id);
    const pending = !!pendingAction;
    const pendingLabel = pending ? extensionActionLabel(pendingAction.action) : "";

    let actions = "";

    if (!ext.builtin && !installed) {
      actions = `
      <button
        type="button"
        class="vn-ExtensionItem__button vn-ExtensionItem__button--primary"
        data-extension-action="install"
        data-extension-id="${escapeHtml(id)}"
        ${pending ? 'disabled aria-disabled=\"true\"' : ""}
      >
        ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : "Install"}
      </button>
    `;
    } else if (!ext.builtin) {
      actions = `
      <button
        type="button"
        class="vn-ExtensionItem__button"
        data-extension-action="${ext.enabled === false ? "enable" : "disable"}"
        data-extension-id="${escapeHtml(id)}"
        ${pending ? 'disabled aria-disabled=\"true\"' : ""}
      >
        ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : ext.enabled === false ? "Enable" : "Disable"}
      </button>

      <button
        type="button"
        class="vn-ExtensionItem__button"
        data-extension-action="uninstall"
        data-extension-id="${escapeHtml(id)}"
        ${pending ? 'disabled aria-disabled=\"true\"' : ""}
      >
        ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : "Uninstall"}
      </button>
    `;
    }

    return `
    <article
      class="vn-ExtensionItem${selected ? " is-selected" : ""}${pending ? " is-pending" : ""}"
      data-extension-id="${escapeHtml(id)}"
      role="listitem"
    >
      <button
        type="button"
        class="vn-ExtensionItem__main"
        data-extension-details="${escapeHtml(id)}"
        aria-label="Open ${escapeHtml(name)} extension details"
      >
        <span class="vn-ExtensionIcon vn-ExtensionItem__icon" aria-hidden="true">
          ${extensionIconHtml(ext, initial)}
          ${pending ? `<span class="vn-ExtensionItem__spinner"><span class="vn-ExtensionActionSpinner" aria-hidden="true"></span></span>` : ""}
        </span>

        <span class="vn-ExtensionItem__content">
          <span class="vn-ExtensionItem__header">
            <span class="vn-ExtensionItem__name">
              ${escapeHtml(name)}
            </span>

            <span
              class="vn-ExtensionItem__status"
              data-state="${safeClass(status)}"
            >
              ${escapeHtml(status)}
            </span>
          </span>

          <span class="vn-ExtensionItem__description">
            ${escapeHtml(
              ext.description ||
                (cells ? `${cells} cells for Vix Note.` : "Vix Note extension"),
            )}
          </span>

          <span class="vn-ExtensionItem__metadata">
            ${metadata
              .map((value) => `<span>${escapeHtml(value)}</span>`)
              .join("")}
          </span>
          ${pending ? `<span class="vn-ExtensionItem__pendingLabel">${escapeHtml(pendingLabel)}</span>` : ""}
        </span>
      </button>

      ${
        actions
          ? `
            <div class="vn-ExtensionItem__actions">
              ${actions}
            </div>
          `
          : ""
      }

      ${
        runtimeError
          ? `
            <p class="vn-ExtensionItem__error">
              ${escapeHtml(runtimeError)}
            </p>
          `
          : ""
      }
    </article>
  `;
  }

  function renderExtensionGroup(group) {
    const open = state.extensionWorkbench.sections[group.id] !== false;

    const bodyId = `vn-extension-group-${safeClass(group.id)}`;

    const items = Array.isArray(group.items) ? group.items : [];

    const content = items.length
      ? items.map(renderExtensionItem).join("")
      : `
      <p class="vn-ExtensionGroup__empty">
        ${escapeHtml(group.emptyMessage || "No extensions.")}
      </p>
    `;

    return `
    <section
      class="vn-ExtensionGroup${open ? " is-open" : " is-closed"}"
      data-extension-group="${escapeHtml(group.id)}"
    >
      <button
        type="button"
        class="vn-ExtensionGroup__header"
        data-extension-group-toggle="${escapeHtml(group.id)}"
        aria-expanded="${open ? "true" : "false"}"
        aria-controls="${bodyId}"
      >
        <svg
          class="vn-ExtensionGroup__chevron"
          viewBox="0 0 16 16"
          aria-hidden="true"
        >
          <path
            d="M5 3.5 10 8l-5 4.5"
            fill="none"
            stroke="currentColor"
            stroke-width="1.5"
            stroke-linecap="round"
            stroke-linejoin="round"
          />
        </svg>

        <span class="vn-ExtensionGroup__title">
          ${escapeHtml(group.label)}
        </span>

        <span class="vn-ExtensionGroup__count">
          ${items.length}
        </span>
      </button>

      <div
        id="${bodyId}"
        class="vn-ExtensionGroup__body"
        ${open ? "" : "hidden"}
        role="list"
      >
        ${content}
      </div>
    </section>
  `;
  }

  function extensionGroups() {
    const updates = uniqueExtensions(state.extensionWorkbench.updates);

    const installed = uniqueExtensions(
      state.extensionWorkbench.installed.filter((ext) => !ext.updateAvailable),
    );

    const recommended = recommendedExtensions();

    const builtins = uniqueExtensions(state.extensionWorkbench.builtins);

    const groups = [];

    if (updates.length) {
      groups.push({
        id: "updates",
        label: "Updates",
        items: updates,
        emptyMessage: "No extension updates.",
      });
    }

    groups.push(
      {
        id: "installed",
        label: "Installed",
        items: installed,
        emptyMessage: "No extensions installed.",
      },
      {
        id: "recommended",
        label: "Recommended",
        items: recommended,
        emptyMessage: "No recommendations available.",
      },
      {
        id: "builtin",
        label: "Built-in",
        items: builtins,
        emptyMessage: "No built-in extensions.",
      },
    );

    return groups;
  }

  function toggleExtensionGroup(groupId) {
    if (!groupId) return;

    const sections = state.extensionWorkbench.sections;

    sections[groupId] = sections[groupId] === false;

    persistUiState();
    renderExtensionsPanel();
  }

  function renderRegistryStatus() {
    const registry = state.extensionWorkbench.registry || {};
    if (!registry || registry.source === "none") {
      return `
        <p class="vn-ExtensionsRegistryStatus is-warning">
          Registry catalog unavailable. Run <code>vix registry sync</code> or refresh the Registry catalog.
        </p>
      `;
    }
    if (registry.error) {
      return `
        <p class="vn-ExtensionsRegistryStatus is-warning">
          Registry unavailable — showing cached results.
        </p>
      `;
    }
    if (registry.stale || registry.source === "cache") {
      return `
        <p class="vn-ExtensionsRegistryStatus">
          Showing cached Registry results${registry.syncedAt ? ` · ${escapeHtml(String(registry.syncedAt))}` : ""}.
        </p>
      `;
    }
    return "";
  }

  function renderExtensionsPanel() {
    const root = $(sel.extensionsList);
    if (!root) return;

    const allExtensions = uniqueExtensions([
      ...(Array.isArray(state.extensions.extensions)
        ? state.extensions.extensions
        : []),
      ...state.extensionWorkbench.marketplace,
      ...state.extensionWorkbench.recommended,
    ]);

    const count = $(sel.extensionsCount);
    if (count) {
      count.textContent = String(allExtensions.length);
    }

    if (state.extensionWorkbench.loading) {
      root.innerHTML = `
      <p class="vn-Tree__empty">
        Searching extensions…
      </p>
    `;
      return;
    }

    if (state.extensionWorkbench.error) {
      root.innerHTML = `
      <p class="vn-Tree__empty">
        ${escapeHtml(state.extensionWorkbench.error)}
      </p>
    `;
      return;
    }

    const query = state.extensionWorkbench.query.trim().toLowerCase();

    if (query) {
      const results = uniqueExtensions(state.extensionWorkbench.marketplace);

      root.innerHTML = renderRegistryStatus() + renderExtensionGroup({
        id: "search",
        label: `Marketplace Results · ${results.length}`,
        items: results,
        emptyMessage: `No Note extensions match "${query}".`,
      });

      return;
    }

    root.innerHTML = renderRegistryStatus() + extensionGroups().map(renderExtensionGroup).join("");
  }

  function extensionDetailsHtml(ext) {
    const caps = Array.isArray(ext.capabilities)
      ? ext.capabilities.join(", ")
      : "";
    const cells = Array.isArray(ext.cellTypes)
      ? ext.cellTypes
          .map((cell) => `${cell.label || cell.id} (${cell.id})`)
          .join(", ")
      : "";
    const runtime = ext.runtime || {};
    return `
      <div class="vn-ExtensionDetails">
        <dl>
          <dt>Source</dt><dd>${escapeHtml(ext.source || "unknown")}</dd>
          <dt>Cell types</dt><dd>${escapeHtml(cells || "none")}</dd>
          <dt>Capabilities</dt><dd>${escapeHtml(caps || "none")}</dd>
          <dt>Runtime</dt><dd>${escapeHtml([runtime.protocol, runtime.mode, runtime.resolvedCommand || runtime.command].filter(Boolean).join(" · ") || "none")}</dd>
          <dt>Health</dt><dd>${runtime.healthy === false ? "Broken" : "Healthy"}</dd>
        </dl>
      </div>`;
  }

  function renderExtensionDetailMain(ext) {
    const root = $(sel.cells);
    if (!root || !ext) return;

    const id = extensionIdentifier(ext);
    const tabId = extensionTabId(id);
    if (state.activeEditorTabId !== tabId) {
      state.activeEditorTabId = tabId;
    }
    setExtensionEditorActive(true);
    renderTabsBar();
    const name = ext.name || id.split("/").pop() || id;
    const status = extensionStatus(ext);
    const runtime = ext.runtime || {};

    const cells = Array.isArray(ext.cellTypes) ? ext.cellTypes : [];

    const capabilities = Array.isArray(ext.capabilities)
      ? ext.capabilities
      : [];

    const diagnostics = Array.isArray(ext.diagnostics)
      ? ext.diagnostics.filter(Boolean)
      : [];

    const publisher =
      ext.publisher ||
      ext.namespace ||
      (id.includes("/") ? id.split("/")[0] : "Vix Registry");

    const description =
      ext.description ||
      (cells.length
        ? `${extensionCellLabels(ext)} for Vix Note.`
        : "Extension for the Vix Note workspace.");

    const initial =
      String(name || "E")
        .trim()
        .charAt(0)
        .toUpperCase() || "E";

    const installed = !ext.builtin && ext.installed === true;
    const pendingAction = extensionPendingAction(id);
    const pending = !!pendingAction;
    const pendingLabel = pending ? extensionActionLabel(pendingAction.action) : "";

    const runtimeCommand = runtime.resolvedCommand || runtime.command || "";

    const runtimeStatus =
      runtime.healthy === false || ext.available === false
        ? "Broken"
        : "Healthy";

    const contributionCount = cells.length + capabilities.length;

    let primaryAction = "";

    if (!ext.builtin && !installed) {
      primaryAction = `
      <button
        type="button"
        class="vn-ExtensionShow__button vn-ExtensionShow__button--primary"
        data-extension-action="install"
        data-extension-id="${escapeHtml(id)}"
        ${pending ? 'disabled aria-disabled=\"true\"' : ""}
      >
        ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : "Install"}
      </button>
    `;
    } else if (!ext.builtin && ext.updateAvailable) {
      primaryAction = `
      <button
        type="button"
        class="vn-ExtensionShow__button vn-ExtensionShow__button--primary"
        data-extension-action="update"
        data-extension-id="${escapeHtml(id)}"
        ${pending ? 'disabled aria-disabled=\"true\"' : ""}
      >
        ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : "Update"}
      </button>
    `;
    }

    const manageActions =
      !ext.builtin && installed
        ? `
        <button
          type="button"
          class="vn-ExtensionShow__button"
          data-extension-action="${ext.enabled === false ? "enable" : "disable"}"
          data-extension-id="${escapeHtml(id)}"
          ${pending ? 'disabled aria-disabled=\"true\"' : ""}
        >
          ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : ext.enabled === false ? "Enable" : "Disable"}
        </button>

        <button
          type="button"
          class="vn-ExtensionShow__button vn-ExtensionShow__button--danger"
          data-extension-action="uninstall"
          data-extension-id="${escapeHtml(id)}"
          ${pending ? 'disabled aria-disabled=\"true\"' : ""}
        >
          ${pending ? `<span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}` : "Uninstall"}
        </button>
      `
        : "";

    const cellTypesHtml = cells.length
      ? cells
          .map((cell) => {
            const cellId = cell.id || "unknown";
            const label = cell.label || cellId;

            return `
            <article class="vn-ExtensionShow__contribution">
              <div
                class="vn-ExtensionShow__contributionIcon"
                aria-hidden="true"
              >
                &lt;/&gt;
              </div>

              <div class="vn-ExtensionShow__contributionBody">
                <strong>${escapeHtml(label)}</strong>
                <span>Cell type</span>
                <code>${escapeHtml(cellId)}</code>
              </div>
            </article>
          `;
          })
          .join("")
      : `
      <p class="vn-ExtensionShow__empty">
        This extension does not contribute any cell types.
      </p>
    `;

    const capabilitiesHtml = capabilities.length
      ? capabilities
          .map(
            (capability) => `
            <span class="vn-ExtensionShow__capability">
              ${escapeHtml(capability)}
            </span>
          `,
          )
          .join("")
      : `
      <span class="vn-ExtensionShow__muted">
        No additional capabilities declared.
      </span>
    `;

    const diagnosticsHtml = diagnostics.length
      ? `
      <section class="vn-ExtensionShow__section">
        <div class="vn-ExtensionShow__sectionHead">
          <div>
            <p class="vn-ExtensionShow__eyebrow">
              Diagnostics
            </p>
            <h2>Extension issues</h2>
          </div>

          <span class="vn-ExtensionShow__sectionCount">
            ${diagnostics.length}
          </span>
        </div>

        <div class="vn-ExtensionShow__diagnostics">
          ${diagnostics
            .map(
              (diagnostic) => `
                <p class="vn-ExtensionShow__diagnostic">
                  ${escapeHtml(diagnostic)}
                </p>
              `,
            )
            .join("")}
        </div>
      </section>
    `
      : "";

    root.innerHTML = `
    <section
      class="vn-ExtensionShow vn-ExtensionEditorView${pending ? " is-pending" : ""}"
      aria-label="${escapeHtml(name)} extension details"
    >
      <header class="vn-ExtensionShow__hero">
        <div
          class="vn-ExtensionIcon vn-ExtensionShow__icon"
          aria-hidden="true"
        >
          ${extensionIconHtml(ext, initial)}
        </div>

        <div class="vn-ExtensionShow__identity">
          <div class="vn-ExtensionShow__titleRow">
            <div>
              <h1>${escapeHtml(name)}</h1>

              <p class="vn-ExtensionShow__identifier">
                ${escapeHtml(id)}
              </p>
            </div>

            <span
              class="vn-ExtensionShow__status"
              data-state="${safeClass(status)}"
            >
              ${escapeHtml(status)}
            </span>
          </div>

          <p class="vn-ExtensionShow__description">
            ${escapeHtml(description)}
          </p>

          <div class="vn-ExtensionShow__publisher">
            <span>Published by</span>
            <strong>${escapeHtml(publisher)}</strong>

            ${
              ext.version
                ? `
                  <span aria-hidden="true">·</span>
                  <span>Version ${escapeHtml(ext.version)}</span>
                `
                : ""
            }

            ${
              ext.source
                ? `
                  <span aria-hidden="true">·</span>
                  <span>${escapeHtml(ext.source)}</span>
                `
                : ""
            }
          </div>

          ${pending ? `<div class="vn-ExtensionShow__pending"><span class="vn-ExtensionActionSpinner" aria-hidden="true"></span>${escapeHtml(pendingLabel)}</div>` : ""}

          <div class="vn-ExtensionShow__actions">
            ${primaryAction}
            ${manageActions}

            <button
              type="button"
              class="vn-ExtensionShow__button"
              data-command="extensions.refresh"
              ${pending ? 'disabled aria-disabled="true"' : ""}
            >
              Reload
            </button>
          </div>
        </div>
      </header>

      <nav
        class="vn-ExtensionShow__tabs"
        aria-label="Extension information"
      >
        <button
          type="button"
          class="is-active"
          aria-current="page"
        >
          Overview
        </button>

        <button
          type="button"
          disabled
          title="README support will be added later"
        >
          README
        </button>

        <button
          type="button"
          disabled
          title="Changelog support will be added later"
        >
          Changelog
        </button>
      </nav>

      <div class="vn-ExtensionShow__layout">
        <main class="vn-ExtensionShow__content">
          <section class="vn-ExtensionShow__section">
            <div class="vn-ExtensionShow__sectionHead">
              <div>
                <p class="vn-ExtensionShow__eyebrow">
                  Contributions
                </p>
                <h2>What this extension adds</h2>
              </div>

              <span class="vn-ExtensionShow__sectionCount">
                ${contributionCount}
              </span>
            </div>

            <div class="vn-ExtensionShow__contributions">
              ${cellTypesHtml}
            </div>

            <div class="vn-ExtensionShow__capabilities">
              ${capabilitiesHtml}
            </div>
          </section>

          <section class="vn-ExtensionShow__section">
            <div class="vn-ExtensionShow__sectionHead">
              <div>
                <p class="vn-ExtensionShow__eyebrow">
                  Runtime
                </p>
                <h2>Execution environment</h2>
              </div>

              <span
                class="vn-ExtensionShow__health"
                data-state="${safeClass(runtimeStatus)}"
              >
                <span aria-hidden="true"></span>
                ${escapeHtml(runtimeStatus)}
              </span>
            </div>

            <dl class="vn-ExtensionShow__runtime">
              <div>
                <dt>Protocol</dt>
                <dd>${escapeHtml(runtime.protocol || "Not declared")}</dd>
              </div>

              <div>
                <dt>Mode</dt>
                <dd>${escapeHtml(runtime.mode || "Not declared")}</dd>
              </div>

              <div>
                <dt>Command</dt>
                <dd>
                  ${
                    runtimeCommand
                      ? `<code>${escapeHtml(runtimeCommand)}</code>`
                      : `<span class="vn-ExtensionShow__muted">No command</span>`
                  }
                </dd>
              </div>

              <div>
                <dt>Availability</dt>
                <dd>
                  ${ext.available === false ? "Unavailable" : "Available"}
                </dd>
              </div>
            </dl>
          </section>

          ${diagnosticsHtml}

          <section class="vn-ExtensionShow__section">
            <div class="vn-ExtensionShow__sectionHead">
              <div>
                <p class="vn-ExtensionShow__eyebrow">
                  Documentation
                </p>
                <h2>README</h2>
              </div>
            </div>

            <div class="vn-ExtensionShow__readme">
              <h3>${escapeHtml(name)}</h3>

              <p>
                ${escapeHtml(description)}
              </p>

              <p class="vn-ExtensionShow__muted">
                Full README content is not available in the local
                extension registry cache yet.
              </p>
            </div>
          </section>
        </main>

        <aside class="vn-ExtensionShow__sidebar">
          <section class="vn-ExtensionShow__about">
            <h2>Extension details</h2>

            <dl>
              <div>
                <dt>Identifier</dt>
                <dd>
                  <code>${escapeHtml(id)}</code>
                </dd>
              </div>

              <div>
                <dt>Publisher</dt>
                <dd>${escapeHtml(publisher)}</dd>
              </div>

              <div>
                <dt>Version</dt>
                <dd>${escapeHtml(ext.version || "Unknown")}</dd>
              </div>

              <div>
                <dt>Source</dt>
                <dd>${escapeHtml(ext.source || "Unknown")}</dd>
              </div>

              <div>
                <dt>Type</dt>
                <dd>${ext.builtin ? "Built-in" : "External"}</dd>
              </div>

              <div>
                <dt>State</dt>
                <dd>${escapeHtml(status)}</dd>
              </div>

              <div>
                <dt>Cell types</dt>
                <dd>${cells.length}</dd>
              </div>

              <div>
                <dt>Capabilities</dt>
                <dd>${capabilities.length}</dd>
              </div>
            </dl>
          </section>
        </aside>
      </div>
    </section>
  `;

    setText(sel.statusKind, `Extension · ${name}`);
  }

  async function refreshExtensionsView() {
    state.extensionWorkbench.loading = true;
    state.extensionWorkbench.error = "";
    state.extensionWorkbench.registry = {
      ...state.extensionWorkbench.registry,
      syncing: true,
    };
    renderExtensionsPanel();
    try {
      const payload = await api("/api/extensions/reload", { method: "POST" });
      applyExtensionsPayload(payload);

      try {
        const catalog = await api("/api/extensions/registry/sync", { method: "POST" });
        applyCatalogPayload(catalog, "recommended");
        if (state.extensionWorkbench.query.trim()) {
          await searchMarketplace(state.extensionWorkbench.query.trim());
        }
        showNotification({
          type: catalog.synced === false ? "warning" : "success",
          message: catalog.synced === false
            ? "Registry unavailable — showing cached results"
            : "Registry updated",
        });
      } catch (catalogError) {
        await loadRecommendedExtensions({ silent: true });
        showNotification({
          type: "warning",
          message: "Registry unavailable — showing cached results",
        });
      }
    } catch (error) {
      reportError(error, { label: "Refresh extensions" });
    } finally {
      state.extensionWorkbench.loading = false;
      state.extensionWorkbench.registry = {
        ...state.extensionWorkbench.registry,
        syncing: false,
      };
      renderExtensionsPanel();
    }
  }

  function extensionActionText(action) {
    const labels = {
      install: {
        title: "Install extension",
        confirm: "Install",
        pending: "Installing…",
        body: "Vix Note will install this package globally and reload available cell types when the installation completes.",
      },
      update: {
        title: "Update extension",
        confirm: "Update",
        pending: "Updating…",
        body: "Vix Note will update this global package and refresh its contributed cells and runtime metadata.",
      },
      enable: {
        title: "Enable extension",
        confirm: "Enable",
        pending: "Enabling…",
        body: "Cell types provided by this extension will become available again in the editor.",
      },
      disable: {
        title: "Disable extension",
        confirm: "Disable",
        pending: "Disabling…",
        body: "Existing cells keep their source, but cells provided by this extension may become unavailable until the extension is enabled again.",
        important: true,
      },
      uninstall: {
        title: "Uninstall extension",
        confirm: "Uninstall",
        pending: "Uninstalling…",
        body: "The global package will be removed. Existing cells keep their source, but their runtime may become unavailable.",
        danger: true,
      },
    };

    return (
      labels[action] || {
        title: "Extension action",
        confirm: "Continue",
        pending: "Working…",
        body: "Vix Note will update this extension.",
      }
    );
  }

  function showExtensionActionModal(action, ext, id) {
    return new Promise((resolve) => {
      const spec = extensionActionText(action);
      const m = openModalShell(spec.title);
      if (!m) {
        resolve(true);
        return;
      }

      const name = ext?.name || id.split("/").pop() || id;
      const initial =
        String(name || "E")
          .trim()
          .charAt(0)
          .toUpperCase() || "E";

      m.body.innerHTML = `
        <div class="vn-ExtensionActionModal${spec.important ? " is-important" : ""}${spec.danger ? " is-danger" : ""}">
          <span class="vn-ExtensionIcon vn-ExtensionActionModal__icon" aria-hidden="true">
            ${extensionIconHtml(ext, initial)}
          </span>
          <div class="vn-ExtensionActionModal__content">
            <strong>${escapeHtml(name)}</strong>
            <code>${escapeHtml(id)}</code>
            <p>${escapeHtml(spec.body)}</p>
          </div>
        </div>
      `;

      m.foot.innerHTML = `
        <button type="button" class="vn-Btn vn-Btn--ghost" data-modal-cancel>Cancel</button>
        <button type="button" class="vn-Btn ${spec.danger ? "vn-Btn--danger" : "vn-Btn--primary"}" data-modal-ok>${escapeHtml(spec.confirm)}</button>
      `;

      const okBtn = $("[data-modal-ok]", m.foot);
      const cancelBtn = $("[data-modal-cancel]", m.foot);
      const allClose = $all("[data-modal-cancel], [data-modal-close]");

      const cleanup = () => {
        okBtn?.removeEventListener("click", onOk);
        for (const c of allClose) c.removeEventListener("click", onCancel);
      };

      const onOk = () => {
        cleanup();
        if (okBtn) {
          okBtn.disabled = true;
          okBtn.textContent = spec.pending;
          okBtn.setAttribute("aria-busy", "true");
        }
        if (cancelBtn) cancelBtn.disabled = true;
        resolve({ modal: m, pendingLabel: spec.pending });
      };

      const onCancel = () => {
        cleanup();
        closeModal();
        resolve(null);
      };

      okBtn?.addEventListener("click", onOk);
      for (const c of allClose) c.addEventListener("click", onCancel);
      okBtn?.focus();
    });
  }

  async function extensionAction(action, packageId) {
    const id = packageId || state.extensionWorkbench.selectedId;
    if (!id) {
      showNotification({ type: "warning", message: "No extension selected" });
      return;
    }

    const ext = findExtensionById(id);
    if (ext && ext.builtin) {
      showNotification({
        type: "warning",
        message: "Built-in extensions cannot be modified",
      });
      return;
    }

    if (isExtensionActionPending(id)) return;

    const modal = await showExtensionActionModal(action, ext, id);
    if (!modal) return;

    if (!beginExtensionAction(id, action, "preparing")) return;
    renderExtensionsPanel();
    if (state.activeEditorTabId === extensionTabId(id)) {
      renderExtensionDetailMain(findExtensionById(id) || ext);
    }

    try {
      updateExtensionActionStage(id, action === "install" ? "installing" : "working");
      renderExtensionsPanel();
      if (state.activeEditorTabId === extensionTabId(id)) {
        renderExtensionDetailMain(findExtensionById(id) || ext);
      }

      const payload = await api(`/api/extensions/${action}`, {
        method: "POST",
        body: JSON.stringify({ package: id, scope: "global" }),
      });

      updateExtensionActionStage(id, "refreshing");
      applyExtensionsPayload(payload);
      await reloadLocalExtensions();
      if (state.extensionWorkbench.query.trim()) {
        await searchMarketplace(state.extensionWorkbench.query.trim());
      } else {
        await loadRecommendedExtensions({ silent: true });
      }

      showNotification({
        type: "success",
        message: `${id}: ${action} complete`,
      });
      closeModal();
    } catch (error) {
      reportError(error, { label: `${action} ${id}` });
      const spec = extensionActionText(action);
      const m = modal.modal;
      const okBtn = m && $("[data-modal-ok]", m.foot);
      const cancelBtn = m && $("[data-modal-cancel]", m.foot);
      if (okBtn) {
        okBtn.disabled = false;
        okBtn.textContent = spec.confirm;
        okBtn.removeAttribute("aria-busy");
      }
      if (cancelBtn) cancelBtn.disabled = false;
    } finally {
      endExtensionAction(id);
      renderExtensionsPanel();
      refreshExtensionDetailAfterAction(action, id);
      renderTabsBar();
    }
  }

  function safeClass(value) {
    return String(value || "unknown")
      .toLowerCase()
      .replace(/[^a-z0-9_-]/g, "-");
  }

  function isCodeKind(kind) {
    const k = normalizeKind(kind);
    const desc = cellTypeDescriptor(k);
    return desc ? !!desc.executable : k === "cpp" || k === "reply";
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
      cellTypeOf(first) === "markdown" &&
      isDefaultIntroSource(first.source, oldTitle)
    ) {
      first.source = defaultIntroSource(newTitle);

      await api(`/api/cells/${encodeURIComponent(first.id)}`, {
        method: "PUT",
        body: JSON.stringify({
          type: cellTypeOf(first),
          kind: cellTypeOf(first),
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
    const desc = cellTypeDescriptor(kind);
    return desc && desc.label ? desc.label : cellTypeDisplayLabel(kind);
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

  function ensureNotificationHost() {
    let host = document.querySelector("[data-notifications]");
    if (!host) {
      host = document.createElement("div");
      host.className = "vn-Notifications";
      host.setAttribute("data-notifications", "");
      host.setAttribute("aria-live", "polite");
      document.body.appendChild(host);
    }
    return host;
  }

  function showNotification({
    type = "info",
    message = "",
    details = "",
    timeout = null,
  } = {}) {
    if (!message) return;
    const host = ensureNotificationHost();
    const id = `n-${Date.now()}-${Math.random().toString(16).slice(2)}`;
    const item = document.createElement("div");
    item.className = `vn-Notification vn-Notification--${safeClass(type)}`;
    item.dataset.notificationId = id;
    item.innerHTML = `
      <div class="vn-Notification__body">
        <strong>${escapeHtml(type)}</strong>
        <span>${escapeHtml(message)}</span>
        ${details ? `<small>${escapeHtml(typeof details === "string" ? details : JSON.stringify(details))}</small>` : ""}
      </div>
      <button type="button" class="vn-Notification__close" aria-label="Dismiss notification">×</button>`;
    host.appendChild(item);
    item
      .querySelector("button")
      ?.addEventListener("click", () => item.remove());
    const delay =
      timeout ?? (type === "error" || type === "warning" ? 0 : 2800);
    if (delay > 0) setTimeout(() => item.remove(), delay);
  }

  function reportError(error, context = {}) {
    console.error(error, context);
    const message =
      error && error.message
        ? error.message
        : String(error || "Unexpected error");
    setMessage(message, "error");
    showNotification({
      type: "error",
      message,
      details: context.label || context.path || "",
    });
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
        pushDraftRowIfMatches(path, depth + 2, rows);
        buildExplorerTreeRows(path, depth + 1, rows);
      } else if (child.type === "dir") {
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
        const content = String(o.data ?? o.content ?? "");
        const mime = String(o.mime || "text/plain").toLowerCase();
        if (kind === "html" && mime === "text/html") {
          return `<div class="vn-Output vn-Output--html">${content}</div>`;
        }
        if (mime === "application/json") {
          try {
            return `<div class="vn-Output vn-Output--json"><pre>${escapeHtml(JSON.stringify(JSON.parse(content), null, 2))}</pre></div>`;
          } catch (_) {}
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
    if (!isCodeKind(cellTypeOf(cell)))
      return `<div class="vn-InputPrompt vn-InputPrompt--empty"></div>`;
    const n = Number(cell.executionCount || 0);
    const label = n > 0 ? `In&nbsp;[${n}]:` : `In&nbsp;[&nbsp;]:`;
    const cls =
      n > 0 ? "vn-InputPrompt" : "vn-InputPrompt vn-InputPrompt--empty";
    return `<div class="${cls}">${label}</div>`;
  }
  function outPrompt(cell) {
    if (!isCodeKind(cellTypeOf(cell))) return "";
    const n = Number(cell.executionCount || 0);
    const outs = Array.isArray(cell.outputs) ? cell.outputs : [];
    if (!outs.length) return `<div class="vn-OutputPrompt"></div>`;
    const label = n > 0 ? `Out[${n}]:` : `Out[&nbsp;]:`;
    return `<div class="vn-OutputPrompt">${label}</div>`;
  }

  function editorBlock(cell) {
    const kind = cellTypeOf(cell);
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
    const kind = cellTypeOf(cell);
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

  function cellTypeForInsertionAfter(id) {
    const cell = findCell(id);
    const type = cell ? cellTypeOf(cell) : currentToolbarKind();
    return type && type !== "unknown" ? type : currentToolbarKind();
  }

  function cellKindOptionsHtml(current) {
    const selected = normalizeKind(current);
    const byId = new Map();
    for (const type of normalizedCellTypes()) {
      const id = normalizeKind(type.id);
      if (id) byId.set(id, { ...type, id });
    }
    if (selected && selected !== "unknown" && !byId.has(selected)) {
      byId.set(selected, {
        id: selected,
        label: cellTypeDisplayLabel(selected),
        language: selected,
        executable: false,
        unavailable: true,
      });
    }
    return Array.from(byId.values())
      .map((type) => {
        const id = normalizeKind(type.id);
        const active = id === selected;
        const hint = type.unavailable
          ? "Unavailable"
          : type.extension
            ? type.extension
            : type.builtin
              ? "Built-in"
              : type.executable
                ? "Executable"
                : "Text";
        return `<button type="button" class="vn-CellKindSelect__option${active ? " is-active" : ""}" data-cell-kind-option="${escapeHtml(id)}" role="menuitemradio" aria-checked="${active ? "true" : "false"}">
          <span>${escapeHtml(type.label || id)}</span>
          <small>${escapeHtml(hint)}</small>
        </button>`;
      })
      .join("");
  }

  function cellKindMenu(cell) {
    const id = String(cell.id || "");
    const kind = cellTypeOf(cell);
    const label = toolbarKindLabel(kind);
    return `<div class="vn-CellKindSelect" data-cell-kind-select>
      <button type="button" class="vn-CellKindSelect__button" data-cell-kind-menu="${escapeHtml(id)}" aria-haspopup="menu" aria-expanded="false" title="Change cell type">
        <span>${escapeHtml(label)}</span>
        <small>${escapeHtml(kind)}</small>
      </button>
      <div class="vn-CellKindSelect__menu" data-cell-kind-menu-panel hidden role="menu" aria-label="Cell type">
        ${cellKindOptionsHtml(kind)}
      </div>
    </div>`;
  }

  function closeCellKindMenus(exceptId = "") {
    for (const root of $all("[data-cell-kind-select]")) {
      const button = root.querySelector("[data-cell-kind-menu]");
      const panel = root.querySelector("[data-cell-kind-menu-panel]");
      if (!button || !panel) continue;
      if (exceptId && button.getAttribute("data-cell-kind-menu") === exceptId) continue;
      button.setAttribute("aria-expanded", "false");
      panel.setAttribute("hidden", "");
      root.classList.remove("is-open");
    }
  }

  function toggleCellKindMenu(id) {
    const safeId = String(id || "");
    const button = $(`[data-cell-kind-menu="${cssEscape(safeId)}"]`);
    const root = button ? button.closest("[data-cell-kind-select]") : null;
    const panel = root ? root.querySelector("[data-cell-kind-menu-panel]") : null;
    if (!button || !root || !panel) return;
    const opening = panel.hasAttribute("hidden");
    closeCellKindMenus(safeId);
    if (opening) {
      panel.removeAttribute("hidden");
      button.setAttribute("aria-expanded", "true");
      root.classList.add("is-open");
    } else {
      panel.setAttribute("hidden", "");
      button.setAttribute("aria-expanded", "false");
      root.classList.remove("is-open");
    }
  }

  function cellToolbar(cell) {
    const availability = executionAvailabilityForCell(cell);
    const firstBtn = availability.executable
      ? `<button type="button" data-cell-action="run" title="${escapeHtml(availability.available ? "Run" : availability.reason)}" ${availability.available ? "" : "disabled"}>${ICONS.run}</button>`
      : `<button type="button" data-cell-action="edit" title="Edit source">${ICONS.edit}</button>`;
    return `
      <div class="vn-Cell__toolbar">
        ${cellKindMenu(cell)}
        ${firstBtn}
        <button type="button" data-cell-action="duplicate" title="Duplicate">${ICONS.copy}</button>
        <button type="button" data-cell-action="up" title="Move up">${ICONS.up}</button>
        <button type="button" data-cell-action="down" title="Move down">${ICONS.down}</button>
        <button type="button" data-cell-action="delete" title="Delete">${ICONS.del}</button>
      </div>`;
  }

  function renderCell(cell) {
    const kind = cellTypeOf(cell);
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
    state.activeEditorTabId = null;
    setExtensionEditorActive(false);

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
    if (!cell) {
      setText(sel.statusKind, "—");
    } else {
      const availability = executionAvailabilityForCell(cell);
      const label = kindLabel(cellTypeOf(cell));
      const ext = availability.extension
        ? extensionDescriptorForCellType(cellTypeOf(cell))
        : null;
      setText(
        sel.statusKind,
        availability.executable && availability.available && ext && !ext.builtin
          ? `${label} · ${ext.name || ext.id}`
          : availability.executable && !availability.available
            ? `${label} unavailable`
            : label,
      );
    }
  }

  /* ==========================================================
   * Keyed reconcile — guarantees one cell = one DOM node
   * ======================================================== */
  function cellSignature(cell) {
    const kind = cellTypeOf(cell);
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
    setExtensionEditorActive(false);
    if (doc.path && !isVirtualUnsavedDocument(doc)) {
      state.activeEditorTabId = documentTabId(doc.path);
    }

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

  function insertAutoIndent(textarea) {
    const value = textarea.value;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const lineStart = value.lastIndexOf("\n", Math.max(0, start - 1)) + 1;
    const line = value.slice(lineStart, start);
    const indent = (line.match(/^\s*/) || [""])[0];
    const extra = /[\{\[\(]\s*$/.test(line) ? EDITOR_INDENT : "";
    const text = "\n" + indent + extra;
    textarea.value = value.slice(0, start) + text + value.slice(end);
    textarea.selectionStart = textarea.selectionEnd = start + text.length;
    markTextareaChanged(textarea);
  }

  const PAIRS = { "(": ")", "[": "]", "{": "}", '"': '"', "'": "'" };
  function handlePairInsertion(textarea, key) {
    const close = PAIRS[key];
    if (!close) return false;
    const value = textarea.value;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const selected = value.slice(start, end);
    textarea.value =
      value.slice(0, start) + key + selected + close + value.slice(end);
    textarea.selectionStart = start + 1;
    textarea.selectionEnd = start + 1 + selected.length;
    markTextareaChanged(textarea);
    return true;
  }

  function handleClosingPair(textarea, key) {
    const start = textarea.selectionStart;
    if (textarea.selectionStart !== textarea.selectionEnd) return false;
    if (textarea.value[start] !== key) return false;
    textarea.selectionStart = textarea.selectionEnd = start + 1;
    return true;
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

  function renderToolbarKindOptions() {
    const menu = $("[data-toolbar-kind-menu]");
    if (!menu) return;

    const current = normalizeKind(
      $(sel.toolbarKind)?.dataset.kind || state.document?.defaultKind || "cpp",
    );
    const types = normalizedCellTypes();
    menu.innerHTML = types
      .map((type) => {
        const id = normalizeKind(type.id);
        const active = id === current;
        const hint = type.builtin
          ? type.executable
            ? "Built-in executable cell"
            : "Built-in text cell"
          : `${type.extension || "Extension"}${type.executable ? " · executable" : ""}`;
        return `
          <button
            type="button"
            class="vn-CellTypeSelect__option${active ? " is-active" : ""}"
            data-kind-option="${escapeHtml(id)}"
            role="option"
            aria-selected="${active ? "true" : "false"}"
          >
            <span class="vn-CellTypeSelect__optionMain">${escapeHtml(type.label || id)}</span>
            <span class="vn-CellTypeSelect__optionHint">${escapeHtml(hint)}</span>
          </button>`;
      })
      .join("");
  }

  function toolbarKindLabel(kind) {
    const desc = cellTypeDescriptor(kind);
    if (desc) return desc.label || desc.id;
    const id = normalizeKind(kind);
    return id ? id : "C++";
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
    const candidate = normalizeKind(kind);
    const nextKind = cellTypeDescriptor(candidate) ? candidate : "cpp";

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
    setToolbarKind(currentToolbarKind(), { applyToCell: false });
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
  async function pushCell(cellEl, options = {}) {
    const cell = localUpdateFromDom(cellEl);
    if (!cell) return;
    const requestedType = normalizeKind(options.type || cellTypeOf(cell));
    setCellType(cell, requestedType);
    cellEl.dataset.kind = requestedType;
    const id = encodeURIComponent(cellEl.dataset.cellId);
    const result = await api(`/api/cells/${id}`, {
      method: "PUT",
      body: JSON.stringify({ type: requestedType, source: cell.source }),
    });
    if (result.document) {
      state.document = unwrapDocument(result.document);
      const updated = findCell(cellEl.dataset.cellId);
      if (updated && cellTypeOf(updated) !== requestedType) {
        console.warn("Cell type changed during sync", {
          id: cellEl.dataset.cellId,
          requestedType,
          receivedType: cellTypeOf(updated),
        });
      }
    }
  }

  /* ==========================================================
   * Dirty tracking (per active tab)
   * ======================================================== */
  function documentSnapshot(doc = state.document) {
    if (!doc) return "";
    try {
      return JSON.stringify({
        title: doc.title || "",
        path: doc.path || "",
        cells: cells().map((c) => ({
          type: cellTypeOf(c),
          source: c.source || "",
        })),
      });
    } catch (_) {
      return String(Date.now());
    }
  }

  function setDirty(dirty) {
    const tab = activeTab();
    if (tab) {
      tab.dirty = !!dirty;
      tab.lastModifiedAt = dirty
        ? Date.now()
        : tab.lastModifiedAt || Date.now();
      if (!dirty) tab.lastSavedSnapshot = documentSnapshot();
      if (dirty && tab.preview) tab.preview = false;
    }
    persistTabs();
    persistSession();
    renderOpenTabs();
    renderTabsBar();
    updateStatusBar();
    scheduleAutoSave();
  }

  function scheduleAutoSave() {
    clearTimeout(state.autoSave.timer);
    if (state.autoSave.mode !== "afterDelay" || !isDirty()) return;
    state.autoSave.timer = setTimeout(() => {
      if (!state.pending.saving && !state.busy)
        saveNote().catch((error) => reportError(error, { label: "Auto Save" }));
    }, state.autoSave.delay);
  }

  function toggleAutoSave() {
    state.autoSave.mode = state.autoSave.mode === "off" ? "afterDelay" : "off";
    showNotification({
      type: "info",
      message: `Auto Save: ${state.autoSave.mode}`,
    });
    persistUiState();
    scheduleAutoSave();
  }
  function isDirty() {
    const tab = activeTab();
    return !!(tab && tab.dirty);
  }

  /* ==========================================================
   * Cell actions
   * ======================================================== */
  function defaultSource(kind) {
    const desc = cellTypeDescriptor(kind);
    if (desc && desc.defaultSource) return desc.defaultSource;
    const lang = normalizeKind((desc && desc.language) || kind);
    const line = desc && desc.commentLine;
    if (line) return `${line} Write your explanation here.\n`;
    if (lang === "markdown") return "Write your explanation here.";
    if (lang === "python" || lang === "py")
      return "# Write your explanation here.\n";
    if (lang === "html") return "<!-- Write your explanation here. -->\n";
    if (
      lang === "css" ||
      lang === "javascript" ||
      lang === "typescript" ||
      lang === "cpp" ||
      lang === "c++" ||
      lang === "reply"
    ) {
      return "// Write your explanation here.\n";
    }
    return "# Write your explanation here.\n";
  }

  async function addCell(
    kind,
    { afterId = null, atIndex = null, source = null } = {},
  ) {
    setMessage("");
    setBusy(true);
    const requestedType = cellTypeDescriptor(kind)
      ? normalizeKind(kind)
      : "cpp";
    const body = {
      type: requestedType,
      source: source != null ? source : defaultSource(requestedType),
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
        const created = findCell(newId);
        selectCell(newId, { edit: cellTypeOf(created) !== "markdown" });
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
    await addCell(cellTypeOf(cell), {
      afterId: id,
      source: String(cell.source || ""),
    });
  }

  async function runCellById(id) {
    const cellEl = cellElById(id);
    const cell = findCell(id);
    if (!cellEl || !cell) return;

    const realType = cellTypeOf(cell);
    const domType = normalizeKind(cellEl.dataset.kind || realType);
    if (domType !== realType) {
      console.warn("Cell DOM type diverged from model before run", {
        id,
        domType,
        modelType: realType,
      });
      cellEl.dataset.kind = realType;
    }

    const availability = executionAvailabilityForCell(cell);
    if (!availability.executable) {
      localUpdateFromDom(cellEl);
      try {
        await pushCell(cellEl, { type: realType });
      } catch (_) {}
      return;
    }
    if (!availability.available) {
      setMessage(
        availability.reason || "Cell runtime is unavailable.",
        "warning",
      );
      showNotification({
        type: "warning",
        message: availability.reason || "Cell runtime is unavailable",
      });
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
      await pushCell(cellEl, { type: realType });
      const synced = findCell(id);
      if (synced && cellTypeOf(synced) !== realType) {
        console.warn("Cell type changed after sync before run", {
          id,
          requestedType: realType,
          receivedType: cellTypeOf(synced),
        });
      }
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

      setCellDiagnostics(id, result?.result);

      if (status === "failure") {
        setKernel("error");

        /*
         * Do not show a global flash for normal C++ execution errors.
         * The error already appears in the cell output and in the Problems panel.
         */
        clearMessageQuietly();

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

      setRunAllDiagnostics(result);

      if (result.ok) {
        setKernel("idle");
        clearMessageQuietly();
      } else if (result.stopped) {
        setKernel("error");
        clearMessageQuietly();
        setTimeout(() => setKernel("idle"), 1200);
      } else {
        setKernel("error");
        clearMessageQuietly();
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
    const previousType = cellTypeOf(cell);
    const nextType = cellTypeDescriptor(newKind)
      ? normalizeKind(newKind)
      : previousType;
    if (previousType === nextType) {
      selectCell(id, { edit: false });
      return;
    }
    localUpdateFromDom(cellEl);
    setCellType(cell, nextType);
    cellEl.classList.add("is-updating-type");
    try {
      const key = encodeURIComponent(id);
      const result = await api(`/api/cells/${key}`, {
        method: "PUT",
        body: JSON.stringify({ type: nextType, source: cell.source }),
      });
      if (result.document) {
        state.selectedId = String(id);
        renderDocument(result.document);
      } else if (result.cell) {
        Object.assign(cell, result.cell);
        setCellType(cell, cell.type || nextType);
        renderCells();
      }
      selectCell(id, { edit: false });
      setToolbarKind(cellTypeOf(findCell(id) || cell), { applyToCell: false });
      setDirty(true);
    } catch (error) {
      setCellType(cell, previousType);
      setToolbarKind(previousType, { applyToCell: false });
      setMessage(error.message || "Failed to change cell type.", "error");
      showNotification({
        type: "error",
        message: error.message || "Failed to change cell type.",
      });
    } finally {
      const el = cellElById(id);
      if (el) el.classList.remove("is-updating-type");
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
   * ======================================================== */
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
    const search = $(sel.explorerSearch);
    if (search && search.value) {
      search.value = "";
    }

    const parent = inlineCreateParent(explicitDir);

    if (parent !== ".") {
      state.explorer.expandedDirs.add(parent);
    }

    state.explorer.selectedDirPath = parent;
    state.explorer.currentPath = parent;
    state.explorer.draft = { kind, parentPath: parent, error: null };

    if (state.activePanel !== "explorer" || state.sidebarCollapsed) {
      setPanel(state.activePanel || "explorer");
    }

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

  function newNote(dir = null) {
    return startInlineCreate("file", dir);
  }
  function newFolder(parentDir = null) {
    return startInlineCreate("dir", parentDir);
  }

  /* ==========================================================
   * Open note
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

      clearDiagnostics();

      openTab(d.path, documentDisplayTitle(d), {
        preview: !!options.preview,
        permanent: !!options.permanent,
      });
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

      applyPathMoveToState(oldPath, newPath, type);

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

  /*
   * Shared state surgery for rename + move. Updates explorer entries,
   * loaded/expanded dir sets, selection, tabs and active document path
   * so that an old path (file or dir) becomes a new path everywhere.
   */
  function applyPathMoveToState(oldPath, newPath, type) {
    const oldNorm = normalizeExplorerPath(oldPath);
    const newNorm = normalizeExplorerPath(newPath);
    if (oldNorm === newNorm) return;

    const oldEntry = state.explorer.entries.get(oldNorm);
    state.explorer.entries.delete(oldNorm);

    const oldPrefix = `${oldNorm}/`;
    const newPrefix = `${newNorm}/`;
    for (const [key, entry] of Array.from(state.explorer.entries.entries())) {
      if (key.startsWith(oldPrefix)) {
        const movedChildPath = normalizeExplorerPath(
          newPrefix + key.slice(oldPrefix.length),
        );
        state.explorer.entries.delete(key);
        state.explorer.entries.set(movedChildPath, {
          ...entry,
          path: movedChildPath,
          title: baseName(movedChildPath),
        });
      }
    }

    state.explorer.entries.set(newNorm, {
      ...(oldEntry || {}),
      path: newNorm,
      type,
      title: baseName(newNorm),
      modified: Date.now(),
      openable: type === "file" ? newNorm.endsWith(".vixnote") : false,
      extension: type === "file" ? ".vixnote" : "",
    });

    if (state.explorer.loadedDirs.has(oldNorm)) {
      state.explorer.loadedDirs.delete(oldNorm);
      state.explorer.loadedDirs.add(newNorm);
    }
    if (state.explorer.expandedDirs.has(oldNorm)) {
      state.explorer.expandedDirs.delete(oldNorm);
      state.explorer.expandedDirs.add(newNorm);
    }
    // Re-map any loaded/expanded descendants of a moved directory.
    for (const set of [
      state.explorer.loadedDirs,
      state.explorer.expandedDirs,
    ]) {
      for (const dir of Array.from(set)) {
        if (dir.startsWith(oldPrefix)) {
          set.delete(dir);
          set.add(
            normalizeExplorerPath(newPrefix + dir.slice(oldPrefix.length)),
          );
        }
      }
    }

    if (state.explorer.selectedDirPath === oldNorm) {
      state.explorer.selectedDirPath = newNorm;
    }
    if (state.explorer.currentPath === oldNorm) {
      state.explorer.currentPath = newNorm;
    }

    // Tabs: exact match (file move/rename) and prefixed (dir move/rename).
    for (const tab of state.tabs) {
      const tabPath = normalizeExplorerPath(tab.path);
      if (tabPath === oldNorm) {
        const oldTitle = noteTitleFromPath(oldNorm);
        const newTitle = noteTitleFromPath(newNorm);
        tab.path = newNorm;
        if (
          !tab.title ||
          tab.title === baseName(oldNorm) ||
          tab.title === oldTitle
        ) {
          tab.title = newTitle;
        }
      } else if (tabPath.startsWith(oldPrefix)) {
        tab.path = normalizeExplorerPath(
          newPrefix + tabPath.slice(oldPrefix.length),
        );
      }
    }

    if (state.activeTabPath) {
      const activeNorm = normalizeExplorerPath(state.activeTabPath);
      if (activeNorm === oldNorm) {
        state.activeTabPath = newNorm;
      } else if (activeNorm.startsWith(oldPrefix)) {
        state.activeTabPath = normalizeExplorerPath(
          newPrefix + activeNorm.slice(oldPrefix.length),
        );
      }
    }

    // Active document path (keep the document open after a move/rename).
    if (
      state.document &&
      normalizeExplorerPath(state.document.path) === oldNorm
    ) {
      state.document.path = newNorm;
    } else if (
      state.document &&
      normalizeExplorerPath(state.document.path).startsWith(oldPrefix)
    ) {
      state.document.path = normalizeExplorerPath(
        newPrefix +
          normalizeExplorerPath(state.document.path).slice(oldPrefix.length),
      );
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
   * Explorer drag and drop (move files / folders)
   * ======================================================== */

  // A move is allowed when:
  //  - we are not moving the root
  //  - source and destination differ
  //  - a folder is not dropped into itself or any of its descendants
  //  - the file/folder is not already a direct child of the target
  function canMoveExplorerPath(sourcePath, sourceType, targetDir) {
    const src = normalizeExplorerPath(sourcePath);
    const dir = normalizeExplorerPath(targetDir);

    if (!src || src === ".") return false; // never move the root
    if (!dir) return false;

    // Dropping a folder into itself.
    if (sourceType === "dir" && dir === src) return false;

    // Dropping a folder into one of its own descendants.
    if (sourceType === "dir" && dir.startsWith(`${src}/`)) return false;

    // No-op: already a direct child of the target directory.
    if (parentPath(src) === dir) return false;

    return true;
  }

  async function moveExplorerPath(sourcePath, targetDir) {
    const src = normalizeExplorerPath(sourcePath);
    const dir = normalizeExplorerPath(targetDir);

    const entry = state.explorer.entries.get(src);
    const type = entry ? entry.type : src.endsWith(".vixnote") ? "file" : "dir";

    if (!canMoveExplorerPath(src, type, dir)) {
      return;
    }

    const oldParent = parentPath(src);
    setBusy(true);
    setMessage("");

    try {
      const result = await api("/api/path/move", {
        method: "POST",
        body: JSON.stringify({ path: src, directory: dir }),
      });

      if (!result || result.ok === false) {
        throw new Error(result?.error || "Failed to move path.");
      }

      const newPath = normalizeExplorerPath(
        result.newPath || joinExplorerPath(dir, baseName(src)),
      );

      // Reflect the move across explorer entries, tabs and active document.
      applyPathMoveToState(src, newPath, type);

      if (dir !== ".") {
        state.explorer.expandedDirs.add(dir);
      }

      // Silently reload both the old and the new parent directories.
      await loadDirectory(oldParent, { silent: true, force: true });
      await loadDirectory(dir, { silent: true, force: true });

      renderExplorer();
      renderOpenTabs();
      renderTabsBar();
      persistTabs();
      // No success flash: a move is a quiet action.
      clearMessageQuietly();
    } catch (error) {
      setMessage(error.message || "Failed to move path.", "error");
    } finally {
      setBusy(false);
    }
  }

  function clearExplorerDropTargets() {
    for (const el of $all(".vn-Tree__row.is-drop-target")) {
      el.classList.remove("is-drop-target");
    }
    const listEl = $(sel.explorerList);
    if (listEl) listEl.classList.remove("is-root-drop-target");
  }

  function explorerDropDirForRow(row) {
    // Returns the target directory for a drop on this row, or null if the
    // row is not a valid drop target.
    if (!row) return null;
    const path = row.getAttribute("data-tree-path");
    const type = row.getAttribute("data-tree-type");
    if (!path) return null;
    // Only folders (and the root row ".") are valid drop targets.
    if (type === "dir") return normalizeExplorerPath(path);
    return null;
  }

  /* ==========================================================
   * Outputs (client-side display only)
   * ======================================================== */
  function copyCell(mode = "copy") {
    const cell = targetId() ? findCell(targetId()) : null;
    if (!cell) return;
    state.cellClipboard = {
      mode,
      cell: {
        type: cellTypeOf(cell),
        kind: cellTypeOf(cell),
        source: cell.source || "",
        metadata: cell.metadata || {},
      },
    };
    showNotification({
      type: "info",
      message: mode === "cut" ? "Cell cut" : "Cell copied",
    });
  }

  async function pasteCell(position = "below") {
    const clip = state.cellClipboard.cell;
    if (!clip) return;
    const id = targetId();
    const idx = id ? cellIndex(id) : cells().length - 1;
    const atIndex = position === "above" ? Math.max(0, idx) : idx + 1;
    const created = await addCell(
      clip.type || clip.kind || currentToolbarKind(),
      { atIndex, source: clip.source || "" },
    );
    if (state.cellClipboard.mode === "cut" && id) {
      await deleteCellById(id);
      state.cellClipboard = { mode: "copy", cell: null };
    }
    return created;
  }

  function clearCellOutput(id) {
    const cellEl = cellElById(id);
    if (!cellEl) return;
    const oa = $(".vn-OutputArea", cellEl);
    if (oa) oa.remove();
    const cell = findCell(id);
    if (cell) cell.outputs = [];
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
  async function loadExtensions() {
    try {
      const payload = await api("/api/extensions");
      applyExtensionsPayload(payload);
      await loadRecommendedExtensions({ silent: true });
      applyTheme(localStorage.getItem(THEME_STORAGE_KEY) || "system", {
        persist: false,
      });
    } catch (error) {
      console.error("Failed to load Vix Note extensions", error);
      state.extensions = { ok: false, extensions: [], cellTypes: [] };
      state.extensionWorkbench.error =
        error && error.message ? error.message : "Extension discovery failed";
      reportError(error, { label: "Failed to load Vix Note extensions" });
    }

    refreshExtensionWorkbenchFromRegistry();
    renderToolbarKindOptions();
    renderExtensionsPanel();
  }

  async function loadDocument() {
    setMessage("");
    await loadExtensions();
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

    // Remove stale children of this directory that no longer exist on disk.
    const incomingPaths = new Set();
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
      incomingPaths.add(path);
    }

    for (const key of Array.from(state.explorer.entries.keys())) {
      if (key === dirPath) continue;
      if (parentPath(key) !== dirPath) continue;
      if (!incomingPaths.has(key)) {
        // Drop the stale entry and any of its descendants.
        state.explorer.entries.delete(key);
        const prefix = `${key}/`;
        for (const child of Array.from(state.explorer.entries.keys())) {
          if (child.startsWith(prefix)) state.explorer.entries.delete(child);
        }
      }
    }

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
    if (!silent) clearMessageQuietly();
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
      if (!silent) clearMessageQuietly();
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

    const realCount = entries.filter((e) => !e.isDraft).length;
    setText(sel.explorerCount, String(realCount));

    if (loadingPath && !entries.length && !state.explorer.draft) {
      listEl.setAttribute("role", "tree");
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        Loading ${escapeHtml(loadingPath)}…
      </p>`;
      return;
    }

    if (!entries.length) {
      listEl.setAttribute("role", "tree");
      listEl.innerHTML = `
      <p class="vn-Tree__empty">
        No notes found. Create one with <strong>New note</strong> or refresh the explorer.
      </p>`;
      return;
    }

    listEl.setAttribute("role", "tree");

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

        // The root row and every file/dir except root are draggable.
        // All folders (and the root) are drop targets.
        const draggable = path !== "." ? ' draggable="true"' : "";

        return `
        <div
          class="vn-Tree__row${active}${loading}${expanded ? " is-expanded" : ""}"
          data-tree-path="${escapeHtml(path)}"
          data-tree-type="${escapeHtml(e.type)}"
          data-tree-openable="${e.openable ? "true" : "false"}"
          role="treeitem"
          aria-level="${depth + 1}"
          aria-selected="${path === state.explorer.currentPath ? "true" : "false"}"
          ${e.type === "dir" ? `aria-expanded="${expanded ? "true" : "false"}"` : ""}
          style="--depth:${depth}"
          tabindex="0"${draggable}
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

  function safeStorageGet(key) {
    try {
      return window.localStorage.getItem(key);
    } catch (_) {
      return null;
    }
  }

  function safeStorageSet(key, value) {
    try {
      window.localStorage.setItem(key, value);
    } catch (_) {}
  }

  function clampNumber(value, fallback, min, max) {
    const n = Number(value);
    if (!Number.isFinite(n)) return fallback;
    return Math.min(max, Math.max(min, n));
  }

  function persistUiState() {
    safeStorageSet(
      UI_STATE_KEY,
      JSON.stringify({
        version: 2,
        sidebarWidth: state.sidebarWidth,
        sidebarCollapsed: state.sidebarCollapsed,
        bottomPanelHeight: state.bottomPanelHeight,
        bottomPanelVisible: state.bottomPanelVisible,
        activePanel: state.activePanel,
        focusMode: state.focusMode,
        autoSave: state.autoSave.mode,
        extensionSections: state.extensionWorkbench.sections,
      }),
    );
  }

  function restoreUiState() {
    const raw = safeStorageGet(UI_STATE_KEY);
    if (!raw) return;
    try {
      const parsed = JSON.parse(raw);
      if (!parsed || parsed.version !== 2) return;
      state.sidebarWidth = clampNumber(
        parsed.sidebarWidth,
        DEFAULT_SIDEBAR_WIDTH,
        MIN_SIDEBAR_WIDTH,
        MAX_SIDEBAR_WIDTH,
      );
      state.sidebarCollapsed = !!parsed.sidebarCollapsed;
      state.bottomPanelHeight = clampNumber(
        parsed.bottomPanelHeight,
        220,
        120,
        520,
      );
      state.bottomPanelVisible = !!parsed.bottomPanelVisible;
      state.activePanel = ["explorer", "problems", "extensions"].includes(
        parsed.activePanel,
      )
        ? parsed.activePanel
        : "explorer";
      if (
        parsed.extensionSections &&
        typeof parsed.extensionSections === "object"
      ) {
        state.extensionWorkbench.sections = {
          ...state.extensionWorkbench.sections,
          ...parsed.extensionSections,
        };
      }
      state.focusMode = !!parsed.focusMode;
      state.autoSave.mode =
        parsed.autoSave === "afterDelay" ? "afterDelay" : "off";
    } catch (_) {}
  }

  function persistSession() {
    safeStorageSet(
      SESSION_STATE_KEY,
      JSON.stringify({
        version: 2,
        tabs: state.tabs
          .filter((tab) => isDocumentTab(tab))
          .map((tab) => ({
            kind: "document",
            path: normalizeExplorerPath(tab.path),
            title: tab.title || baseName(tab.path),
            dirty: !!tab.dirty,
            preview: !!tab.preview,
            lastModifiedAt: tab.lastModifiedAt || 0,
          })),
        activeTabPath: state.activeTabPath
          ? normalizeExplorerPath(state.activeTabPath)
          : null,
        selectedId: state.selectedId,
        closedTabs: state.closedTabs.slice(0, MAX_CLOSED_TABS),
      }),
    );
  }

  function restoreSession() {
    const raw = safeStorageGet(SESSION_STATE_KEY);
    if (!raw) return false;
    try {
      const parsed = JSON.parse(raw);
      if (!parsed || parsed.version !== 2) return false;
      if (Array.isArray(parsed.tabs) && parsed.tabs.length) {
        state.tabs = parsed.tabs
          .filter((t) => t && t.path)
          .map((t) => ({
            kind: "document",
            path: normalizeExplorerPath(t.path),
            title: t.title || baseName(t.path),
            dirty: !!t.dirty,
            preview: !!t.preview,
            lastModifiedAt: Number(t.lastModifiedAt || 0),
          }));
        state.activeTabPath = parsed.activeTabPath
          ? normalizeExplorerPath(parsed.activeTabPath)
          : state.tabs[0]?.path || null;
        state.activeEditorTabId = state.activeTabPath ? documentTabId(state.activeTabPath) : null;
      }
      state.closedTabs = Array.isArray(parsed.closedTabs)
        ? parsed.closedTabs.slice(0, MAX_CLOSED_TABS)
        : [];
      if (parsed.selectedId) state.selectedId = parsed.selectedId;
      return true;
    } catch (_) {
      return false;
    }
  }

  function persistTabs() {
    try {
      const payload = {
        activeTabPath: state.activeTabPath,
        tabs: state.tabs
          .filter((tab) => isDocumentTab(tab))
          .map((tab) => ({
            kind: "document",
            path: tab.path,
            title: tab.title || baseName(tab.path),
            dirty: false,
          })),
      };
      localStorage.setItem(TABS_STORAGE_KEY, JSON.stringify(payload));
      persistSession();
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
          kind: "document",
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

      state.activeEditorTabId = state.activeTabPath ? documentTabId(state.activeTabPath) : null;
      return true;
    } catch (_) {
      state.tabs = [];
      state.activeTabPath = null;
      state.activeEditorTabId = null;
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


  function documentTabId(path) {
    return `document:${normalizeExplorerPath(path || "")}`;
  }

  function extensionTabId(extensionId) {
    return `extension:${String(extensionId || "")}`;
  }

  function isExtensionTab(tab) {
    return !!tab && tab.kind === "extension";
  }

  function isDocumentTab(tab) {
    return !!tab && (!tab.kind || tab.kind === "document");
  }

  function editorTabId(tab) {
    if (!tab) return "";
    return isExtensionTab(tab) ? extensionTabId(tab.extensionId) : documentTabId(tab.path);
  }

  function setExtensionEditorActive(active) {
    if (app) app.classList.toggle("is-extension-tab-active", !!active);
  }

  /* ==========================================================
   * Tabs
   * ======================================================== */
  function activeTab() {
    const wanted = state.activeEditorTabId || (state.activeTabPath ? documentTabId(state.activeTabPath) : "");
    return state.tabs.find((t) => editorTabId(t) === wanted) || null;
  }

  function activeDocumentTab() {
    return state.tabs.find((t) => isDocumentTab(t) && t.path === state.activeTabPath) || null;
  }

  function openTab(path, title, options = {}) {
    if (!path) return;
    const normalized = normalizeExplorerPath(path);
    const preview = !!options.preview && !options.permanent;
    let tab = state.tabs.find(
      (t) => normalizeExplorerPath(t.path) === normalized,
    );

    if (!tab && preview) {
      const existingPreview = state.tabs.find((t) => isDocumentTab(t) && t.preview && !t.dirty);
      if (existingPreview) {
        existingPreview.path = normalized;
        existingPreview.title = title || baseName(normalized);
        existingPreview.preview = true;
        existingPreview.dirty = false;
        tab = existingPreview;
      }
    }

    if (!tab) {
      tab = {
        kind: "document",
        path: normalized,
        title: title || baseName(normalized),
        dirty: false,
        preview,
        lastSavedSnapshot: "",
        lastModifiedAt: Date.now(),
      };
      state.tabs.push(tab);
    } else {
      if (title) tab.title = title;
      if (options.permanent) tab.preview = false;
      else if (preview) tab.preview = true;
    }
    state.activeTabPath = normalized;
    state.activeEditorTabId = documentTabId(normalized);
    setExtensionEditorActive(false);
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
      const tab = activeDocumentTab();
      if (tab) tab.title = title;
    }
  }

  async function openExtensionTab(extensionIdOrExtension) {
    const input = typeof extensionIdOrExtension === "string"
      ? extensionIdOrExtension
      : extensionIdentifier(extensionIdOrExtension);
    const id = input || "";
    if (!id) return;
    const ext = findExtensionById(id);
    if (!ext) {
      showNotification({ type: "warning", message: "Extension details are unavailable" });
      return;
    }
    const tabId = extensionTabId(id);
    let tab = state.tabs.find((t) => editorTabId(t) === tabId);
    const title = ext.name || id.split("/").pop() || id;
    if (!tab) {
      tab = {
        kind: "extension",
        extensionId: id,
        title,
        icon: extensionIconSource(ext),
        dirty: false,
      };
      state.tabs.push(tab);
    } else {
      tab.title = title;
      tab.icon = extensionIconSource(ext);
    }
    state.activeEditorTabId = tabId;
    state.extensionWorkbench.selectedId = id;
    setExtensionEditorActive(true);
    persistTabs();
    renderTabsBar();
    renderExtensionsPanel();
    renderExtensionDetailMain(ext);
  }

  async function switchTab(tabRef) {
    if (!tabRef) return;
    const found = state.tabs.find((t) => editorTabId(t) === tabRef) ||
      state.tabs.find((t) => isDocumentTab(t) && normalizeExplorerPath(t.path) === normalizeExplorerPath(tabRef));
    if (!found) return;
    const nextId = editorTabId(found);
    if (nextId === state.activeEditorTabId) return;

    if (isExtensionTab(found)) {
      state.activeEditorTabId = nextId;
      state.extensionWorkbench.selectedId = found.extensionId;
      setExtensionEditorActive(true);
      persistTabs();
      renderTabsBar();
      renderExtensionsPanel();
      renderExtensionDetailMain(findExtensionById(found.extensionId));
      return;
    }

    const path = found.path;
    if (path === state.activeTabPath && state.activeEditorTabId === nextId) return;
    const current = activeTab();
    if (current && isDocumentTab(current) && isDirty()) {
      const proceed = await showModalConfirm({
        title: "Unsaved changes",
        body: `“${escapeHtml(current.title)}” has unsaved changes. Switch anyway? Your unsaved edits in the editor may be replaced.`,
        confirm: "Switch tab",
        danger: true,
      });
      if (!proceed) return;
    }
    state.activeTabPath = path;
    state.activeEditorTabId = nextId;
    setExtensionEditorActive(false);
    persistTabs();
    await openNotePath(path);
  }

  async function closeTab(tabRef) {
    const foundIndex = state.tabs.findIndex((t) => editorTabId(t) === tabRef) >= 0
      ? state.tabs.findIndex((t) => editorTabId(t) === tabRef)
      : state.tabs.findIndex((t) => isDocumentTab(t) && normalizeExplorerPath(t.path) === normalizeExplorerPath(tabRef));
    const idx = foundIndex;
    if (idx < 0) return;

    const tab = state.tabs[idx];
    const id = editorTabId(tab);
    if (isDocumentTab(tab) && tab.dirty) {
      const proceed = await showModalConfirm({
        title: "Close tab",
        body: `“${escapeHtml(tab.title)}” has unsaved changes. Close without saving?`,
        confirm: "Close tab",
        danger: true,
      });
      if (!proceed) return;
    }

    state.tabs.splice(idx, 1);
    if (isDocumentTab(tab)) {
      state.closedTabs.unshift({
        path: tab.path,
        title: tab.title || baseName(tab.path),
      });
      state.closedTabs = state.closedTabs.slice(0, MAX_CLOSED_TABS);
    }

    if (state.activeEditorTabId === id || (!state.activeEditorTabId && isDocumentTab(tab) && state.activeTabPath === tab.path)) {
      const next = state.tabs[idx] || state.tabs[idx - 1] || null;
      if (next) {
        await switchTab(editorTabId(next));
      } else {
        state.activeEditorTabId = null;
        state.activeTabPath = null;
        clearPersistedTabs();
        setExtensionEditorActive(false);
        clearEditorNoOpenNote();
        return;
      }
    } else {
      persistTabs();
    }

    renderOpenTabs();
    renderTabsBar();
  }

  // Reorder tabs without touching disk. position is "before" | "after".
  async function closeActiveTab() {
    const tab = activeTab();
    if (tab) await closeTab(editorTabId(tab));
  }

  async function closeOtherTabs(tabRef) {
    const keepTab = state.tabs.find((tab) => editorTabId(tab) === tabRef) ||
      state.tabs.find((tab) => isDocumentTab(tab) && normalizeExplorerPath(tab.path) === normalizeExplorerPath(tabRef));
    if (!keepTab) return;
    const keep = editorTabId(keepTab);
    for (const tab of [...state.tabs]) {
      if (editorTabId(tab) !== keep) await closeTab(editorTabId(tab));
    }
  }

  async function closeTabsToRight(tabRef) {
    const index = state.tabs.findIndex((tab) => editorTabId(tab) === tabRef) >= 0
      ? state.tabs.findIndex((tab) => editorTabId(tab) === tabRef)
      : state.tabs.findIndex((tab) => isDocumentTab(tab) && normalizeExplorerPath(tab.path) === normalizeExplorerPath(tabRef));
    if (index < 0) return;
    for (const tab of [...state.tabs.slice(index + 1)])
      await closeTab(editorTabId(tab));
  }

  async function closeAllTabs() {
    for (const tab of [...state.tabs]) await closeTab(editorTabId(tab));
  }

  async function reopenClosedTab() {
    const tab = state.closedTabs.shift();
    if (!tab)
      return showNotification({
        type: "info",
        message: "No closed editor to reopen.",
      });
    await openNotePath(tab.path, { silent: true });
  }

  async function activateRelativeTab(delta) {
    if (!state.tabs.length) return;
    const activeId = state.activeEditorTabId || "";
    const current = state.tabs.findIndex((tab) => editorTabId(tab) === activeId);
    const next = (current + delta + state.tabs.length) % state.tabs.length;
    await switchTab(editorTabId(state.tabs[next]));
  }

  async function activateTabByIndex(index) {
    const tab = state.tabs[index];
    if (tab) await switchTab(editorTabId(tab));
  }

  function reorderTabs(sourceRef, targetRef, position) {
    if (sourceRef === targetRef) return;

    const fromIndex = state.tabs.findIndex((t) => editorTabId(t) === sourceRef);
    if (fromIndex < 0) return;

    const [moved] = state.tabs.splice(fromIndex, 1);

    let targetIndex = state.tabs.findIndex((t) => editorTabId(t) === targetRef);
    if (targetIndex < 0) {
      state.tabs.splice(fromIndex, 0, moved);
      return;
    }

    const insertAt = position === "after" ? targetIndex + 1 : targetIndex;
    state.tabs.splice(insertAt, 0, moved);

    persistTabs();
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
      bar.setAttribute("role", "tablist");
      bar.innerHTML = `<div class="vn-TabsBar__empty">No open editors</div>`;
      return;
    }
    bar.setAttribute("role", "tablist");
    const activeId = state.activeEditorTabId || "";
    bar.innerHTML = state.tabs
      .map((t) => {
        const id = editorTabId(t);
        const active = id === activeId ? " is-active" : "";
        const preview = t.preview ? " is-preview" : "";
        if (isExtensionTab(t)) {
          const ext = findExtensionById(t.extensionId) || t;
          const label = t.title || t.extensionId.split("/").pop() || t.extensionId;
          return `<div class="vn-Tab vn-Tab--extension${active}" role="tab" aria-selected="${id === activeId ? "true" : "false"}" aria-label="Extension ${escapeHtml(label)}" data-tab-id="${escapeHtml(id)}" data-tab-kind="extension" title="${escapeHtml(t.extensionId)}" draggable="true" tabindex="0">
              <span class="vn-Tab__extensionIcon vn-ExtensionIcon" aria-hidden="true">${extensionIconHtml(ext, label)}</span>
              <span class="vn-Tab__label">${escapeHtml(label)}</span>
              <button class="vn-Tab__close" type="button" data-tab-close="${escapeHtml(id)}" aria-label="Close tab">×</button>
            </div>`;
        }
        return `<div class="vn-Tab${active}${preview}${t.dirty ? " is-dirty" : ""}" role="tab" aria-selected="${id === activeId ? "true" : "false"}" aria-label="${escapeHtml(t.title)}${t.dirty ? " unsaved" : ""}" data-tab-id="${escapeHtml(id)}" data-tab-kind="document" data-tab-path="${escapeHtml(t.path)}" title="${escapeHtml(t.path)}" draggable="true" tabindex="0">
            ${tabDot(t)}
            <span class="vn-Tab__label">${escapeHtml(t.title)}${t.preview ? " ◦" : ""}</span>
            <button class="vn-Tab__close" type="button" data-tab-close="${escapeHtml(id)}" aria-label="Close tab">×</button>
          </div>`;
      })
      .join("");
  }

  /* ==========================================================
   * Diagnostics / Problems
   * ======================================================== */
  const DIAGNOSTIC_SEVERITY = {
    compiler_error: "error",
    runtime_error: "error",
    error: "error",
    stderr: "error",
    warning: "warning",
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

  function setRunAllDiagnostics(runResult) {
    const results = Array.isArray(runResult?.results) ? runResult.results : [];
    const executable = cells().filter(
      (c) => executionAvailabilityForCell(c).executable,
    );
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

    const filterText = String(state.diagnostics.filter || "")
      .trim()
      .toLowerCase();
    const severityFilter = state.diagnostics.severity || "all";
    const visibleItems = state.diagnostics.items.filter((d) => {
      if (severityFilter !== "all" && d.severity !== severityFilter)
        return false;
      if (!filterText) return true;
      return `${d.cellLabel || ""} ${d.kind || ""} ${d.message || ""}`
        .toLowerCase()
        .includes(filterText);
    });

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

    if (!visibleItems.length) {
      listEl.innerHTML = `
        <p class="vn-Tree__empty">
          No problems match the current filter.
        </p>`;
      return;
    }

    const groups = new Map();
    for (const d of visibleItems) {
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

  function refreshCellProblemBadges() {
    for (const el of $all(".vn-Cell")) {
      const id = el.dataset.cellId;
      const hasError = state.diagnostics.byCell.get(id) > 0;
      el.classList.toggle("has-problem", !!hasError);
    }
  }

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

  function normalizeActivityPanel(panel) {
    return ["explorer", "problems", "extensions"].includes(panel)
      ? panel
      : "explorer";
  }

  function setPanel(panel) {
    state.activePanel = normalizeActivityPanel(panel);
    state.bottomPanelVisible = state.activePanel === "problems";
    state.sidebarCollapsed = false;
    for (const p of $all("[data-panel]")) {
      p.hidden = p.dataset.panel !== state.activePanel;
    }
    app.classList.remove("is-sidebar-collapsed");
    renderActivityBar();
    persistUiState();
  }

  function syncActivityPanelState(options = {}) {
    state.activePanel = normalizeActivityPanel(state.activePanel);
    for (const p of $all("[data-panel]")) {
      p.hidden = p.dataset.panel !== state.activePanel;
    }
    if (state.sidebarCollapsed) {
      state.bottomPanelVisible = false;
      app.classList.add("is-sidebar-collapsed");
    } else {
      state.bottomPanelVisible = state.activePanel === "problems";
      app.classList.remove("is-sidebar-collapsed");
    }
    renderActivityBar();
    if (options.persist !== false) persistUiState();
  }

  function closeSidebarPanel() {
    state.sidebarCollapsed = true;
    if (state.activePanel === "problems") state.bottomPanelVisible = false;
    app.classList.add("is-sidebar-collapsed");
    renderActivityBar();
    persistUiState();
  }

  function toggleActivityPanel(panel) {
    const normalized = normalizeActivityPanel(panel);
    if (state.sidebarCollapsed) {
      setPanel(normalized);
      return;
    }
    if (state.activePanel === normalized) {
      closeSidebarPanel();
      return;
    }
    setPanel(normalized);
  }

  function revealActiveFile() {
    const path = normalizeExplorerPath(
      currentDocPath() || state.activeTabPath || "",
    );
    if (!path) return;
    state.explorer.selectedDirPath = parentPath(path);
    state.explorer.currentPath = path;
    loadExplorerForDocumentPath(path).then(() => {
      const row = document.querySelector(
        `[data-tree-path="${cssEscape(path)}"]`,
      );
      if (row instanceof HTMLElement) {
        row.focus({ preventScroll: false });
        row.scrollIntoView({ block: "nearest" });
      }
      setPanel("explorer");
    });
  }

  async function deleteExplorerSelection(
    path = state.explorer.currentPath,
    type = "file",
  ) {
    const normalized = normalizeExplorerPath(path);
    if (!normalized || normalized === ".") return;
    if (type === "dir") {
      const ok = await showModalConfirm({
        title: "Delete folder",
        body: `Delete folder “${escapeHtml(baseName(normalized))}” and everything inside it from disk? This cannot be undone from Vix Note.`,
        confirm: "Delete",
        danger: true,
      });
      if (ok) await deletePath(normalized, { recursive: true });
      return;
    }
    if (await confirmDelete(baseName(normalized), "file"))
      await deletePath(normalized);
  }

  function resetLayout() {
    state.sidebarCollapsed = false;
    state.sidebarWidth = DEFAULT_SIDEBAR_WIDTH;
    state.bottomPanelHeight = 220;
    state.bottomPanelVisible = false;
    state.focusMode = false;
    applySidebarWidth(DEFAULT_SIDEBAR_WIDTH);
    app.classList.remove("is-sidebar-collapsed", "is-focus");
    setPanel("explorer");
    persistUiState();
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
    persistUiState();
  }
  function toggleExplorerSidebar() {
    toggleActivityPanel("explorer");
  }
  function toggleSidebar(forceCollapsed) {
    if (forceCollapsed === true) {
      closeSidebarPanel();
      return;
    }
    toggleActivityPanel(state.activePanel || "explorer");
  }

  function toggleFocus() {
    state.focusMode = !state.focusMode;
    app.classList.toggle("is-focus", state.focusMode);
    persistUiState();
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
    closeCellKindMenus();
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
  const commands = new Map();
  const legacyCommandAliases = new Map();

  function currentToolbarKind() {
    const button = $(sel.toolbarKind);
    const value = button ? button.dataset.kind : "cpp";
    const kind = normalizeKind(value);
    return cellTypeDescriptor(kind) ? kind : "cpp";
  }

  function targetId() {
    return state.selectedId;
  }

  function registerCommand(id, handler, options = {}) {
    commands.set(id, {
      id,
      handler,
      label: options.label || id,
      category: options.category || "",
      keybinding: options.keybinding || "",
      when: options.when || null,
      aliases: options.aliases || [],
    });
    for (const alias of options.aliases || [])
      legacyCommandAliases.set(alias, id);
  }

  async function executeCommand(id, context = {}) {
    const resolved = legacyCommandAliases.get(id) || id;
    const command = commands.get(resolved);
    if (!command) throw new Error(`Unknown command: ${id}`);
    if (command.when && !command.when(state, context)) return undefined;
    try {
      return await command.handler(context);
    } catch (error) {
      reportError(error, { label: command.label, command: resolved });
      return undefined;
    }
  }

  function runCommand(name) {
    executeCommand(name);
  }

  function registerCoreCommands() {
    if (commands.size) return;
    const hasCell = () => !!targetId();
    registerCommand(
      "note.new",
      ({ path } = {}) => newNote(path || state.explorer.selectedDirPath || "."),
      {
        label: "New Note",
        category: "File",
        keybinding: "Ctrl+N",
        aliases: ["new-note"],
      },
    );
    registerCommand("note.open", () => openNote(), {
      label: "Open Note",
      category: "File",
      keybinding: "Ctrl+O",
      aliases: ["open-note"],
    });
    registerCommand("note.save", () => saveNote(), {
      label: "Save Note",
      category: "File",
      keybinding: "Ctrl+S",
      aliases: ["save"],
    });
    registerCommand("note.saveAll", () => saveNote(), {
      label: "Save All",
      category: "File",
    });
    registerCommand("note.reload", () => loadDocument(), {
      label: "Reload from Disk",
      category: "File",
      aliases: ["reload"],
    });
    registerCommand(
      "note.close",
      ({ path } = {}) => (path ? closeTab(path) : closeActiveTab()),
      { label: "Close Editor", category: "File", keybinding: "Ctrl+W" },
    );
    registerCommand("note.closeAll", () => closeAllTabs(), {
      label: "Close All Editors",
      category: "File",
    });
    registerCommand(
      "note.closeOthers",
      ({ path } = {}) => closeOtherTabs(path || activeTab()?.path),
      { label: "Close Other Editors", category: "File" },
    );
    registerCommand("note.reopenClosed", () => reopenClosedTab(), {
      label: "Reopen Closed Editor",
      category: "File",
      keybinding: "Ctrl+Shift+T",
    });
    registerCommand("note.toggleAutoSave", () => toggleAutoSave(), {
      label: "Toggle Auto Save",
      category: "File",
    });

    registerCommand(
      "cell.insertAbove",
      () => targetId() && insertAbove(targetId()),
      {
        label: "Insert Cell Above",
        category: "Cell",
        aliases: ["insert-above"],
      },
    );
    registerCommand(
      "cell.insertBelow",
      () => addCell(targetId() ? cellTypeForInsertionAfter(targetId()) : currentToolbarKind(), { afterId: targetId() }),
      {
        label: "Insert Cell Below",
        category: "Cell",
        keybinding: "B",
        aliases: ["insert-below"],
      },
    );
    registerCommand(
      "cell.delete",
      () => targetId() && deleteCellById(targetId()),
      {
        label: "Delete Cell",
        category: "Cell",
        keybinding: "D D",
        aliases: ["cut-cell"],
      },
    );
    registerCommand(
      "cell.duplicate",
      () => targetId() && duplicateCell(targetId()),
      { label: "Duplicate Cell", category: "Cell", aliases: ["duplicate"] },
    );
    registerCommand(
      "cell.moveUp",
      () => targetId() && moveCellById(targetId(), "up"),
      { label: "Move Cell Up", category: "Cell", aliases: ["move-up"] },
    );
    registerCommand(
      "cell.moveDown",
      () => targetId() && moveCellById(targetId(), "down"),
      { label: "Move Cell Down", category: "Cell", aliases: ["move-down"] },
    );
    registerCommand("cell.run", () => targetId() && runCellById(targetId()), {
      label: "Run Selected Cell",
      category: "Cell",
      keybinding: "Ctrl+Enter",
      when: hasCell,
      aliases: ["run-cell"],
    });
    registerCommand(
      "cell.runAndAdvance",
      async () => {
        if (targetId()) {
          await runCellById(targetId());
          selectAdjacent(1);
        }
      },
      {
        label: "Run Cell and Select Next",
        category: "Cell",
        keybinding: "Shift+Enter",
        aliases: ["run-advance"],
      },
    );
    registerCommand(
      "cell.runAndInsertBelow",
      async () => {
        const id = targetId();
        if (!id) return;
        await runCellById(id);
        await addCell(cellTypeForInsertionAfter(id), { afterId: id });
      },
      {
        label: "Run Cell and Insert Below",
        category: "Cell",
        keybinding: "Alt+Enter",
      },
    );
    registerCommand("cell.runAll", () => runAll(), {
      label: "Run All Cells",
      category: "Cell",
      keybinding: "Ctrl+Shift+Enter",
      aliases: ["run-all"],
    });
    registerCommand("cell.changeType", () => openCellTypePicker(), {
      label: "Change Cell Type",
      category: "Cell",
    });
    registerCommand("cell.focusEditor", () => enterEditMode(), {
      label: "Focus Cell Editor",
      category: "Cell",
    });
    registerCommand("cell.copy", () => copyCell("copy"), {
      label: "Copy Cell",
      category: "Edit",
      aliases: ["copy-cell"],
    });
    registerCommand("cell.cut", () => copyCell("cut"), {
      label: "Cut Cell",
      category: "Edit",
    });
    registerCommand("cell.pasteBelow", () => pasteCell("below"), {
      label: "Paste Cell Below",
      category: "Edit",
    });
    registerCommand("cell.pasteAbove", () => pasteCell("above"), {
      label: "Paste Cell Above",
      category: "Edit",
    });
    registerCommand(
      "cell.clearOutput",
      () => targetId() && clearCellOutput(targetId()),
      {
        label: "Clear Selected Output",
        category: "Cell",
        aliases: ["clear-cell"],
      },
    );
    registerCommand("cell.clearAllOutputs", () => clearAllOutputs(), {
      label: "Clear All Outputs",
      category: "Cell",
      aliases: ["clear-all"],
    });
    registerCommand(
      "cell.toCpp",
      () => targetId() && changeKind(targetId(), "cpp"),
      {
        label: "Change Cell to C++",
        category: "Cell",
        keybinding: "Y",
        aliases: ["to-cpp"],
      },
    );
    registerCommand(
      "cell.toMarkdown",
      () => targetId() && changeKind(targetId(), "markdown"),
      {
        label: "Change Cell to Markdown",
        category: "Cell",
        keybinding: "M",
        aliases: ["to-markdown"],
      },
    );
    registerCommand(
      "cell.toReply",
      () => targetId() && changeKind(targetId(), "reply"),
      {
        label: "Change Cell to Reply",
        category: "Cell",
        keybinding: "R",
        aliases: ["to-reply"],
      },
    );
    registerCommand(
      "cell.toHtml",
      () => targetId() && changeKind(targetId(), "html"),
      {
        label: "Change Cell to HTML",
        category: "Cell",
        keybinding: "H",
        aliases: ["to-html"],
      },
    );

    registerCommand(
      "explorer.refresh",
      ({ path } = {}) =>
        refreshExplorer(path || state.explorer.currentPath || "."),
      { label: "Refresh Explorer", category: "Explorer", aliases: ["refresh"] },
    );
    registerCommand(
      "explorer.newFile",
      ({ path } = {}) => newNote(path || state.explorer.selectedDirPath || "."),
      { label: "New Note", category: "Explorer" },
    );
    registerCommand(
      "explorer.newFolder",
      ({ path } = {}) =>
        newFolder(path || state.explorer.selectedDirPath || "."),
      { label: "New Folder", category: "Explorer", aliases: ["new-folder"] },
    );
    registerCommand(
      "explorer.rename",
      ({ path, type } = {}) =>
        startInlineRename(
          path || state.explorer.currentPath || ".",
          type || "file",
        ),
      { label: "Rename", category: "Explorer" },
    );
    registerCommand(
      "explorer.delete",
      ({ path, type } = {}) => deleteExplorerSelection(path, type),
      { label: "Delete", category: "Explorer" },
    );
    registerCommand(
      "explorer.open",
      ({ path } = {}) => path && openFileRowIfAllowed(path),
      { label: "Open", category: "Explorer" },
    );
    registerCommand("explorer.revealActiveFile", () => revealActiveFile(), {
      label: "Reveal Active File in Explorer",
      category: "Explorer",
    });

    registerCommand("view.toggleSidebar", () => toggleExplorerSidebar(), {
      label: "Toggle Sidebar",
      category: "View",
      keybinding: "Ctrl+B",
      aliases: ["toggle-sidebar"],
    });
    registerCommand("view.showExplorer", () => setPanel("explorer"), {
      label: "Explorer",
      category: "View",
      aliases: ["show-explorer"],
    });
    registerCommand("view.showProblems", () => setPanel("problems"), {
      label: "Problems",
      category: "View",
      keybinding: "Ctrl+J",
      aliases: ["show-problems"],
    });
    registerCommand("view.showExtensions", () => setPanel("extensions"), {
      label: "Extensions",
      category: "View",
      keybinding: "Ctrl/Cmd+Shift+X",
      aliases: ["show-extensions"],
    });
    registerCommand("extensions.show", () => setPanel("extensions"), {
      label: "Show Extensions",
      category: "Extensions",
      keybinding: "Ctrl/Cmd+Shift+X",
    });
    registerCommand("extensions.refresh", () => refreshExtensionsView(), {
      label: "Refresh",
      category: "Extensions",
    });
    registerCommand("extensions.reload", () => refreshExtensionsView(), {
      label: "Reload Extensions",
      category: "Extensions",
    });
    registerCommand(
      "extensions.search",
      () => {
        setPanel("extensions");
        $(sel.extensionsSearch)?.focus();
      },
      {
        label: "Search Marketplace",
        category: "Extensions",
      },
    );
    registerCommand(
      "extensions.install",
      ({ packageId } = {}) => extensionAction("install", packageId),
      {
        label: "Install Extension",
        category: "Extensions",
      },
    );
    registerCommand(
      "extensions.uninstall",
      ({ packageId } = {}) => extensionAction("uninstall", packageId),
      {
        label: "Uninstall Extension",
        category: "Extensions",
      },
    );
    registerCommand(
      "extensions.enable",
      ({ packageId } = {}) => extensionAction("enable", packageId),
      {
        label: "Enable Extension",
        category: "Extensions",
      },
    );
    registerCommand(
      "extensions.disable",
      ({ packageId } = {}) => extensionAction("disable", packageId),
      {
        label: "Disable Extension",
        category: "Extensions",
      },
    );
    registerCommand(
      "extensions.showDetails",
      ({ packageId } = {}) => {
        if (packageId) state.extensionWorkbench.selectedId = packageId;
        setPanel("extensions");
        renderExtensionsPanel();
      },
      {
        label: "Show Extension Details",
        category: "Extensions",
      },
    );
    registerCommand("view.toggleFocusMode", () => toggleFocus(), {
      label: "Toggle Focus Mode",
      category: "View",
      aliases: ["toggle-focus"],
    });
    registerCommand("view.commandPalette", () => openCommandPalette(), {
      label: "Command Palette",
      category: "View",
      keybinding: "Ctrl+Shift+P",
    });
    registerCommand("view.quickOpen", () => openQuickOpen(), {
      label: "Quick Open",
      category: "View",
      keybinding: "Ctrl+P",
      aliases: ["workbench.action.quickOpen"],
    });
    registerCommand("view.find", () => openFindBox(), {
      label: "Find in Document",
      category: "Edit",
      keybinding: "Ctrl+F",
    });
    registerCommand("view.resetLayout", () => resetLayout(), {
      label: "Reset Layout",
      category: "View",
    });
    registerCommand("preferences.colorTheme", () => openColorThemePicker(), {
      label: "Color Theme",
      category: "Preferences",
      aliases: ["theme", "appearance"],
    });
    registerCommand(
      "developer.reloadInterface",
      () => window.location.reload(),
      { label: "Reload Interface", category: "Developer" },
    );
    registerCommand("kernel.restart", () => restartKernel(false), {
      label: "Restart Kernel",
      category: "Developer",
      aliases: ["restart"],
    });
    registerCommand("kernel.restartAndRun", () => restartKernel(true), {
      label: "Restart Kernel and Run All",
      category: "Developer",
      aliases: ["restart-run"],
    });
    registerCommand("help.shortcuts", () => showShortcuts(), {
      label: "Keyboard Shortcuts",
      category: "Help",
      keybinding: "?",
      aliases: ["shortcuts"],
    });
    registerCommand("help.about", () => showAbout(), {
      label: "About Vix Note",
      category: "Help",
      aliases: ["about"],
    });
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
   * Command palette, Quick Open, Find
   * ======================================================== */
  function fuzzyScore(query, text) {
    const q = String(query || "")
      .trim()
      .toLowerCase();
    const t = String(text || "").toLowerCase();
    if (!q) return 1;
    const words = q.split(/\s+/).filter(Boolean);
    let score = 0;
    for (const word of words) {
      const idx = t.indexOf(word);
      if (idx < 0) return 0;
      score += idx === 0 ? 5 : 2;
    }
    return score;
  }

  function availableCommands() {
    return Array.from(commands.values())
      .filter((cmd) => !cmd.when || cmd.when(state, {}))
      .sort((a, b) =>
        `${a.category}:${a.label}`.localeCompare(`${b.category}:${b.label}`),
      );
  }

  function commandMatches(query) {
    return availableCommands()
      .map((cmd) => ({
        cmd,
        score: fuzzyScore(query, `${cmd.category} ${cmd.label} ${cmd.id}`),
      }))
      .filter((item) => item.score > 0)
      .sort(
        (a, b) => b.score - a.score || a.cmd.label.localeCompare(b.cmd.label),
      );
  }

  function ensurePickerRoot(kind) {
    let root = document.querySelector(`[data-${kind}]`);
    if (!root) {
      root = document.createElement("div");
      root.className =
        kind === "command-palette"
          ? "vn-CommandPalette"
          : kind === "quick-open"
            ? "vn-QuickOpen"
            : "vn-FindBox";
      root.setAttribute(`data-${kind}`, "");
      document.body.appendChild(root);
    }
    return root;
  }

  function closePicker(kind) {
    const root = document.querySelector(`[data-${kind}]`);
    if (root) root.remove();
    if (kind === "command-palette") state.commandPalette.open = false;
    if (kind === "quick-open") state.quickOpen.open = false;
    if (kind === "find-box") state.find.open = false;
    restoreFocus();
  }

  function renderCommandPalette() {
    const root = ensurePickerRoot("command-palette");
    const matches = commandMatches(state.commandPalette.query);
    state.commandPalette.selected = Math.min(
      state.commandPalette.selected,
      Math.max(0, matches.length - 1),
    );
    root.innerHTML = `
      <div class="vn-PickerOverlay" data-picker-close></div>
      <section class="vn-Picker" role="dialog" aria-modal="true" aria-label="Command Palette">
        <input class="vn-Picker__input" data-command-palette-input aria-label="Search commands" placeholder="Type a command…" value="${escapeHtml(state.commandPalette.query)}" />
        <div class="vn-Picker__list" role="listbox">
          ${
            matches.length
              ? matches
                  .map(
                    (item, index) => `
            <button type="button" class="vn-Picker__item${index === state.commandPalette.selected ? " is-selected" : ""}" role="option" aria-selected="${index === state.commandPalette.selected ? "true" : "false"}" data-palette-command="${escapeHtml(item.cmd.id)}" data-index="${index}">
              <span class="vn-Picker__label"><small>${escapeHtml(item.cmd.category || "Command")}</small>${escapeHtml(item.cmd.label)}</span>
              <code>${escapeHtml(item.cmd.keybinding || "")}</code>
            </button>`,
                  )
                  .join("")
              : `<p class="vn-Picker__empty">No commands match.</p>`
          }
        </div>
      </section>`;
    const input = root.querySelector("[data-command-palette-input]");
    input?.focus({ preventScroll: true });
    input?.setSelectionRange(input.value.length, input.value.length);
  }

  function openCommandPalette() {
    rememberFocus("commandPalette");
    closeContextMenu();
    state.commandPalette.open = true;
    state.commandPalette.query = "";
    state.commandPalette.selected = 0;
    renderCommandPalette();
  }

  function quickOpenFiles() {
    const recent = new Map(
      state.tabs.map((tab, i) => [
        normalizeExplorerPath(tab.path),
        {
          path: normalizeExplorerPath(tab.path),
          title: tab.title || baseName(tab.path),
          recentRank: i,
        },
      ]),
    );
    for (const entry of state.explorer.entries.values()) {
      if (
        entry.type === "file" &&
        (entry.openable || String(entry.path || "").endsWith(".vixnote"))
      ) {
        const path = normalizeExplorerPath(entry.path);
        if (!recent.has(path))
          recent.set(path, { path, title: baseName(path), recentRank: 9999 });
      }
    }
    return Array.from(recent.values());
  }

  function quickOpenMatches(query) {
    return quickOpenFiles()
      .map((file) => ({
        file,
        score: fuzzyScore(query, `${file.title} ${file.path}`),
      }))
      .filter((item) => !query || item.score > 0)
      .sort((a, b) =>
        query ? b.score - a.score : a.file.recentRank - b.file.recentRank,
      );
  }

  function renderQuickOpen() {
    const root = ensurePickerRoot("quick-open");
    const matches = quickOpenMatches(state.quickOpen.query);
    state.quickOpen.selected = Math.min(
      state.quickOpen.selected,
      Math.max(0, matches.length - 1),
    );
    root.innerHTML = `
      <div class="vn-PickerOverlay" data-picker-close></div>
      <section class="vn-Picker" role="dialog" aria-modal="true" aria-label="Quick Open">
        <input class="vn-Picker__input" data-quick-open-input aria-label="Search files" placeholder="Go to file…" value="${escapeHtml(state.quickOpen.query)}" />
        <div class="vn-Picker__list" role="listbox">
          ${
            matches.length
              ? matches
                  .map(
                    (item, index) => `
            <button type="button" class="vn-Picker__item${index === state.quickOpen.selected ? " is-selected" : ""}" role="option" aria-selected="${index === state.quickOpen.selected ? "true" : "false"}" data-quick-open-path="${escapeHtml(item.file.path)}" data-index="${index}">
              <span class="vn-Picker__label"><small>${escapeHtml(item.file.path)}</small>${escapeHtml(item.file.title || baseName(item.file.path))}</span>
            </button>`,
                  )
                  .join("")
              : `<p class="vn-Picker__empty">No known .vixnote files.</p>`
          }
        </div>
      </section>`;
    const input = root.querySelector("[data-quick-open-input]");
    input?.focus({ preventScroll: true });
    input?.setSelectionRange(input.value.length, input.value.length);
  }

  function openQuickOpen() {
    rememberFocus("quickOpen");
    closeContextMenu();
    state.quickOpen.open = true;
    state.quickOpen.query = "";
    state.quickOpen.selected = 0;
    renderQuickOpen();
  }

  function currentPickerMatches(kind) {
    return kind === "command-palette"
      ? commandMatches(state.commandPalette.query)
      : quickOpenMatches(state.quickOpen.query);
  }

  async function activatePickerSelection(kind) {
    if (kind === "command-palette") {
      const item = currentPickerMatches(kind)[state.commandPalette.selected];
      if (!item) return;
      closePicker(kind);
      await executeCommand(item.cmd.id);
      return;
    }
    const item = currentPickerMatches(kind)[state.quickOpen.selected];
    if (!item) return;
    closePicker(kind);
    await openNotePath(item.file.path, { silent: true });
  }

  function bindPickers() {
    document.addEventListener("input", (event) => {
      const input = event.target;
      if (!(input instanceof HTMLInputElement)) return;
      if (input.matches("[data-command-palette-input]")) {
        state.commandPalette.query = input.value;
        state.commandPalette.selected = 0;
        renderCommandPalette();
      } else if (input.matches("[data-quick-open-input]")) {
        state.quickOpen.query = input.value;
        state.quickOpen.selected = 0;
        renderQuickOpen();
      } else if (input.matches("[data-cell-type-filter]")) {
        renderCellTypePicker(input.value);
      } else if (input.matches("[data-find-input]")) {
        state.find.query = input.value;
        updateFindMatches();
        renderFindBox();
      }
    });

    document.addEventListener("click", async (event) => {
      const target = event.target instanceof Element ? event.target : null;
      if (!target) return;
      if (target.closest("[data-picker-close]")) {
        closePicker("command-palette");
        closePicker("quick-open");
        closePicker("find-box");
        return;
      }
      const cmd = target.closest("[data-palette-command]");
      if (cmd) {
        const id = cmd.getAttribute("data-palette-command");
        closePicker("command-palette");
        await executeCommand(id);
      }
      const file = target.closest("[data-quick-open-path]");
      if (file) {
        const path = file.getAttribute("data-quick-open-path");
        closePicker("quick-open");
        await openNotePath(path, { silent: true });
      }
      const changeCellType = target.closest("[data-change-cell-type]");
      if (changeCellType) {
        const type = changeCellType.getAttribute("data-change-cell-type");
        closePicker("command-palette");
        if (targetId() && type) await changeKind(targetId(), type);
      }
      const findAction = target.closest("[data-find-action]");
      if (findAction) {
        const action = findAction.getAttribute("data-find-action");
        if (action === "next") gotoFindMatch(1);
        if (action === "prev") gotoFindMatch(-1);
        if (action === "case") {
          state.find.caseSensitive = !state.find.caseSensitive;
          updateFindMatches();
          renderFindBox();
        }
        if (action === "close") closePicker("find-box");
      }
    });

    document.addEventListener("keydown", async (event) => {
      const inCommand = !!document.querySelector("[data-command-palette]");
      const inQuick = !!document.querySelector("[data-quick-open]");
      if (!inCommand && !inQuick && !state.find.open) return;
      const kind = inCommand
        ? "command-palette"
        : inQuick
          ? "quick-open"
          : "find-box";
      if (event.key === "Escape") {
        event.preventDefault();
        closePicker(kind);
        return;
      }
      if (kind === "find-box") {
        if (event.key === "Enter") {
          event.preventDefault();
          gotoFindMatch(event.shiftKey ? -1 : 1);
        }
        return;
      }
      const selected =
        kind === "command-palette" ? state.commandPalette : state.quickOpen;
      if (event.key === "ArrowDown") {
        event.preventDefault();
        selected.selected = Math.min(
          selected.selected + 1,
          Math.max(0, currentPickerMatches(kind).length - 1),
        );
        kind === "command-palette" ? renderCommandPalette() : renderQuickOpen();
      } else if (event.key === "ArrowUp") {
        event.preventDefault();
        selected.selected = Math.max(0, selected.selected - 1);
        kind === "command-palette" ? renderCommandPalette() : renderQuickOpen();
      } else if (event.key === "Enter") {
        event.preventDefault();
        await activatePickerSelection(kind);
      }
    });
  }

  function updateFindMatches() {
    const query = state.find.query || "";
    state.find.matches = [];
    if (!query) {
      state.find.index = 0;
      return;
    }
    const needle = state.find.caseSensitive ? query : query.toLowerCase();
    for (const cell of cells()) {
      const source = String(cell.source || "");
      const haystack = state.find.caseSensitive ? source : source.toLowerCase();
      let at = haystack.indexOf(needle);
      while (at >= 0) {
        state.find.matches.push({
          cellId: cell.id,
          start: at,
          end: at + query.length,
        });
        at = haystack.indexOf(needle, at + Math.max(1, query.length));
      }
    }
    state.find.index = Math.min(
      state.find.index,
      Math.max(0, state.find.matches.length - 1),
    );
  }

  function renderFindBox() {
    const root = ensurePickerRoot("find-box");
    root.innerHTML = `
      <div class="vn-FindBox__panel" role="dialog" aria-modal="false" aria-label="Find in document">
        <input class="vn-FindBox__input" data-find-input aria-label="Find" placeholder="Find in cells" value="${escapeHtml(state.find.query)}" />
        <span class="vn-FindBox__count">${state.find.matches.length ? `${state.find.index + 1}/${state.find.matches.length}` : "0/0"}</span>
        <button type="button" data-find-action="prev" aria-label="Previous match">↑</button>
        <button type="button" data-find-action="next" aria-label="Next match">↓</button>
        <button type="button" data-find-action="case" class="${state.find.caseSensitive ? "is-active" : ""}" aria-label="Toggle case sensitive">Aa</button>
        <button type="button" data-find-action="close" aria-label="Close find">×</button>
      </div>`;
    root.querySelector("input")?.focus({ preventScroll: true });
  }

  function openFindBox() {
    rememberFocus("editor");
    state.find.open = true;
    state.find.query = "";
    state.find.index = 0;
    updateFindMatches();
    renderFindBox();
  }

  function gotoFindMatch(delta) {
    updateFindMatches();
    if (!state.find.matches.length) return;
    state.find.index =
      (state.find.index + delta + state.find.matches.length) %
      state.find.matches.length;
    const match = state.find.matches[state.find.index];
    selectCell(match.cellId, { edit: true, focus: true });
    requestAnimationFrame(() => {
      const cell = cellElById(match.cellId);
      const ta = cell
        ? cell.querySelector('textarea[data-action="edit-source"]')
        : null;
      if (ta) {
        ta.focus();
        ta.setSelectionRange(match.start, match.end);
        updateCursorStatus(ta);
      }
      renderFindBox();
    });
  }

  function renderCellTypePicker(query = "") {
    const current = targetId()
      ? cellTypeOf(findCell(targetId()))
      : currentToolbarKind();
    const q = String(query || "")
      .trim()
      .toLowerCase();
    const list = normalizedCellTypes()
      .map((type) => ({
        type,
        score: fuzzyScore(
          q,
          `${type.label} ${type.id} ${type.extension || ""} ${type.language || ""}`,
        ),
      }))
      .filter((item) => !q || item.score > 0)
      .sort(
        (a, b) => b.score - a.score || a.type.label.localeCompare(b.type.label),
      );
    const root = ensurePickerRoot("command-palette");
    root.innerHTML = `
      <div class="vn-PickerOverlay" data-picker-close></div>
      <section class="vn-Picker" role="dialog" aria-modal="true" aria-label="Change Cell Type">
        <input class="vn-Picker__input" data-cell-type-filter aria-label="Search cell types" placeholder="Change cell type…" value="${escapeHtml(query)}" />
        <div class="vn-Picker__list" role="listbox">
          ${
            list.length
              ? list
                  .map(
                    ({ type }) => `
            <button type="button" class="vn-Picker__item${type.id === current ? " is-selected" : ""}" data-change-cell-type="${escapeHtml(type.id)}" role="option" aria-selected="${type.id === current ? "true" : "false"}">
              <span class="vn-Picker__label"><small>${escapeHtml(type.id)}${type.extension ? ` · ${escapeHtml(type.extension)}` : ""}</small>${escapeHtml(type.label || type.id)}</span>
              <code>${type.executable ? "executable" : "text"}${type.builtin ? " · builtin" : ""}</code>
            </button>`,
                  )
                  .join("")
              : `<p class="vn-Picker__empty">No cell types match.</p>`
          }
        </div>
      </section>`;
    const input = root.querySelector("[data-cell-type-filter]");
    input?.focus({ preventScroll: true });
    input?.setSelectionRange(input.value.length, input.value.length);
  }

  function openCellTypePicker() {
    rememberFocus("commandPalette");
    state.commandPalette.open = true;
    renderCellTypePicker("");
  }

  /* ==========================================================
   * Context menu (shared)
   * ======================================================== */
  let contextMenuEl = null;
  let contextMenuItems = [];
  let contextMenuIndex = 0;

  function closeContextMenu() {
    if (contextMenuEl) {
      contextMenuEl.remove();
      contextMenuEl = null;
      contextMenuItems = [];
      restoreFocus();
    }
  }

  function enabledContextIndexes() {
    return contextMenuItems
      .map((item, index) => ({ item, index }))
      .filter(({ item }) => !item.separator && item.enabled !== false);
  }

  function renderContextSelection() {
    if (!contextMenuEl) return;
    for (const btn of contextMenuEl.querySelectorAll("[data-ctx-index]")) {
      btn.classList.toggle(
        "is-selected",
        Number(btn.getAttribute("data-ctx-index")) === contextMenuIndex,
      );
    }
  }

  function openContextMenu(items, position, context = {}) {
    closeContextMenu();
    rememberFocus("contextMenu");
    contextMenuItems = items.map((item) => ({
      ...item,
      context: { ...context, ...item },
    }));
    const menu = document.createElement("div");
    menu.className = "vn-Context";
    menu.setAttribute("role", "menu");
    menu.tabIndex = -1;
    menu.innerHTML = contextMenuItems
      .map((it, index) =>
        it.separator
          ? `<div class="vn-Context__sep" role="separator"></div>`
          : `<button type="button" role="menuitem" class="vn-Context__item${it.danger ? " is-danger" : ""}${it.enabled === false ? " is-disabled" : ""}" data-ctx-index="${index}" ${it.enabled === false ? "disabled" : ""}>
              <span>${escapeHtml(it.label)}</span><code>${escapeHtml(it.keybinding || "")}</code>
            </button>`,
      )
      .join("");
    document.body.appendChild(menu);
    contextMenuEl = menu;
    const first = enabledContextIndexes()[0];
    contextMenuIndex = first ? first.index : 0;
    const rect = menu.getBoundingClientRect();
    const x = Math.min(position.x, window.innerWidth - rect.width - 8);
    const y = Math.min(position.y, window.innerHeight - rect.height - 8);
    menu.style.left = `${Math.max(8, x)}px`;
    menu.style.top = `${Math.max(8, y)}px`;
    renderContextSelection();
    menu.focus({ preventScroll: true });
    menu.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-ctx-index]");
      if (!btn || btn.disabled) return;
      contextMenuIndex = Number(btn.getAttribute("data-ctx-index"));
      activateContextMenuItem();
    });
  }

  function showContextMenu(x, y, items, context = {}) {
    openContextMenu(items, { x, y }, context);
  }

  async function activateContextMenuItem() {
    const item = contextMenuItems[contextMenuIndex];
    if (!item || item.separator || item.enabled === false) return;
    closeContextMenu();
    if (item.command) await executeCommand(item.command, item.context || item);
    else if (item.run) await item.run();
  }

  function moveContextMenu(delta) {
    const enabled = enabledContextIndexes();
    if (!enabled.length) return;
    const current = enabled.findIndex(
      ({ index }) => index === contextMenuIndex,
    );
    const next = enabled[(current + delta + enabled.length) % enabled.length];
    contextMenuIndex = next.index;
    renderContextSelection();
  }

  function copyText(value) {
    const text = String(value || "");
    if (navigator.clipboard?.writeText)
      navigator.clipboard.writeText(text).catch(() => {});
    showNotification({ type: "info", message: "Path copied" });
  }

  function fileContextItems(path) {
    return [
      { label: "Open", command: "explorer.open", path },
      {
        label: "Open to the Side",
        command: "explorer.open",
        path,
        enabled: false,
      },
      { separator: true },
      {
        label: "Rename",
        command: "explorer.rename",
        path,
        type: "file",
        keybinding: "F2",
      },
      {
        label: "Delete",
        command: "explorer.delete",
        path,
        type: "file",
        danger: true,
        keybinding: "Del",
      },
      { separator: true },
      { label: "Copy Path", run: () => copyText(path) },
      { label: "Copy Relative Path", run: () => copyText(path) },
      { label: "Reveal in Explorer", command: "explorer.revealActiveFile" },
    ];
  }

  function dirContextItems(path) {
    return [
      { label: "New Note", command: "explorer.newFile", path },
      { label: "New Folder", command: "explorer.newFolder", path },
      { label: "Refresh", command: "explorer.refresh", path },
      { separator: true },
      {
        label: "Rename",
        command: "explorer.rename",
        path,
        type: "dir",
        enabled: path !== ".",
      },
      {
        label: "Delete",
        command: "explorer.delete",
        path,
        type: "dir",
        danger: true,
        enabled: path !== ".",
      },
      { separator: true },
      { label: "Copy Path", run: () => copyText(path) },
    ];
  }

  function emptyExplorerContextItems() {
    return [
      { label: "New Note", command: "explorer.newFile" },
      { label: "New Folder", command: "explorer.newFolder" },
      { label: "Refresh", command: "explorer.refresh" },
    ];
  }

  function tabContextItems(path) {
    return [
      { label: "Close", command: "note.close", path, keybinding: "Ctrl+W" },
      { label: "Close Others", run: () => closeOtherTabs(path) },
      { label: "Close to the Right", run: () => closeTabsToRight(path) },
      { label: "Close All", command: "note.closeAll" },
      { separator: true },
      { label: "Copy Path", run: () => copyText(path) },
      { label: "Reveal in Explorer", command: "explorer.revealActiveFile" },
    ];
  }

  function cellContextItems(id) {
    return [
      { label: "Run Cell", command: "cell.run", enabled: !!id },
      { label: "Run All Cells", command: "cell.runAll" },
      { separator: true },
      { label: "Insert Above", command: "cell.insertAbove" },
      { label: "Insert Below", command: "cell.insertBelow" },
      { label: "Change Cell Type", command: "cell.changeType" },
      { separator: true },
      { label: "Copy Cell", command: "cell.copy" },
      { label: "Cut Cell", command: "cell.cut" },
      {
        label: "Paste Above",
        command: "cell.pasteAbove",
        enabled: !!state.cellClipboard.cell,
      },
      {
        label: "Paste Below",
        command: "cell.pasteBelow",
        enabled: !!state.cellClipboard.cell,
      },
      { separator: true },
      { label: "Duplicate", command: "cell.duplicate" },
      { label: "Delete", command: "cell.delete", danger: true },
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
          applyToCell: false,
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

      const extensionGroupToggle = target
        ? target.closest("[data-extension-group-toggle]")
        : null;

      if (extensionGroupToggle) {
        event.preventDefault();

        toggleExtensionGroup(
          extensionGroupToggle.dataset.extensionGroupToggle || "",
        );

        return;
      }

      const extensionDetails = target
        ? target.closest("[data-extension-details]")
        : null;
      if (extensionDetails) {
        event.preventDefault();
        state.extensionWorkbench.selectedId =
          extensionDetails.dataset.extensionDetails || "";
        openExtensionTab(state.extensionWorkbench.selectedId);
        return;
      }

      const extensionActionButton = target
        ? target.closest("[data-extension-action]")
        : null;
      if (extensionActionButton) {
        event.preventDefault();
        event.stopPropagation();
        extensionAction(
          extensionActionButton.dataset.extensionAction || "",
          extensionActionButton.dataset.extensionId || "",
        );
        return;
      }

      const themeToggle = target ? target.closest("[data-theme-toggle]") : null;
      if (themeToggle) {
        event.preventDefault();
        event.stopPropagation();
        toggleThemeMenu();
        return;
      }

      const themeOption = target ? target.closest("[data-theme-option]") : null;
      if (themeOption) {
        event.preventDefault();
        setTheme(themeOption.dataset.themeOption || "system");
        closeThemeMenu();
        return;
      }

      if (target && !target.closest("[data-theme-menu-root]")) {
        closeThemeMenu();
      }

      const menuCommand = target ? target.closest("[data-command]") : null;
      if (menuCommand) {
        event.preventDefault();
        closeAllMenus();
        executeCommand(menuCommand.getAttribute("data-command"));
        return;
      }

      const t =
        event.target instanceof Element
          ? event.target.closest("[data-action]")
          : null;
      if (!t) return;
      const action = t.getAttribute("data-action");
      const actionCommand =
        legacyCommandAliases.get(action) ||
        {
          "toggle-sidebar": "view.toggleSidebar",
          save: "note.save",
          "run-cell": "cell.run",
          "run-all": "cell.runAll",
          restart: "kernel.restart",
          "insert-below": "cell.insertBelow",
          "cut-cell": "cell.delete",
          duplicate: "cell.duplicate",
          "move-up": "cell.moveUp",
          "move-down": "cell.moveDown",
          "new-note": "note.new",
          "open-note": "note.open",
          "new-folder": "explorer.newFolder",
          refresh: "explorer.refresh",
          shortcuts: "help.shortcuts",
        }[action];
      if (actionCommand) {
        event.preventDefault();
        executeCommand(actionCommand);
        return;
      }

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
          addCell(targetId() ? cellTypeForInsertionAfter(targetId()) : currentToolbarKind(), { afterId: targetId() });
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
        toggleActivityPanel(b.dataset.activity);
      });
    }
  }

  /* ==========================================================
   * Wiring: explorer + tabs bar
   * ======================================================== */
  function bindExplorer() {
    const listEl = $(sel.explorerList);
    if (listEl) {
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

        const row = e.target.closest("[data-tree-path]");

        if (!row) {
          state.explorer.selectedDirPath = ".";
          state.explorer.currentPath = ".";
          renderExplorer();
          return;
        }

        const path = normalizeExplorerPath(row.getAttribute("data-tree-path"));
        const type = row.getAttribute("data-tree-type");
        const rows = $all("[data-tree-path]", listEl).filter(
          (el) => el.offsetParent !== null,
        );
        const rowIndex = rows.indexOf(row);
        const focusRow = (next) => {
          if (!next) return;
          state.explorer.currentPath = normalizeExplorerPath(
            next.getAttribute("data-tree-path"),
          );
          const nextType = next.getAttribute("data-tree-type");
          state.explorer.selectedDirPath =
            nextType === "dir"
              ? state.explorer.currentPath
              : parentPath(state.explorer.currentPath);
          renderExplorer();
          requestAnimationFrame(() => {
            const focused = $(
              `[data-tree-path="${cssEscape(state.explorer.currentPath)}"]`,
              listEl,
            );
            focused?.focus({ preventScroll: true });
          });
        };

        if (e.key === "ArrowDown") {
          e.preventDefault();
          focusRow(rows[Math.min(rows.length - 1, rowIndex + 1)]);
          return;
        }
        if (e.key === "ArrowUp") {
          e.preventDefault();
          focusRow(rows[Math.max(0, rowIndex - 1)]);
          return;
        }
        if (e.key === "ArrowRight") {
          e.preventDefault();
          if (type === "dir") {
            if (!state.explorer.expandedDirs.has(path)) {
              toggleDirectory(path);
            } else {
              focusRow(rows[rowIndex + 1]);
            }
          }
          return;
        }
        if (e.key === "ArrowLeft") {
          e.preventDefault();
          if (
            type === "dir" &&
            state.explorer.expandedDirs.has(path) &&
            path !== "."
          ) {
            state.explorer.expandedDirs.delete(path);
            renderExplorer();
            requestAnimationFrame(() =>
              $(`[data-tree-path="${cssEscape(path)}"]`, listEl)?.focus({
                preventScroll: true,
              }),
            );
          } else {
            const parent = parentPath(path);
            focusRow($(`[data-tree-path="${cssEscape(parent)}"]`, listEl));
          }
          return;
        }
        if (e.key === "F2") {
          e.preventDefault();
          startInlineRename(path, type === "dir" ? "dir" : "file");
          return;
        }
        if (e.key === "Delete" || e.key === "Backspace") {
          e.preventDefault();
          executeCommand("explorer.delete", { path, type });
          return;
        }
        if (e.key === "Enter" || e.key === " ") {
          e.preventDefault();
          if (type === "dir") {
            toggleDirectory(path);
          } else if (type === "file") {
            openFileRowIfAllowed(path, { preview: false, permanent: true });
          }
        }
      });

      listEl.addEventListener(
        "focusout",
        (e) => {
          const input = e.target.closest("[data-tree-input]");
          if (input && state.explorer.draft) {
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
        const path = normalizeExplorerPath(row.getAttribute("data-tree-path"));
        const type = row.getAttribute("data-tree-type");
        state.explorer.currentPath = path;

        if (type === "dir") {
          state.explorer.selectedDirPath = path;
          toggleDirectory(path);
          return;
        }
        if (type === "file") {
          state.explorer.selectedDirPath = parentPath(path);
          openFileRowIfAllowed(path, { preview: true });
        }
      });

      listEl.addEventListener("dblclick", (e) => {
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");
        if (type === "file")
          openFileRowIfAllowed(path, { preview: false, permanent: true });
      });

      listEl.addEventListener("contextmenu", (e) => {
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) {
          e.preventDefault();
          showContextMenu(e.clientX, e.clientY, emptyExplorerContextItems(), {
            path: state.explorer.selectedDirPath || ".",
            type: "dir",
          });
          return;
        }
        e.preventDefault();
        const path = normalizeExplorerPath(row.getAttribute("data-tree-path"));
        const type = row.getAttribute("data-tree-type");
        state.explorer.currentPath = path;
        state.explorer.selectedDirPath =
          type === "dir" ? path : parentPath(path);
        showContextMenu(
          e.clientX,
          e.clientY,
          type === "dir" ? dirContextItems(path) : fileContextItems(path),
          { path, type },
        );
      });

      // ---- Explorer drag and drop ----
      listEl.addEventListener("dragstart", (e) => {
        // Never start a drag from an inline input.
        if (e.target.closest("[data-tree-input], [data-tree-rename-input]")) {
          e.preventDefault();
          return;
        }
        const row = e.target.closest("[data-tree-path]");
        if (!row) return;
        const path = row.getAttribute("data-tree-path");
        const type = row.getAttribute("data-tree-type");
        if (!path || path === ".") {
          e.preventDefault();
          return;
        }
        state.drag.tab = null;
        state.drag.explorer = { path: normalizeExplorerPath(path), type };
        row.classList.add("is-dragging");
        if (e.dataTransfer) {
          e.dataTransfer.effectAllowed = "move";
          // Some browsers require data to be set for a drag to begin.
          try {
            e.dataTransfer.setData("text/plain", path);
          } catch (_) {}
        }
      });

      listEl.addEventListener("dragover", (e) => {
        const drag = state.drag.explorer;
        if (!drag) return;

        const row = e.target.closest("[data-tree-path]");
        const targetDir = row ? explorerDropDirForRow(row) : "."; // empty space => root

        clearExplorerDropTargets();

        if (targetDir == null) {
          return; // not a valid target (e.g. a file row)
        }

        if (!canMoveExplorerPath(drag.path, drag.type, targetDir)) {
          return;
        }

        e.preventDefault();
        if (e.dataTransfer) e.dataTransfer.dropEffect = "move";

        if (row) {
          row.classList.add("is-drop-target");
        } else {
          listEl.classList.add("is-root-drop-target");
        }
      });

      listEl.addEventListener("dragleave", (e) => {
        const row = e.target.closest("[data-tree-path]");
        if (row) row.classList.remove("is-drop-target");
      });

      listEl.addEventListener("drop", (e) => {
        const drag = state.drag.explorer;
        if (!drag) return;

        const row = e.target.closest("[data-tree-path]");
        const targetDir = row ? explorerDropDirForRow(row) : ".";

        clearExplorerDropTargets();

        if (targetDir == null) return;
        if (!canMoveExplorerPath(drag.path, drag.type, targetDir)) return;

        e.preventDefault();
        const source = drag.path;
        state.drag.explorer = null;
        moveExplorerPath(source, targetDir);
      });

      listEl.addEventListener("dragend", () => {
        state.drag.explorer = null;
        for (const el of $all(".vn-Tree__row.is-dragging")) {
          el.classList.remove("is-dragging");
        }
        clearExplorerDropTargets();
      });
    }

    const search = $(sel.explorerSearch);
    if (search) search.addEventListener("input", renderExplorer);

    const problemsFilter = $(sel.problemsFilter);
    if (problemsFilter) {
      problemsFilter.addEventListener("input", () => {
        state.diagnostics.filter = problemsFilter.value;
        renderProblems();
      });
    }

    const problemsSeverity = $(sel.problemsSeverity);
    if (problemsSeverity) {
      problemsSeverity.addEventListener("change", () => {
        state.diagnostics.severity = problemsSeverity.value || "all";
        renderProblems();
      });
    }

    const problemsList = $(sel.problemsList);
    if (problemsList) {
      problemsList.addEventListener("click", (e) => {
        const goto = e.target.closest("[data-problem-goto]");
        if (goto) {
          gotoCellFromDiagnostic(goto.getAttribute("data-problem-goto"));
        }
      });
    }

    bindTabsBar();

    document.addEventListener("click", () => closeContextMenu());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeContextMenu();
    });
    window.addEventListener("blur", closeContextMenu);
    window.addEventListener("resize", closeContextMenu);
  }

  /* ==========================================================
   * Wiring: tabs bar (click + reorder drag and drop)
   * ======================================================== */
  function clearTabDropMarkers() {
    for (const el of $all(".vn-Tab.is-drop-before, .vn-Tab.is-drop-after")) {
      el.classList.remove("is-drop-before", "is-drop-after");
    }
  }

  function bindTabsBar() {
    const tabsBar = $(sel.tabsBar);
    if (!tabsBar) return;

    tabsBar.addEventListener("auxclick", (e) => {
      if (e.button !== 1) return;
      const tab = e.target.closest("[data-tab-id]");
      if (tab) {
        e.preventDefault();
        closeTab(tab.getAttribute("data-tab-id"));
      }
    });

    tabsBar.addEventListener("dblclick", (e) => {
      const tab = e.target.closest("[data-tab-id]");
      if (!tab) return;
      const id = tab.getAttribute("data-tab-id");
      const found = state.tabs.find((t) => editorTabId(t) === id);
      if (found) {
        found.preview = false;
        persistTabs();
        renderTabsBar();
      }
    });

    tabsBar.addEventListener("contextmenu", (e) => {
      const tab = e.target.closest("[data-tab-id]");
      if (!tab) return;
      e.preventDefault();
      const id = tab.getAttribute("data-tab-id");
      const found = state.tabs.find((t) => editorTabId(t) === id);
      switchTab(id);
      if (found && isExtensionTab(found)) {
        openContextMenu([{ label: "Close", run: () => closeTab(id) }], { x: e.clientX, y: e.clientY });
      } else if (found) {
        openContextMenu(tabContextItems(found.path), { x: e.clientX, y: e.clientY });
      }
    });

    tabsBar.addEventListener("keydown", (e) => {
      const tab = e.target.closest("[data-tab-id]");
      if (!tab) return;
      if (e.key === "Enter" || e.key === " ") {
        e.preventDefault();
        switchTab(tab.getAttribute("data-tab-id"));
      }
    });

    tabsBar.addEventListener("click", (e) => {
      const close = e.target.closest("[data-tab-close]");
      if (close) {
        e.stopPropagation();
        closeTab(close.getAttribute("data-tab-close"));
        return;
      }
      const tab = e.target.closest("[data-tab-id]");
      if (tab) switchTab(tab.getAttribute("data-tab-id"));
    });

    tabsBar.addEventListener("dragstart", (e) => {
      const tab = e.target.closest("[data-tab-id]");
      if (!tab) return;
      const path = tab.getAttribute("data-tab-id");
      const fromIndex = state.tabs.findIndex((t) => editorTabId(t) === path);
      state.drag.explorer = null;
      state.drag.tab = { path, fromIndex };
      tab.classList.add("is-dragging");
      if (e.dataTransfer) {
        e.dataTransfer.effectAllowed = "move";
        try {
          e.dataTransfer.setData("text/plain", path);
        } catch (_) {}
      }
    });

    tabsBar.addEventListener("dragover", (e) => {
      const drag = state.drag.tab;
      if (!drag) return;
      const tab = e.target.closest("[data-tab-id]");
      if (!tab) return;
      const path = tab.getAttribute("data-tab-id");
      if (path === drag.path) {
        clearTabDropMarkers();
        return;
      }
      e.preventDefault();
      if (e.dataTransfer) e.dataTransfer.dropEffect = "move";

      // Decide before/after from cursor position within the tab.
      const rect = tab.getBoundingClientRect();
      const after = e.clientX > rect.left + rect.width / 2;
      clearTabDropMarkers();
      tab.classList.add(after ? "is-drop-after" : "is-drop-before");
    });

    tabsBar.addEventListener("dragleave", (e) => {
      const tab = e.target.closest("[data-tab-id]");
      if (tab) tab.classList.remove("is-drop-before", "is-drop-after");
    });

    tabsBar.addEventListener("drop", (e) => {
      const drag = state.drag.tab;
      if (!drag) return;
      const tab = e.target.closest("[data-tab-id]");
      if (!tab) {
        clearTabDropMarkers();
        return;
      }
      const targetPath = tab.getAttribute("data-tab-id");
      if (targetPath === drag.path) {
        clearTabDropMarkers();
        return;
      }
      e.preventDefault();
      const rect = tab.getBoundingClientRect();
      const after = e.clientX > rect.left + rect.width / 2;
      const source = drag.path;
      state.drag.tab = null;
      clearTabDropMarkers();
      reorderTabs(source, targetPath, after ? "after" : "before");
    });

    tabsBar.addEventListener("dragend", () => {
      state.drag.tab = null;
      for (const el of $all(".vn-Tab.is-dragging")) {
        el.classList.remove("is-dragging");
      }
      clearTabDropMarkers();
    });

    $(sel.extensionsSearch)?.addEventListener("input", (event) => {
      const input =
        event.target instanceof HTMLInputElement ? event.target : null;
      state.extensionWorkbench.query = input ? input.value : "";
      scheduleMarketplaceSearch();
    });
  }

  function openFileRowIfAllowed(path, options = {}) {
    const entry = state.explorer.entries.get(path);
    if (entry && entry.openable === false && !path.endsWith(".vixnote")) {
      setMessage("Only .vixnote files can be opened in Vix Note.", "warning");
      return;
    }
    openNotePath(path, {
      silent: true,
      preview: !!options.preview,
      permanent: !!options.permanent,
    });
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

      const kindOption = target.closest("[data-cell-kind-option]");
      if (kindOption) {
        event.preventDefault();
        event.stopPropagation();
        const cellEl = kindOption.closest(".vn-Cell");
        const id = cellEl ? cellEl.dataset.cellId : "";
        const kind = kindOption.getAttribute("data-cell-kind-option");
        closeCellKindMenus();
        if (id && kind) void changeKind(id, kind);
        return;
      }

      const kindButton = target.closest("[data-cell-kind-menu]");
      if (kindButton) {
        event.preventDefault();
        event.stopPropagation();
        const id = kindButton.getAttribute("data-cell-kind-menu");
        if (id) {
          selectCell(id, { edit: false });
          toggleCellKindMenu(id);
        }
        return;
      }

      if (!target.closest("[data-cell-kind-select]")) closeCellKindMenus();

      const insertBtn = target.closest("[data-insert-after]");
      if (insertBtn) {
        const afterId = insertBtn.getAttribute("data-insert-after");
        addCell(cellTypeForInsertionAfter(afterId), {
          afterId,
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

    container.addEventListener("keydown", (event) => {
      const ta = event.target;
      if (!(ta instanceof HTMLTextAreaElement)) return;
      if (ta.getAttribute("data-action") !== "edit-source") return;
      if (
        event.key === "Enter" &&
        !event.shiftKey &&
        !event.ctrlKey &&
        !event.metaKey &&
        !event.altKey
      ) {
        event.preventDefault();
        insertAutoIndent(ta);
        return;
      }
      if (
        PAIRS[event.key] &&
        !event.ctrlKey &&
        !event.metaKey &&
        !event.altKey
      ) {
        event.preventDefault();
        handlePairInsertion(ta, event.key);
        return;
      }
      if (
        [")", "]", "}", '"', "'"].includes(event.key) &&
        handleClosingPair(ta, event.key)
      ) {
        event.preventDefault();
      }
    });

    container.addEventListener("contextmenu", (event) => {
      const target = event.target;
      if (!(target instanceof Element)) return;
      const cellEl = target.closest(".vn-Cell");
      if (!cellEl) return;
      event.preventDefault();
      selectCell(cellEl.dataset.cellId, { edit: false, focus: true });
      openContextMenu(cellContextItems(cellEl.dataset.cellId), {
        x: event.clientX,
        y: event.clientY,
      });
    });

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
  const keybindings = [
    { key: "Ctrl+S", mac: "Meta+S", command: "note.save", allowTyping: false },
    { key: "Ctrl+N", mac: "Meta+N", command: "note.new", allowTyping: false },
    { key: "Ctrl+O", mac: "Meta+O", command: "note.open", allowTyping: false },
    { key: "Ctrl+W", mac: "Meta+W", command: "note.close", allowTyping: false },
    {
      key: "Ctrl+Shift+T",
      mac: "Meta+Shift+T",
      command: "note.reopenClosed",
      allowTyping: false,
    },
    {
      key: "Ctrl+P",
      mac: "Meta+P",
      command: "view.quickOpen",
      allowTyping: false,
    },
    {
      key: "Ctrl+Shift+P",
      mac: "Meta+Shift+P",
      command: "view.commandPalette",
      allowTyping: true,
    },
    { key: "F1", command: "view.commandPalette", allowTyping: true },
    {
      key: "Ctrl+B",
      mac: "Meta+B",
      command: "view.toggleSidebar",
      allowTyping: false,
    },
    {
      key: "Ctrl+J",
      mac: "Meta+J",
      command: "view.showProblems",
      allowTyping: false,
    },
    {
      key: "Ctrl+Shift+X",
      mac: "Meta+Shift+X",
      command: "view.showExtensions",
      allowTyping: false,
    },
    {
      key: "Ctrl+Enter",
      mac: "Meta+Enter",
      command: "cell.run",
      allowTyping: true,
    },
    { key: "Shift+Enter", command: "cell.runAndAdvance", allowTyping: true },
    { key: "Alt+Enter", command: "cell.runAndInsertBelow", allowTyping: true },
    {
      key: "Ctrl+Shift+Enter",
      mac: "Meta+Shift+Enter",
      command: "cell.runAll",
      allowTyping: true,
    },
    { key: "Ctrl+F", mac: "Meta+F", command: "view.find", allowTyping: false },
  ];

  function eventKeybinding(event) {
    const parts = [];
    if (event.ctrlKey) parts.push("Ctrl");
    if (event.metaKey) parts.push("Meta");
    if (event.altKey) parts.push("Alt");
    if (event.shiftKey) parts.push("Shift");
    let key = event.key.length === 1 ? event.key.toUpperCase() : event.key;
    parts.push(key);
    return parts.join("+");
  }

  function matchingKeybinding(event) {
    const actual = eventKeybinding(event);
    return (
      keybindings.find(
        (binding) => binding.key === actual || binding.mac === actual,
      ) || null
    );
  }

  let lastDTime = 0;
  function handleDoubleD() {
    const now = Date.now();
    if (now - lastDTime < 500) {
      lastDTime = 0;
      executeCommand("cell.delete");
    } else lastDTime = now;
  }
  async function insertAbove(id) {
    const idx = cellIndex(id);
    if (idx < 0) return;
    await addCell(cellTypeForInsertionAfter(id), { atIndex: idx });
  }

  function inlineInputActive() {
    return !!(state.explorer.draft || state.explorer.rename);
  }

  function bindKeyboard() {
    document.addEventListener("keydown", async (event) => {
      if (contextMenuEl) {
        if (event.key === "Escape") {
          event.preventDefault();
          closeContextMenu();
          return;
        }
        if (event.key === "ArrowDown") {
          event.preventDefault();
          moveContextMenu(1);
          return;
        }
        if (event.key === "ArrowUp") {
          event.preventDefault();
          moveContextMenu(-1);
          return;
        }
        if (event.key === "Enter") {
          event.preventDefault();
          await activateContextMenuItem();
          return;
        }
      }

      const inField = isTypingTarget(event.target);
      const inTextarea = event.target instanceof HTMLTextAreaElement;
      const binding = matchingKeybinding(event);
      if (binding && (!inField || binding.allowTyping)) {
        if (
          binding.command === "cell.runAndAdvance" &&
          inTextarea &&
          !event.shiftKey
        )
          return;
        event.preventDefault();
        await executeCommand(binding.command);
        if (
          state.editing &&
          inTextarea &&
          ["cell.run", "cell.runAndAdvance", "cell.runAndInsertBelow"].includes(
            binding.command,
          )
        )
          enterCommandMode();
        return;
      }

      if (inlineInputActive() && inField) return;

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
        if ((event.ctrlKey || event.metaKey) && event.key === "/") {
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
        updateLineFocus(ta);
        updateCursorStatus(ta);
        return;
      }

      if (inField) return;
      if (event.key === "Escape") {
        exitEditMode({ focusCell: true });
        return;
      }
      if (event.key === "?") {
        event.preventDefault();
        await executeCommand("help.shortcuts");
        return;
      }

      if (event.altKey && /^[1-9]$/.test(event.key)) {
        event.preventDefault();
        await activateTabByIndex(Number(event.key) - 1);
        return;
      }
      if ((event.ctrlKey || event.metaKey) && event.key === "Tab") {
        event.preventDefault();
        await activateRelativeTab(event.shiftKey ? -1 : 1);
        return;
      }

      if (!state.selectedId) return;
      switch (event.key.toLowerCase()) {
        case "enter":
          event.preventDefault();
          await executeCommand("cell.focusEditor");
          break;
        case "arrowup":
        case "k":
          event.preventDefault();
          selectAdjacent(-1);
          break;
        case "arrowdown":
        case "j":
          event.preventDefault();
          selectAdjacent(1);
          break;
        case "a":
          event.preventDefault();
          await executeCommand("cell.insertAbove");
          break;
        case "b":
          event.preventDefault();
          await executeCommand("cell.insertBelow");
          break;
        case "d":
          handleDoubleD();
          break;
        case "c":
          event.preventDefault();
          await executeCommand("cell.copy");
          break;
        case "x":
          event.preventDefault();
          await executeCommand("cell.cut");
          break;
        case "v":
          event.preventDefault();
          await executeCommand("cell.pasteBelow");
          break;
        case "m":
          event.preventDefault();
          await executeCommand("cell.toMarkdown");
          break;
        case "y":
          event.preventDefault();
          await executeCommand("cell.toCpp");
          break;
        case "r":
          event.preventDefault();
          await executeCommand("cell.toReply");
          break;
        case "h":
          event.preventDefault();
          await executeCommand("cell.toHtml");
          break;
        default:
          break;
      }
    });

    window.addEventListener("beforeunload", (event) => {
      if (state.tabs.some((tab) => tab.dirty)) {
        event.preventDefault();
        event.returnValue = "You have unsaved Vix Note changes.";
      }
    });
  }

  /* ==========================================================
   * Init
   * ======================================================== */
  function init() {
    registerCoreCommands();
    restoreUiState();
    restoreTheme();
    restorePersistedTabs();
    restoreSession();
    applySidebarWidth(state.sidebarWidth || DEFAULT_SIDEBAR_WIDTH);
    app.classList.toggle(
      "is-sidebar-collapsed",
      !!state.sidebarCollapsed ||
        window.matchMedia("(max-width: 900px)").matches,
    );
    state.sidebarCollapsed = app.classList.contains("is-sidebar-collapsed");
    app.classList.toggle("is-focus", !!state.focusMode);

    bindActions();
    bindActivityBar();
    bindMenus();
    bindSidebarResize();
    bindCellInteractions();
    bindExplorer();
    bindPickers();
    bindKeyboard();

    const statusProblems = $(sel.statusProblems);
    if (statusProblems) {
      statusProblems.addEventListener("click", () =>
        toggleActivityPanel("problems"),
      );
    }

    for (const c of $all("[data-modal-close]"))
      c.addEventListener("click", () => closeModal());

    syncActivityPanelState({ persist: false });
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
