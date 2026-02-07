// /assets/js/services/websocket.js
import { state, domElements } from "../config.js";
import { setStatus, appendLog } from "../ui.js";
import { updatePitchZero } from "../modules/pitchZero.js";

let ws = null;
let reconnectTimer = null;
let telemetryCallback = null;
let uiConfigCallback = null;
let pidParamsCallback = null;
let rgbStateCallback = null;
let groupConfigCallback = null;

/**
 * 发送 WebSocket 消息 (JSON)
 * @param {object} obj
 */
export function sendWebSocketMessage(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

/**
 * 处理收到的 WebSocket 消息
 * @param {MessageEvent} event
 */
function handleMessage(event) {
  let msg = null;
  try {
    msg = JSON.parse(event.data);
  } catch (e) {
    console.error("Failed to parse WebSocket message:", event.data);
    return;
  }

  if (!msg || !msg.type) return;

  switch (msg.type) {
    case "telemetry":
      if (telemetryCallback) telemetryCallback(msg);
      break;
    case "ui_config":
      if (uiConfigCallback) uiConfigCallback(msg);
      break;
    case "pid":
      if (pidParamsCallback) pidParamsCallback(msg);
      break;
    case "rgb_state":
      if (rgbStateCallback) rgbStateCallback(msg);
      break;
    case "group_config":
      if (groupConfigCallback) groupConfigCallback(msg);
      break;
    case "pitch_zero_state":
      if (typeof msg.value === "number") {
        updatePitchZero(msg.value);
        if (msg.saved) {
          appendLog("[OK] 平衡零点已保存");
        }
      }
      break;
    case "info":
      if (msg.text) appendLog(`[INFO] ${msg.text}`);
      break;
    default:
      // 未知消息类型
      break;
  }
}

/**
 * 尝试重新连接 WebSocket
 */
function reconnect() {
  clearTimeout(reconnectTimer);
  reconnectTimer = setTimeout(connectWebSocket, 1000);
}

/**
 * 连接到 WebSocket 服务器
 * @param {object} callbacks - 回调函数集合
 * @param {function} callbacks.onTelemetry - 遥测数据回调
 * @param {function} callbacks.onUiConfig - UI配置数据回调
 * @param {function} callbacks.onPidParams - PID参数数据回调
 * @param {function} callbacks.onRgbState - RGB状态回调
 * @param {function} callbacks.onGroupConfig - 车队配置回调
 */
export function connectWebSocket(callbacks = {}) {
  if (callbacks.onTelemetry) telemetryCallback = callbacks.onTelemetry;
  if (callbacks.onUiConfig) uiConfigCallback = callbacks.onUiConfig;
  if (callbacks.onPidParams) pidParamsCallback = callbacks.onPidParams;
  if (callbacks.onRgbState) rgbStateCallback = callbacks.onRgbState;
  if (callbacks.onGroupConfig) groupConfigCallback = callbacks.onGroupConfig;

  const protocol = location.protocol === "http:" ? "ws://" : "wss://";
  const url = `${protocol}${location.host}/ws`;

  setStatus("连接中…");

  try {
    ws = new WebSocket(url);

    ws.onopen = () => {
      state.connected = true;
      setStatus("已就绪");
      sendWebSocketMessage({ type: "get_pid" });
      appendLog("[SEND] get_pid");
    };

    ws.onclose = () => {
      state.connected = false;
      setStatus("重新连接中…");
      reconnect();
    };

    ws.onerror = () => {
      state.connected = false;
      setStatus("异常");
      // onerror 也会触发 onclose，所以重连逻辑在 onclose 中统一处理
    };

    ws.onmessage = handleMessage;
  } catch (e) {
    console.error("WebSocket connection failed:", e);
    reconnect();
  }
}

/**
 * 通过 HTTP API 同步初始状态
 */
export async function syncInitialState() {
  try {
    const response = await fetch("/api/state");
    if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
    const s = await response.json();

    if (typeof s.ms === "number") domElements.rateHzInput.value = s.ms;
    if (typeof s.running === "boolean")
      domElements.runSwitch.checked = !!s.running;
    if (typeof s.chart_enable === "boolean") {
      domElements.chartSwitch.checked = !!s.chart_enable;
      state.chartsOn = !!s.chart_enable;
    }
    if (typeof s.rgb_mode === "number") state.rgb.mode = s.rgb_mode;
    if (typeof s.rgb_count === "number") state.rgb.count = s.rgb_count;
    if (typeof s.rgb_max === "number") state.rgb.max = s.rgb_max;
    if (domElements.rgbCountInput) {
      domElements.rgbCountInput.value = state.rgb.count;
      domElements.rgbCountInput.max = state.rgb.max;
    }
    if (domElements.rgbCountRange) {
      domElements.rgbCountRange.value = state.rgb.count;
      domElements.rgbCountRange.max = state.rgb.max;
    }
    if (domElements.rgbCountValue) domElements.rgbCountValue.textContent = state.rgb.max ?? state.rgb.count;
    if (s.wifi) {
      state.wifi.ssid = s.wifi.ssid || state.wifi.ssid;
      state.wifi.password = s.wifi.password || state.wifi.password;
      state.wifi.open =
        typeof s.wifi.open === "boolean"
          ? s.wifi.open
          : (s.wifi.password || "").length < 8;
      state.wifi.ip = s.wifi.ip || state.wifi.ip;
    }
    if (typeof s.battery === "number") {
      state.battery.voltage = s.battery;
    }
    if (typeof s.pitch_zero === "number") {
      updatePitchZero(s.pitch_zero);
    }
    appendLog(`[INIT] /api/state ok -> running=${s.running}, ms=${s.ms}`);
    return s;
  } catch (e) {
    appendLog(`[INIT] /api/state fail: ${e.message}`);
    return null;
  }
}
