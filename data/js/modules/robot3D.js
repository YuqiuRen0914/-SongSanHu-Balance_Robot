// /assets/js/modules/robot3D.js
import { state } from '../config.js';
import { domElements } from '../config.js';
import { appendLog } from '../ui.js';

// --- Private Helper Functions ---
const createMaterial = (hex, metalness = 0.1, roughness = 0.6) => new THREE.MeshStandardMaterial({ color: hex, metalness, roughness });
const createBox = (w, h, d, material) => new THREE.Mesh(new THREE.BoxGeometry(w, h, d), material);
const createCylinder = (rT, rB, h, material, segments = 36) => new THREE.Mesh(new THREE.CylinderGeometry(rT, rB, h, segments), material);

function createMainWheel(radius = 0.18, width = 0.08) {
  const group = new THREE.Group();
  const tireMat = createMaterial(0x2a2d31, 0.25, 0.4);
  const rimMat = createMaterial(0x606a76, 0.25, 0.55);
  const hubMat = createMaterial(0x9aa3ad, 0.2, 0.4);

  const tire = createCylinder(radius, radius, width, tireMat);
  tire.rotation.z = Math.PI / 2;
  group.add(tire);

  const rim = createCylinder(radius * 0.82, radius * 0.82, width * 0.55, rimMat);
  rim.rotation.z = Math.PI / 2;
  group.add(rim);

  const hub = createCylinder(radius * 0.28, radius * 0.28, width * 0.7, hubMat, 24);
  hub.rotation.z = Math.PI / 2;
  group.add(hub);

  return group;
}

function createChassis() {
  const body = new THREE.Group();
  const pcb = createBox(0.90, 0.02, 0.44, createMaterial(0x1f7a42, 0.15, 0.5));
  const base = createBox(1.05, 0.10, 0.52, createMaterial(0x3a3f46, 0.3, 0.5));
  base.position.y = 0.35;
  body.add(base);

  const mid = createBox(1.00, 0.40, 0.50, createMaterial(0x5a6069, 0.2, 0.6));
  mid.position.y = 0.60;
  body.add(mid);

  const lid = createBox(0.98, 0.06, 0.48, createMaterial(0xd7dbe2, 0.1, 0.9));
  lid.position.y = 0.83;
  body.add(lid);

  pcb.position.y = 0.87;
  body.add(pcb);
  
  return { body, pcb };
}


// --- Public API ---

/**
 * 初始化3D场景
 */
export function init3D() {
  if (typeof THREE === 'undefined') {
    appendLog('[WARN] Three.js not loaded, 3D view disabled.');
    return;
  }
  
  const host = domElements.robotCanvas;
  host.innerHTML = "";

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0xf6f7fb);

  const w = host.clientWidth;
  const h = 360;
  const camera = new THREE.PerspectiveCamera(40, w / h, 0.1, 100);
  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(w, h);
  renderer.setPixelRatio(window.devicePixelRatio || 1);
  host.appendChild(renderer.domElement);
  
  // Lighting & Grid
  scene.add(new THREE.HemisphereLight(0xffffff, 0x8899aa, 0.95));
  const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
  dirLight.position.set(3, 5, 2);
  scene.add(dirLight);
  const grid = new THREE.GridHelper(12, 24, 0xcccccc, 0xeeeeee);
  grid.position.y = 0;
  scene.add(grid);

  // Robot Model
  const robot = new THREE.Group();
  scene.add(robot);
  const { body, pcb } = createChassis();
  robot.add(body);
  const wheelL = createMainWheel(0.18, 0.08);
  wheelL.position.set(-0.42, 0.16, 0);
  robot.add(wheelL);
  const wheelR = createMainWheel(0.18, 0.08);
  wheelR.position.set(0.42, 0.16, 0);
  robot.add(wheelR);

  // Orbit Controls Logic
  const orbit = { target: new THREE.Vector3(0, 0.7, 0), r: 5.0, theta: 0.8, phi: 1.0, minR: 2, maxR: 10 };
  
  function applyOrbit() {
    const { r, theta, phi, target } = orbit;
    const p = new THREE.Vector3(
        r * Math.sin(phi) * Math.cos(theta),
        r * Math.cos(phi),
        r * Math.sin(phi) * Math.sin(theta)
    );
    camera.position.copy(target.clone().add(p));
    camera.lookAt(target);
  }
  applyOrbit();

  // Pointer event listeners for orbit control
  const el = renderer.domElement;
  let isDragging = false, button = 0, lastX = 0, lastY = 0;
  el.addEventListener('pointerdown', e => { isDragging = true; button = e.button; lastX = e.clientX; lastY = e.clientY; el.setPointerCapture(e.pointerId); });
  el.addEventListener('pointerup', e => { isDragging = false; el.releasePointerCapture(e.pointerId); });
  el.addEventListener('pointerleave', () => { isDragging = false; });
  el.addEventListener('contextmenu', e => e.preventDefault());
  el.addEventListener('pointermove', e => {
      if (!isDragging) return;
      const dx = e.clientX - lastX, dy = e.clientY - lastY;
      lastX = e.clientX; lastY = e.clientY;
      const rotSpeed = 0.005, panSpeed = 0.002 * orbit.r;
      if (button === 2 || e.ctrlKey) {
          const right = new THREE.Vector3();
          camera.getWorldDirection(right);
          right.cross(camera.up).normalize();
          const up = camera.up.clone().normalize();
          orbit.target.addScaledVector(right, -dx * panSpeed);
          orbit.target.addScaledVector(up, dy * panSpeed);
      } else {
          orbit.theta -= dx * rotSpeed;
          orbit.phi -= dy * rotSpeed;
          const eps = 0.05;
          orbit.phi = Math.max(eps, Math.min(Math.PI - eps, orbit.phi));
      }
      applyOrbit();
  });
  el.addEventListener('wheel', e => {
      e.preventDefault();
      const k = Math.pow(1.1, e.deltaY > 0 ? 1 : -1);
      orbit.r = Math.max(orbit.minR, Math.min(orbit.maxR, orbit.r * k));
      applyOrbit();
  }, { passive: false });

  // Window resize handler
  window.addEventListener('resize', () => {
    const newWidth = host.clientWidth;
    camera.aspect = newWidth / h;
    camera.updateProjectionMatrix();
    renderer.setSize(newWidth, h);
  });


  // Store references in state
  state.three = { scene, camera, renderer, robot, pcb };
  
  setAttitude(0, 0, 0);
  renderer.setAnimationLoop(() => renderer.render(scene, camera));
  appendLog('[INIT] 3D ready');
}

/**
 * 更新3D模型的姿态
 * @param {number} pitchDeg 
 * @param {number} rollDeg 
 * @param {number} yawDeg 
 */
export function setAttitude(pitchDeg, rollDeg, yawDeg) {
  const t = state.three;
  if (!t.robot) return;
  const toRad = Math.PI / 180;
  
  const rollAdj = rollDeg - state.attitudeZero.roll;
  const yawAdj = yawDeg - state.attitudeZero.yaw;

  t.robot.rotation.x = pitchDeg * toRad;
  t.robot.rotation.z = rollAdj * toRad;
  t.robot.rotation.y = yawAdj * toRad;

  domElements.attOut.textContent = `pitch: ${pitchDeg.toFixed(1)}°  roll: ${rollAdj.toFixed(1)}°  yaw: ${yawAdj.toFixed(1)}°`;
}