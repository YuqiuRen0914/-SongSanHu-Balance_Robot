// /assets/js/modules/pid.js
import { state, domElements, PID_KEYS } from "../config.js";
import { sendWebSocketMessage } from "../services/websocket.js";
import { appendLog } from "../ui.js";

// Helper to query PID related elements by key
const getPidElements = (k) => ({
  sv: document.getElementById(`sv_${k}`),
  nv: document.getElementById(`nv_${k}`),
  rg: document.getElementById(`rg_${k}`),
});

/**
 * Binds events for a single PID key (slider and number input)
 * @param {object} keyInfo - { k: 'key01', fix: 3 }
 */
function bindPidKey(keyInfo) {
  const { k, fix } = keyInfo;
  const { sv, nv, rg } = getPidElements(k);

  if (!nv || !rg) return;

  // Initialize state with value from the number input
  const initialValue = parseFloat(nv.value || "0");
  state.pidParams[k] = initialValue;
  if (sv) sv.textContent = initialValue.toFixed(fix);

  // Sync slider's position from a given value, clamping it within min/max
  const updateSliderPosition = (value) => {
    const lo = parseFloat(rg.min);
    const hi = parseFloat(rg.max);
    let clampedValue = value;
    if (Number.isFinite(lo) && clampedValue < lo) clampedValue = lo;
    if (Number.isFinite(hi) && clampedValue > hi) clampedValue = hi;
    rg.value = String(clampedValue);
  };

  updateSliderPosition(initialValue);

  // --- Event Listeners ---

  // Slider changes -> update number input and state
  rg.addEventListener("input", () => {
    const value = parseFloat(rg.value);
    nv.value = value.toFixed(fix);
    if (sv) sv.textContent = value.toFixed(fix);
    state.pidParams[k] = value;
  });

  // Number input changes -> update display, state, and slider position
  nv.addEventListener("input", () => {
    const rawValue = nv.value;
    const value = parseFloat(rawValue);
    if (Number.isFinite(value)) {
      if (sv) sv.textContent = rawValue; // Show intermediate value for better UX
      updateSliderPosition(value);
      state.pidParams[k] = value;
    }
  });

  // Format and commit number input on change, blur, or Enter key
  const commitNumberInput = () => {
    let value = parseFloat(nv.value);
    if (!Number.isFinite(value)) value = 0;

    // Format the number input
    nv.value = value.toFixed(fix);

    // Manually trigger the 'input' event on the slider to sync everything
    rg.dispatchEvent(new Event("input"));
  };

  nv.addEventListener("change", commitNumberInput);
  nv.addEventListener("blur", commitNumberInput);
  nv.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      commitNumberInput();
      e.preventDefault(); // Prevent form submission if any
    }
  });
}

/**
 * Initializes all PID control interactions.
 */
export function initPID() {
  PID_KEYS.forEach(bindPidKey);
  domElements.btnPidSend.onclick = () => {
    const param = {};
    PID_KEYS.forEach(({ k }) => {
      const v = state.pidParams[k];
      const num = Number.isFinite(v) ? v : 0;
      param[k] = num;
    });
    sendWebSocketMessage({ type: "set_pid", param });
    appendLog("[SAVE] PID参数已保存");
  };
  domElements.btnPidPull.onclick = () =>
    sendWebSocketMessage({ type: "get_pid" });
}

/**
 * Updates the UI with PID parameters received from the backend.
 * @param {object} params - Object with PID key-value pairs.
 */
export function fillPidToUI(params) {
  Object.keys(params).forEach((k) => {
    const keyInfo = PID_KEYS.find((item) => item.k === k);
    if (!keyInfo) return; // Ignore unknown keys

    const v = +params[k];
    if (!Number.isFinite(v)) return;

    const { sv, nv, rg } = getPidElements(k);

    if (sv) sv.textContent = v.toFixed(keyInfo.fix);
    if (nv) nv.value = v.toFixed(keyInfo.fix);
    if (rg) rg.value = String(v); // This will also be clamped by the browser if out of range

    state.pidParams[k] = v;
  });
}

/**
 * 应用 slider group names 和 labels from a configuration object.
 * @param {Array<object>} config
 */
export function applySliderConfig(config) {
  if (!Array.isArray(config)) return;
  config.forEach((groupConfig, groupIndex) => {
    // 增加对 pidGroup5 的支持
    const titleEl = document.querySelector(`#pidGroup${groupIndex + 1}`);
    if (titleEl && groupConfig.group) titleEl.textContent = groupConfig.group;

    if (Array.isArray(groupConfig.names)) {
      groupConfig.names.forEach((name, sliderIndex) => {
        const labelEl = document.querySelector(
          `#pid${groupIndex + 1}Label${sliderIndex + 1}`
        );
        if (labelEl) labelEl.textContent = name;
      });
    }
  });
}
