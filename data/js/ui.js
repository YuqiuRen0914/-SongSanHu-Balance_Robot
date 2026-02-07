// /assets/js/ui.js
import { state, domElements } from "./config.js";
import { sendWebSocketMessage } from "./services/websocket.js";

/**
 * 更新状态显示文本
 * @param {string} text - The status text to display.
 */
export function setStatus(text) {
  domElements.statusLabel.textContent = `连接状态: ${text}`;
}

/**
 * 在日志区域打印一行新日志（覆盖旧的）
 * @param {string} text
 */
export function logLine(text) {
  domElements.log.textContent = String(text);
}

/**
 * 在日志区域追加一行日志
 * @param {string} text
 */
export function appendLog(text) {
  const el = domElements.log;
  const isScrolledToBottom =
    el.scrollTop + el.clientHeight >= el.scrollHeight - 2;
  el.textContent += `\n${text}`;
  if (isScrolledToBottom) {
    el.scrollTop = el.scrollHeight;
  }
}

/**
 * 更新摔倒状态指示灯
 * @param {boolean|null} fallState - null/undefined for off, true for fallen (red), false for stable (green).
 */
export function updateFallIndicator(fallState) {
  // 全局警报态：摔倒时给页面加上红色警告背景，恢复时淡出
  const body = document.body;
  if (body) body.classList.toggle("alert-fall", !!fallState);

  const { fallLamp, fallLabel } = domElements;
  if (!fallLamp || !fallLabel) return;

  fallLamp.classList.remove("green", "red", "off");

  if (fallState === null || typeof fallState === "undefined") {
    fallLamp.classList.add("off");
    fallLabel.textContent = "—";
  } else if (fallState) {
    fallLamp.classList.add("red");
    fallLabel.textContent = "已摔倒";
  } else {
    fallLamp.classList.add("green");
    fallLabel.textContent = "稳定";
  }
}

export function updateEnergyBar(voltage) {
  if (!domElements.energyFill || !domElements.energyText) return;
  const minV = 9.0;
  const maxV = 12.6;
  const clamped = Math.min(Math.max(voltage, minV), maxV);
  const pct = Math.min(100, Math.max(0, ((clamped - minV) / (maxV - minV)) * 100));
  const pctText = Math.round(pct);
  const hueShift = (50 - pct) * 0.6; // 绿->蓝 平滑过渡
  const brightness = 0.85 + pct * 0.0015;
  domElements.energyFill.style.width = `${pct}%`;
  domElements.energyFill.style.filter = `hue-rotate(${hueShift}deg) brightness(${brightness})`;
  domElements.energyText.textContent = `${pctText}%`;
}

/**
 * 绑定顶部工具栏的事件监听
 */
function bindToolbarEvents() {
  domElements.btnSetRate.onclick = () => {
    const ms = Math.max(
      20,
      Math.min(1000, parseInt(domElements.rateHzInput.value || "100", 100))
    );
    sendWebSocketMessage({ type: "telem_hz", ms });
    appendLog(`[SEND] telem_hz ${ms} Hz`);
  };

  domElements.runSwitch.onchange = () => {
    sendWebSocketMessage({
      type: "robot_run",
      running: domElements.runSwitch.checked,
    });
    appendLog(
      `[SEND] robot_run ${domElements.runSwitch.checked ? "on" : "off"}`
    );
  };

  domElements.chartSwitch.onchange = () => {
    state.chartsOn = domElements.chartSwitch.checked;
    sendWebSocketMessage({ type: "charts_send", on: state.chartsOn });
    appendLog(`[INFO] charts ${state.chartsOn ? "enabled" : "disabled"}`);
    if (!state.chartsOn) {
      updateFallIndicator(null);
    }
  };

  domElements.fallDetectSwitch.onchange = () => {
    sendWebSocketMessage({
      type: "fall_check",
      enable: domElements.fallDetectSwitch.checked,
    });
    appendLog(
      `[SEND] fall_check ${domElements.fallDetectSwitch.checked ? "on" : "off"}`
    );
  };

  domElements.btnZeroAtt.onclick = () => {
    sendWebSocketMessage({ type: "imu_restart" });
    appendLog("[INFO] attitude zeroed (roll/yaw)");
  };

  domElements.btnSystemRestart.onclick = () => {
    if (confirm("确认要重启ESP32控制器吗？\n\n重启后需要等待约10秒重新连接。")) {
      sendWebSocketMessage({ type: "system_restart" });
      appendLog("[SEND] system_restart - ESP32 正在重启...");
      setStatus("系统重启中...");
      // 5秒后尝试重新连接
      setTimeout(() => {
        appendLog("[INFO] 尝试重新连接...");
        location.reload();
      }, 5000);
    }
  };
}

/**
 * 初始化按钮的按压视觉效果 (PC + 触摸)
 */
function initButtonPressEffects() {
  const addPressEffect = (el) => {
    el.addEventListener("pointerdown", () => el.classList.add("is-press"));
    const removePressEffect = () => el.classList.remove("is-press");
    el.addEventListener("pointerup", removePressEffect);
    el.addEventListener("pointerleave", removePressEffect);
    el.addEventListener("pointercancel", removePressEffect);
  };

  document.querySelectorAll(".btn").forEach(addPressEffect);
}

/**
 * 初始化所有UI相关的事件绑定
 */
export function initUI() {
  bindToolbarEvents();
  initButtonPressEffects();

  // 监听页面可见性变化
  document.addEventListener("visibilitychange", () => {
    state.isPageVisible = !document.hidden;
  });
}
