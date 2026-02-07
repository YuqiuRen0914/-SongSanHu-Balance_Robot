import { state, domElements } from "../config.js";
import { appendLog } from "../ui.js";

const MIN_PWD_LEN = 8;
const MAX_PWD_LEN = 63;
const MAX_SSID_LEN = 32;

function setHint(text, isError = false) {
  if (!domElements.wifiHint) return;
  domElements.wifiHint.textContent = text;
  domElements.wifiHint.classList.toggle("error", !!isError);
}

function updateToggleLabel() {
  if (!domElements.wifiTogglePwd || !domElements.wifiPasswordInput) return;
  const showing = domElements.wifiPasswordInput.type === "text";
  domElements.wifiTogglePwd.textContent = showing ? "隐藏密钥" : "显示密钥";
}

function fillWifiForm() {
  const { wifiSsidInput, wifiPasswordInput, wifiIp } = domElements;
  if (wifiSsidInput) wifiSsidInput.value = state.wifi.ssid || "";
  if (wifiPasswordInput) wifiPasswordInput.value = state.wifi.password || "";
  if (wifiIp) wifiIp.textContent = state.wifi.ip || "192.168.4.1";
  updateToggleLabel();
}

function hydrateWifiState(payload) {
  if (!payload) return;
  state.wifi.ssid = payload.ssid ?? state.wifi.ssid ?? "";
  state.wifi.password = payload.password ?? state.wifi.password ?? "";
  const open =
    typeof payload.open === "boolean"
      ? payload.open
      : (payload.password || "").length < MIN_PWD_LEN;
  state.wifi.open = open;
  state.wifi.ip = payload.ip ?? state.wifi.ip ?? "";
  fillWifiForm();
}

function updateHintFromState(defaultText = "通信参数已更新") {
  if (state.wifi.open) {
    setHint("当前通信为开放模式（密钥少于 8 位），建议设置新密钥", true);
  } else {
    setHint(defaultText, false);
  }
}

function readInputs() {
  const ssid = (domElements.wifiSsidInput?.value || "").trim();
  const password = (domElements.wifiPasswordInput?.value || "").trim();
  return { ssid, password };
}

function validateInputs(ssid, password) {
  if (!ssid) throw new Error("SSID 不能为空");
  if (ssid.length > MAX_SSID_LEN) throw new Error("SSID 需不超过 32 个字符");
  if (password.length && password.length < MIN_PWD_LEN)
    throw new Error("密码需至少 8 位或留空为开放热点");
  if (password.length > MAX_PWD_LEN)
    throw new Error("密码需不超过 63 位");
}

async function fetchWifiInfo() {
  try {
    const res = await fetch("/api/wifi");
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    hydrateWifiState(data.wifi || data);
    updateHintFromState("通信参数已更新");
    appendLog(
      `[WIFI] 当前链路: ${state.wifi.ssid}${state.wifi.open ? " (开放)" : ""}`
    );
  } catch (e) {
    setHint(`获取通信配置失败: ${e.message}`, true);
    appendLog(`[WARN] 获取 Wi-Fi 配置失败: ${e.message}`);
  }
}

async function saveWifiConfig() {
  const btn = domElements.wifiSaveBtn;
  const { ssid, password } = readInputs();
  try {
    validateInputs(ssid, password);
  } catch (e) {
    setHint(e.message, true);
    return;
  }

  if (btn) {
    btn.disabled = true;
    btn.textContent = "重启中…";
  }
  setHint("重启通信模块中…");

  try {
    const res = await fetch("/api/wifi", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ssid, password }),
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok || data.ok === false)
      throw new Error(data.error || `HTTP ${res.status}`);

    hydrateWifiState(data.wifi || data);
    if (state.wifi.open) {
      setHint("已更新，当前为开放通信（密钥少于 8 位），建议设置 8 位以上密钥", true);
    } else {
      setHint("已更新，通信模块正在重启，如断开请重新连接", false);
    }
    appendLog(
      `[WIFI] 通信模块已更新 -> ${state.wifi.ssid}${state.wifi.open ? " (开放)" : ""}`
    );
  } catch (e) {
    setHint(`更新失败: ${e.message}`, true);
    appendLog(`[WARN] 通信更新失败: ${e.message}`);
  } finally {
    if (btn) {
      btn.disabled = false;
      btn.textContent = "重启通信模块";
    }
  }
}

function bindEvents() {
  if (domElements.wifiTogglePwd && domElements.wifiPasswordInput) {
    domElements.wifiTogglePwd.addEventListener("click", () => {
      const input = domElements.wifiPasswordInput;
      input.type = input.type === "password" ? "text" : "password";
      updateToggleLabel();
    });
  }
  if (domElements.wifiSaveBtn) {
    domElements.wifiSaveBtn.addEventListener("click", () => saveWifiConfig());
  }
}

export function initWifiSettings() {
  fillWifiForm();
  bindEvents();
  fetchWifiInfo();
}

export function applyWifiStateFromHttp(payload) {
  hydrateWifiState(payload);
  updateHintFromState();
}
