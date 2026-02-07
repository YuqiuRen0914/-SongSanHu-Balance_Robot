// /assets/js/modules/pitchZero.js
import { state, domElements } from "../config.js";
import { sendWebSocketMessage } from "../services/websocket.js";
import { appendLog } from "../ui.js";

/**
 * 初始化pitch零点控制
 */
export function initPitchZero() {
  const { pitchZeroValue, pitchZeroRange, btnPitchZeroSend } = domElements;
  
  if (!pitchZeroValue || !pitchZeroRange || !btnPitchZeroSend) return;
  
  // 同步滑块和显示值
  const syncControls = (value) => {
    const clamped = Math.max(-5, Math.min(5, parseFloat(value) || 0));
    pitchZeroValue.textContent = clamped.toFixed(1);
    pitchZeroRange.value = clamped;
    state.pitchZero = clamped;
  };
  
  // 滑块变化
  pitchZeroRange.addEventListener("input", () => {
    syncControls(pitchZeroRange.value);
  });
  
  // 发送按钮
  btnPitchZeroSend.onclick = () => {
    sendWebSocketMessage({ 
      type: "pitch_zero_set", 
      value: state.pitchZero 
    });
    appendLog(`[SAVE] pitch_zero=${state.pitchZero.toFixed(1)}°`);
  };
  
  // 请求当前值
  sendWebSocketMessage({ type: "get_pitch_zero" });
}

/**
 * 更新pitch零点显示
 */
export function updatePitchZero(value) {
  const { pitchZeroValue, pitchZeroRange } = domElements;
  
  if (!pitchZeroValue || !pitchZeroRange) return;
  
  const clamped = Math.max(-5, Math.min(5, parseFloat(value) || 0));
  pitchZeroValue.textContent = clamped.toFixed(1);
  pitchZeroRange.value = clamped;
  state.pitchZero = clamped;
}

