import { state, domElements } from "../config.js";
import { appendLog } from "../ui.js";
import { sendWebSocketMessage } from "../services/websocket.js";

const FALLBACK_MODES = [
  { id: 0, name: "霓虹循环", desc: "多彩光环顺时针流动" },
  { id: 1, name: "呼吸律动", desc: "青蓝渐隐渐现" },
  { id: 2, name: "流星雨", desc: "暖白尾焰快速掠过" },
  { id: 3, name: "心跳红", desc: "双击式心跳闪烁" },
];

let rgbModes = [...FALLBACK_MODES];

function clampCount(count) {
  const max = Number.isFinite(state.rgb.max) ? state.rgb.max : 12;
  const v = parseInt(count ?? state.rgb.count ?? 1, 10);
  if (!Number.isFinite(v) || v <= 0) return 1;
  return Math.min(v, max);
}

function renderModeButtons() {
  const wrap = domElements.rgbModeList;
  if (!wrap) return;
  wrap.innerHTML = "";
  const fragment = document.createDocumentFragment();
  rgbModes.forEach((m, idx) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "chip";
    btn.dataset.mode = typeof m.id === "number" ? m.id : idx;
    btn.textContent = m.name || `模式${idx + 1}`;
    btn.title = m.desc || "";
    fragment.appendChild(btn);
  });
  wrap.appendChild(fragment);
  highlightActiveMode();
}

function updateModeDescription() {
  const descEl = domElements.rgbModeDesc;
  const tagEl = domElements.rgbModeTag;
  if (!descEl) return;
  const modeInfo =
    rgbModes.find((m) => m.id === state.rgb.mode) ||
    rgbModes[state.rgb.mode] ||
    null;
  descEl.textContent = modeInfo?.desc || "选择一个灯效模式进行预览和切换";
  if (tagEl) {
    tagEl.textContent = modeInfo?.name
      ? `${modeInfo.name} / Mode ${state.rgb.mode}`
      : `Mode ${state.rgb.mode}`;
  }
}

function highlightActiveMode() {
  const wrap = domElements.rgbModeList;
  if (!wrap) return;
  wrap.querySelectorAll("button[data-mode]").forEach((btn) => {
    const isActive = Number(btn.dataset.mode) === state.rgb.mode;
    btn.classList.toggle("active", isActive);
  });
}

function applyRgbTheme() {
  const card = domElements.rgbCard;
  if (!card) return;
  const themes = ["neon", "breath", "meteor", "heart"];
  card.classList.remove(...themes.map((t) => `rgb-theme-${t}`));
  const key = themes[state.rgb.mode] || themes[0];
  card.classList.add(`rgb-theme-${key}`);
}

function updateCountUI() {
  const { rgbCountInput, rgbCountRange, rgbCountValue } = domElements;
  if (rgbCountInput) {
    rgbCountInput.min = "1";
    rgbCountInput.max = String(state.rgb.max || 12);
    rgbCountInput.value = state.rgb.count ?? "";
  }
  if (rgbCountRange) {
    rgbCountRange.min = "1";
    rgbCountRange.max = String(state.rgb.max || 12);
    rgbCountRange.value = state.rgb.count ?? rgbCountRange.value ?? 1;
  }
  if (rgbCountValue) {
    rgbCountValue.textContent = state.rgb.max ?? state.rgb.count ?? "—";
  }
}

function sendRgbUpdate(payload) {
  const next = {
    mode:
      typeof payload.mode === "number" ? payload.mode : state.rgb.mode ?? 0,
    count:
      typeof payload.count === "number"
        ? clampCount(payload.count)
        : clampCount(state.rgb.count ?? 1),
  };

  state.rgb.mode = next.mode;
  state.rgb.count = next.count;

  sendWebSocketMessage({
    type: "rgb_set",
    mode: next.mode,
    count: next.count,
  });
  appendLog(`[SEND] rgb_set mode=${next.mode} count=${next.count}`);

  highlightActiveMode();
  updateModeDescription();
  updateCountUI();
  applyRgbTheme();
}

function bindModeEvents() {
  const wrap = domElements.rgbModeList;
  if (!wrap) return;
  wrap.addEventListener("click", (e) => {
    const btn = e.target.closest("button[data-mode]");
    if (!btn) return;
    const mode = parseInt(btn.dataset.mode, 10);
    if (Number.isNaN(mode)) return;
    sendRgbUpdate({ mode });
  });
}

function bindCountEvents() {
  const { rgbCountInput, rgbCountRange } = domElements;
  const syncCount = (value, push = false) => {
    state.rgb.count = clampCount(value);
    updateCountUI();
    if (push) {
      sendRgbUpdate({ count: state.rgb.count });
    }
  };

  if (rgbCountInput) {
    rgbCountInput.addEventListener("change", (e) =>
      syncCount(e.target.value, true)
    );
  }
  if (rgbCountRange) {
    rgbCountRange.addEventListener("input", (e) => syncCount(e.target.value));
    rgbCountRange.addEventListener("change", (e) =>
      syncCount(e.target.value, true)
    );
  }
}

export function initRgbControls() {
  renderModeButtons();
  bindModeEvents();
  bindCountEvents();
  updateModeDescription();
  updateCountUI();
  applyRgbTheme();
}

export function applyRgbConfig(cfg) {
  if (!cfg) return;
  if (Array.isArray(cfg.modes) && cfg.modes.length) {
    rgbModes = cfg.modes.map((m, idx) => ({
      id: typeof m.id === "number" ? m.id : idx,
      name: m.name || FALLBACK_MODES[idx]?.name || `模式${idx + 1}`,
      desc: m.desc || FALLBACK_MODES[idx]?.desc || "",
    }));
  }
  if (typeof cfg.max_count === "number") {
    state.rgb.max = cfg.max_count;
  }
  if (cfg.state) {
    updateRgbState(cfg.state, true);
  }
  renderModeButtons();
  updateModeDescription();
  updateCountUI();
  applyRgbTheme();
}

export function updateRgbState(payload, silent = false) {
  if (payload && typeof payload.mode === "number") {
    state.rgb.mode = payload.mode;
  }
  if (payload && typeof payload.count === "number") {
    state.rgb.count = clampCount(payload.count);
  }
  highlightActiveMode();
  updateModeDescription();
  updateCountUI();
  applyRgbTheme();
  if (!silent) {
    appendLog(`[INFO] rgb -> mode=${state.rgb.mode} count=${state.rgb.count}`);
  }
}
