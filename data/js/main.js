// /assets/js/main.js
import { state, domElements } from "./config.js";
import { initUI, updateFallIndicator, updateEnergyBar, logLine } from "./ui.js";
import { initCharts, feedChartsData, applyChartConfig } from "./modules/charts.js";
import { init3D, setAttitude } from "./modules/robot3D.js";
import { initPID, fillPidToUI, applySliderConfig } from "./modules/pid.js";
import { initJoystick } from "./modules/joystick.js";
import {
  initRgbControls,
  applyRgbConfig,
  updateRgbState,
} from "./modules/rgb.js";
import { initWifiSettings, applyWifiStateFromHttp } from "./modules/wifi.js";
import { initGroup, handleGroupConfig, handleGroupStatus } from "./modules/group.js";
import { initPitchZero } from "./modules/pitchZero.js";
import { connectWebSocket, syncInitialState } from "./services/websocket.js";

/**
 * 主初始化函数
 */
async function main() {
  logLine('initializing...');
  
  // 初始化各个模块
  initUI();
  initCharts();
  initPID();
  initRgbControls();
  initJoystick();
  initGroup();
  initPitchZero();
  init3D();

  // 启动时将指示灯置为初始状态
  updateFallIndicator(null);
  updateEnergyBar(0);
  
  // 从后端同步初始状态 (HTTP)
  const initState = await syncInitialState();
  applyWifiStateFromHttp(initState?.wifi);
  if (typeof initState?.battery === "number") updateEnergyBar(initState.battery);
  initWifiSettings();
  
  // 建立 WebSocket 连接
  connectWebSocket({
    onTelemetry: (msg) => {
      // 姿态更新
      if (typeof msg.pitch !== 'undefined') {
        setAttitude(msg.pitch, msg.roll, msg.yaw);
      }

      if (typeof msg.fallen !== 'undefined') {
        updateFallIndicator(msg.fallen);
      }
      
      // 图表和指示灯更新 (仅在图表开启时)
      if (state.chartsOn) {
        if (Array.isArray(msg.d) && msg.d.length >= 9) {
          feedChartsData(...msg.d);
        }
      }
      if (typeof msg.battery === "number") {
        updateEnergyBar(msg.battery);
      }
      
      // 车队状态
      if (msg.group_status) {
        handleGroupStatus(msg);
      }
    },
    onUiConfig: (msg) => {
      if (msg.charts) applyChartConfig(msg.charts);
      if (msg.sliders) applySliderConfig(msg.sliders);
      if (msg.rgb) applyRgbConfig(msg.rgb);
    },
    onPidParams: (msg) => {
      if (msg.param) fillPidToUI(msg.param);
    },
    onRgbState: (msg) => updateRgbState(msg, true),
    onGroupConfig: (msg) => {
      if (msg.type === 'group_config') handleGroupConfig(msg);
    },
  });

  logLine('ready');
}

// DOM加载完毕后执行主函数
window.addEventListener('DOMContentLoaded', main);
