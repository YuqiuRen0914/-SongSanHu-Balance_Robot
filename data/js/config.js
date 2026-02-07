// /assets/js/config.js

/**
 * 全局状态管理
 */
export const state = {
  connected: false,
  chartsOn: false,
  isPageVisible: true,
  joystick: { x: 0, y: 0, a: 0, isDragging: false, lastSendTime: 0 },
  pidParams: {},
  attitudeZero: { roll: 0, yaw: 0 },
  rgb: { mode: 0, count: 5, max: 12 },
  wifi: { ssid: "", password: "", open: false, ip: "" },
  battery: { voltage: 0, percent: 0 },
  pitchZero: -2.1,
  charts: {
    chart1: null,
    chart2: null,
    chart3: null,
  },
  three: {
    scene: null,
    camera: null,
    renderer: null,
    robot: null,
    pcb: null,
  },
};

// 2. DOM 元素缓存
function getElement(id) {
  const element = document.getElementById(id);
  if (!element) {
    // 允许在测试环境中某些元素不存在
    console.warn(`Element with ID '${id}' not found.`);
    return null;
  }
  return element;
}
/**
 * DOM 元素引用
 * 统一在此处获取，方便管理和维护
 */
export const domElements = {
  // Toolbar
  btnSetRate: getElement("btnSetRate"),
  rateHzInput: getElement("rateHz"),
  runSwitch: getElement("runSwitch"),
  chartSwitch: getElement("chartSwitch"),
  fallDetectSwitch: getElement("fallDetectSwitch"),
  statusLabel: getElement("status"),

  // Indicators
  fallLamp: getElement("fallLamp"),
  fallLabel: getElement("fallLabel"),
  energyFill: getElement("energyFill"),
  energyText: getElement("energyText"),

  // Charts
  chart1: {
    canvas: getElement("chart1"),
    title: getElement("chartTitle1"),
  },
  chart2: {
    canvas: getElement("chart2"),
    title: getElement("chartTitle2"),
  },
  chart3: {
    canvas: getElement("chart3"),
    title: getElement("chartTitle3"),
  },

  // PID Controls
  pidCard: getElement("pidCard"),
  btnPidSend: getElement("btnPidSend"),
  btnPidPull: getElement("btnPidPull"),

  // 3D View
  robotCanvas: getElement("robotCanvas"),
  btnZeroAtt: getElement("btnZeroAtt"),
  attOut: getElement("attOut"),
  // Pitch Zero Control
  pitchZeroValue: getElement("pitchZeroValue"),
  pitchZeroRange: getElement("pitchZeroRange"),
  btnPitchZeroSend: getElement("btnPitchZeroSend"),
  // Joystick
  joystick: getElement("joystick"),
  stick: getElement("stick"),
  joyOut: getElement("joyOut"),

  // Log
  log: getElement("log"),

  // RGB
  rgbCard: getElement("rgbCard"),
  rgbModeList: getElement("rgbModeList"),
  rgbModeDesc: getElement("rgbModeDesc"),
  rgbModeTag: getElement("rgbModeTag"),
  rgbCountInput: getElement("rgbCountInput"),
  rgbCountRange: getElement("rgbCountRange"),
  rgbCountValue: getElement("rgbCountValue"),

  // WiFi
  wifiSsidInput: getElement("wifiSsid"),
  wifiPasswordInput: getElement("wifiPassword"),
  wifiSaveBtn: getElement("wifiSaveBtn"),
  wifiHint: getElement("wifiHint"),
  wifiIp: getElement("wifiIp"),
  wifiTogglePwd: getElement("wifiTogglePwd"),
  
  // System
  btnSystemRestart: getElement("btnSystemRestart"),
};

/**
 * 全局常量
 */
export const CONSTANTS = {
  MAX_CHART_POINTS: 100,  // 减少数据点，使波形更清晰
  JOYSTICK_SEND_INTERVAL: 50, // ms, 20Hz
};

// PID 参数键名列表
export const PID_KEYS = Array.from({ length: 12 }, (_, i) => {
    const key = `key${String(i + 1).padStart(2, "0")}`;
    return { k: key, fix: 3 };
});
