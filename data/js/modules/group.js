/**
 * 车队配置模块
 * 负责处理车队配置UI、WebSocket通信和状态显示
 */

import { sendWebSocketMessage } from '../services/websocket.js';

// 车队状态
const groupState = {
  role: 'standalone',
  myMac: '',
  leaderMac: '',
  groupId: 0,
  espnowEnabled: false,
  espnowStatus: 'unknown',
  followersOnline: [] // 从车在线列表
};

// UI元素
let elements = {};

/**
 * 初始化车队模块
 */
export function initGroup() {
  const success = cacheElements();
  if (!success) {
    return;
  }
  bindEvents();
  requestGroupConfig();
}

/**
 * 缓存DOM元素
 */
function cacheElements() {
  elements = {
    // 车队配置面板
    groupCard: document.getElementById('groupCard'),
    roleSelect: document.getElementById('groupRole'),
    groupIdInput: document.getElementById('groupId'),
    leaderMacInput: document.getElementById('leaderMac'),
    espnowSwitch: document.getElementById('espnowSwitch'),
    myMacDisplay: document.getElementById('myMac'),
    groupSaveBtn: document.getElementById('btnSaveGroup'),
    groupRefreshBtn: document.getElementById('btnRefreshGroup'),
    
    // 状态显示（注意：没有groupStatus元素，直接使用其他元素）
    roleDisplay: document.getElementById('roleDisplay'),
    espnowStatusLamp: document.getElementById('espnowStatusLamp'),
    espnowStatusText: document.getElementById('espnowStatusText'),
    followersCount: document.getElementById('followersCount'),
    followersList: document.getElementById('followersList')
  };
  
  // 如果元素不存在（老版本HTML），则不初始化
  if (!elements.groupCard) {
    console.warn('[GROUP] Group UI elements not found, skipping initialization');
    return false;
  }
  
  return true;
}

/**
 * 绑定事件
 */
function bindEvents() {
  if (!elements.groupSaveBtn) return;
  
  elements.groupSaveBtn.addEventListener('click', saveGroupConfig);
  elements.groupRefreshBtn.addEventListener('click', requestGroupConfig);
  elements.roleSelect.addEventListener('change', onRoleChange);
}

/**
 * 角色变化时显示/隐藏相关字段
 */
function onRoleChange() {
  const role = elements.roleSelect.value;
  const leaderMacField = document.getElementById('leaderMacField');
  
  if (leaderMacField) {
    if (role === 'follower') {
      leaderMacField.style.display = 'block';
    } else {
      leaderMacField.style.display = 'none';
    }
  }
}

/**
 * 请求当前车队配置
 */
function requestGroupConfig() {
  sendWebSocketMessage({ type: 'get_group_config' });
}

/**
 * 保存车队配置
 */
function saveGroupConfig() {
  const role = elements.roleSelect.value;
  const groupId = parseInt(elements.groupIdInput.value) || 0;
  const leaderMac = elements.leaderMacInput.value.trim();
  const espnowEnabled = elements.espnowSwitch.checked;
  
  // 验证
  if (role === 'follower' && !validateMacAddress(leaderMac)) {
    alert('请输入有效的头车MAC地址 (格式: AA:BB:CC:DD:EE:FF)');
    return;
  }
  
  const config = {
    type: 'group_config',
    param: {
      role: role,
      group_id: groupId,
      espnow_enabled: espnowEnabled ? 1 : 0  // 确保发送数字
    }
  };
  
  if (role === 'follower') {
    config.param.leader_mac = leaderMac.toUpperCase();
  }
  
  sendWebSocketMessage(config);
  
  // 提示用户重启
  alert('配置已保存！\n\n请重启ESP32使配置生效。');
}

/**
 * 验证MAC地址格式
 */
function validateMacAddress(mac) {
  const macRegex = /^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/;
  return macRegex.test(mac);
}

/**
 * 处理车队配置响应
 */
export function handleGroupConfig(data) {
  // 更新状态
  groupState.role = data.role || 'standalone';
  groupState.myMac = data.my_mac || '';
  groupState.leaderMac = data.leader_mac || '';
  groupState.groupId = data.group_id || 0;
  groupState.espnowEnabled = data.espnow_enabled || false;
  groupState.espnowStatus = data.espnow_status || 'unknown';
  
  // 更新UI
  updateUI();
}

/**
 * 更新UI显示
 */
function updateUI() {
  if (!elements.groupCard) {
    return;
  }
  
  // 更新配置表单
  elements.roleSelect.value = groupState.role;
  elements.groupIdInput.value = groupState.groupId;
  elements.leaderMacInput.value = groupState.leaderMac;
  elements.espnowSwitch.checked = groupState.espnowEnabled;
  elements.myMacDisplay.textContent = groupState.myMac || '未知';
  
  // 根据角色显示/隐藏字段
  onRoleChange();
  
  // 更新状态显示
  updateStatusDisplay();
}

/**
 * 更新状态显示区域
 */
function updateStatusDisplay() {
  if (!elements.roleDisplay) {
    return;
  }
  
  // 角色显示
  const roleNames = {
    'standalone': '单机模式',
    'leader': '头车',
    'follower': '从车'
  };
  elements.roleDisplay.textContent = roleNames[groupState.role] || '未知';
  
  // ESP-NOW状态
  if (elements.espnowStatusLamp && elements.espnowStatusText) {
    const statusOk = groupState.espnowStatus === 'ok';
    elements.espnowStatusLamp.className = 'bulb ' + (statusOk ? 'on' : 'off');
    elements.espnowStatusText.textContent = statusOk ? '正常' : '未启用';
  }
  
  // 从车数量（仅头车显示）
  if (elements.followersCount) {
    if (groupState.role === 'leader') {
      elements.followersCount.style.display = 'block';
      updateFollowersList();
    } else {
      elements.followersCount.style.display = 'none';
    }
  }
}

/**
 * 处理车队状态更新（遥测数据）
 */
export function handleGroupStatus(data) {
  if (!data.group_status) return;
  
  const status = data.group_status;
  
  // 更新从车在线列表
  if (status.followers) {
    groupState.followersOnline = status.followers;
    updateFollowersList();
  }
  
  // 更新其他状态
  if (status.espnow_status) {
    groupState.espnowStatus = status.espnow_status;
    updateStatusDisplay();
  }
}

/**
 * 更新从车列表显示
 */
function updateFollowersList() {
  if (!elements.followersList) return;
  
  const count = groupState.followersOnline.length;
  elements.followersCount.querySelector('.count').textContent = count;
  
  if (count === 0) {
    elements.followersList.innerHTML = '<div class="no-followers">无从车在线</div>';
    return;
  }
  
  let html = '<div class="followers-grid">';
  groupState.followersOnline.forEach((follower, index) => {
    const lastSeen = Date.now() - follower.last_seen_ms;
    const isOnline = lastSeen < 2000; // 2秒内视为在线
    html += `
      <div class="follower-item ${isOnline ? 'online' : 'offline'}">
        <div class="follower-icon">🚗</div>
        <div class="follower-info">
          <div class="follower-name">从车 #${index + 1}</div>
          <div class="follower-mac">${follower.mac}</div>
          <div class="follower-status">${isOnline ? '在线' : '离线'}</div>
        </div>
      </div>
    `;
  });
  html += '</div>';
  
  elements.followersList.innerHTML = html;
}

/**
 * 获取当前车队状态（供其他模块使用）
 */
export function getGroupState() {
  return { ...groupState };
}

/**
 * 是否为头车
 */
export function isLeader() {
  return groupState.role === 'leader';
}

/**
 * 是否为从车
 */
export function isFollower() {
  return groupState.role === 'follower';
}

