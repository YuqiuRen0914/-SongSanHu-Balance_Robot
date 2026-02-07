// /assets/js/modules/joystick.js
import { state, domElements, CONSTANTS } from "../config.js";
import { sendWebSocketMessage } from "../services/websocket.js";

let joystickRadius, stickRadius, maxDisplacement;
const AXIS_SNAP_RATIO = 0.25; // 副轴必须超过主轴的比例才触发对角线
const AXIS_SNAP_MIN = 0.05; // 副轴绝对值低于该值直接锁为 0

function setStickPosition(px, py) {
  domElements.stick.style.left = `${px + joystickRadius}px`;
  domElements.stick.style.top = `${py + joystickRadius}px`;
}

function applyAxisSnap(px, py) {
  // 先归一化再按优势轴吸附，返回吸附后的 px/py 及归一化值
  const nx = px / maxDisplacement;
  const ny = -py / maxDisplacement; // Y 轴取负保持上推为正

  const absX = Math.abs(nx);
  const absY = Math.abs(ny);

  let lockedX = nx;
  let lockedY = ny;

  // 优势轴锁定：一轴明显更大时，另一轴被清零
  if (absY > AXIS_SNAP_MIN && absX < Math.max(AXIS_SNAP_MIN, absY * AXIS_SNAP_RATIO)) {
    lockedX = 0;
  } else if (absX > AXIS_SNAP_MIN && absY < Math.max(AXIS_SNAP_MIN, absX * AXIS_SNAP_RATIO)) {
    lockedY = 0;
  }

  const clampedX = Math.max(-1, Math.min(1, lockedX));
  const clampedY = Math.max(-1, Math.min(1, lockedY));

  return {
    px: clampedX * maxDisplacement,
    py: -clampedY * maxDisplacement,
    nx: clampedX,
    ny: clampedY,
  };
}

function updateJoystickReadout(nx, ny) {
  const r = Math.min(1, Math.hypot(nx, ny));
  const ang = (Math.atan2(ny, nx) * 180) / Math.PI;

  domElements.joyOut.textContent = `x: ${nx.toFixed(2)}  y: ${ny.toFixed(
    2
  )}  r: ${r.toFixed(2)}  θ: ${ang.toFixed(1)}°`;

  state.joystick.x = nx;
  state.joystick.y = ny;
  state.joystick.a = ang;
}

function getPositionFromEvent(e) {
  const rect = domElements.joystick.getBoundingClientRect();
  const centerX = rect.left + rect.width / 2;
  const centerY = rect.top + rect.height / 2;

  const clientX = e.touches ? e.touches[0].clientX : e.clientX;
  const clientY = e.touches ? e.touches[0].clientY : e.clientY;

  const x = clientX - centerX;
  const y = clientY - centerY;

  const distance = Math.hypot(x, y);
  if (distance <= maxDisplacement) {
    return { x, y };
  }

  // Clamp to the edge of the joystick area
  const k = maxDisplacement / distance;
  return { x: x * k, y: y * k };
}

function sendJoystickData() {
  sendWebSocketMessage({
    type: "joy",
    x: state.joystick.x,
    y: state.joystick.y,
    a: state.joystick.a,
  });
}

function handlePointerStart(e) {
  state.joystick.isDragging = true;
  const pos = getPositionFromEvent(e);
  const snapped = applyAxisSnap(pos.x, pos.y);
  setStickPosition(snapped.px, snapped.py);
  updateJoystickReadout(snapped.nx, snapped.ny);
  sendJoystickData();
}

function handlePointerMove(e) {
  if (!state.joystick.isDragging) return;
  e.preventDefault(); // Prevent page scrolling on touch devices
  const pos = getPositionFromEvent(e);
  const snapped = applyAxisSnap(pos.x, pos.y);
  setStickPosition(snapped.px, snapped.py);
  updateJoystickReadout(snapped.nx, snapped.ny);
}

function handlePointerEnd() {
  if (!state.joystick.isDragging) return;
  state.joystick.isDragging = false;
  setStickPosition(0, 0);
  updateJoystickReadout(0, 0);
  sendWebSocketMessage({ type: "joy", x: 0, y: 0, a: 0 });
}

export function initJoystick() {
  const joy = domElements.joystick;
  joystickRadius = joy.clientWidth / 2;
  stickRadius = domElements.stick.clientWidth / 2;
  maxDisplacement = joystickRadius - stickRadius;

  setStickPosition(0, 0);

  // Use pointer events for unified mouse and touch input
  joy.addEventListener("pointerdown", (e) => {
    joy.setPointerCapture(e.pointerId);
    handlePointerStart(e);
  });
  joy.addEventListener("pointermove", handlePointerMove);
  joy.addEventListener("pointerup", handlePointerEnd);
  joy.addEventListener("pointercancel", handlePointerEnd);
  joy.addEventListener("pointerleave", handlePointerEnd);

  // Throttled sending for performance
  setInterval(() => {
    if (!state.joystick.isDragging) return;
    const now = performance.now();
    if (now - state.joystick.lastSendTime < CONSTANTS.JOYSTICK_SEND_INTERVAL)
      return;
    state.joystick.lastSendTime = now;
    sendJoystickData();
  }, 20); // Check every 20ms, actual send rate is controlled by JOYSTICK_SEND_INTERVAL
}
