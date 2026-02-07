# 车队控制系统 - 前端界面与状态监控更新

## 更新概述

本次更新为ESP32车队控制系统添加了完整的前端网页界面和实时状态监控功能，包括：

1. ✅ 车队配置网页界面
2. ✅ 从车心跳机制
3. ✅ 实时在线状态监控
4. ✅ 车队状态遥测数据

## 新增功能

### 1. 前端车队配置界面

新增了独立的车队配置卡片，位于WiFi配置下方，包含以下功能：

**配置选项：**
- 本机MAC地址显示（自动获取）
- 车辆角色选择（单机/头车/从车）
- 车队ID设置（0-255）
- 头车MAC地址输入（从车模式）
- ESP-NOW通信开关

**状态显示：**
- 当前角色指示
- ESP-NOW状态灯（绿色=正常，灰色=未启用）
- 从车在线数量（仅头车显示）
- 从车列表实时显示

### 2. 从车心跳机制

**实现细节：**
- 从车每1秒向头车发送心跳包
- 心跳包含：MAC地址、车队ID、时间戳、电池电量
- 数据校验和保护
- 自动重连机制

**超时处理：**
- 3秒无心跳视为离线
- 自动更新在线状态
- 串口日志记录离线事件

### 3. 实时状态监控

**头车界面显示：**
```
从车在线: 2

🚗 从车 #1
   AA:BB:CC:DD:EE:01
   在线 (绿色边框)

🚗 从车 #2
   AA:BB:CC:DD:EE:02
   在线 (绿色边框)
```

**离线显示：**
- 红色边框
- 半透明显示
- 状态文字显示"离线"

### 4. WebSocket遥测扩展

在现有遥测数据中新增 `group_status` 字段：

```json
{
  "type": "telemetry",
  "fallen": false,
  "pitch": 0.5,
  "roll": 0.1,
  "yaw": 0.0,
  "battery": 12.5,
  "group_status": {
    "espnow_status": "ok",
    "followers": [
      {
        "mac": "AA:BB:CC:DD:EE:01",
        "last_seen_ms": 150
      },
      {
        "mac": "AA:BB:CC:DD:EE:02",
        "last_seen_ms": 320
      }
    ]
  }
}
```

## 文件清单

### 后端文件（已更新）

| 文件 | 说明 |
|------|------|
| `include/my_group.h` | 新增心跳结构和从车追踪接口 |
| `src/my_motion_lib/my_group.cpp` | 实现心跳发送/接收、从车列表管理 |
| `src/my_motion_lib/my_motion.cpp` | 集成心跳发送和状态更新 |
| `src/my_net_lib/my_web_bridge.cpp` | 遥测数据中添加车队状态 |

### 前端文件（新增/更新）

| 文件 | 说明 |
|------|------|
| `data/js/modules/group.js` | **新增** 车队配置模块 |
| `data/home.html` | 添加车队配置面板HTML |
| `data/css/main.css` | 添加车队相关样式 |
| `data/js/main.js` | 集成车队模块初始化 |
| `data/js/services/websocket.js` | 添加车队消息处理 |

## 使用说明

### 头车配置流程

1. 打开头车网页界面
2. 滚动到"车队配置"卡片
3. 记录显示的"本机MAC地址"
4. 选择角色：**头车（接收网页控制）**
5. 设置车队ID（例如：1）
6. 勾选"启用ESP-NOW通信"
7. 点击"保存配置"
8. 重启ESP32

### 从车配置流程

1. 打开从车网页界面
2. 滚动到"车队配置"卡片
3. 选择角色：**从车（跟随头车运动）**
4. 输入头车MAC地址（从步骤3记录的地址）
5. 设置相同的车队ID（例如：1）
6. 勾选"启用ESP-NOW通信"
7. 点击"保存配置"
8. 重启ESP32

### 监控车队状态

配置完成后，在头车网页界面可以看到：

1. **ESP-NOW状态灯**
   - 🟢 绿色 = 通信正常
   - ⚫ 灰色 = 未启用

2. **从车在线数量**
   - 实时显示当前在线的从车数量

3. **从车详细列表**
   - 每辆从车的MAC地址
   - 在线/离线状态
   - 视觉指示（绿色边框=在线，红色边框=离线）

## 技术特性

### 性能参数

- **心跳频率**: 1Hz（每秒1次）
- **超时时间**: 3秒
- **延迟**: <10ms（ESP-NOW通信）
- **最大从车数**: 10辆（可在代码中调整）

### 数据流

```
从车 → 心跳包(1Hz) → 头车
                      ↓
                  追踪列表更新
                      ↓
                  WebSocket遥测
                      ↓
                  前端界面更新
```

### 安全特性

- ✅ 数据校验和验证
- ✅ 车队ID隔离
- ✅ 超时自动离线
- ✅ MAC地址验证

## 界面截图说明

### 车队配置面板

```
┌─────────────────────────────────────────┐
│ 车队控制系统                              │
│ 车队配置                                  │
│ 配置头车/从车模式，组建车队协同控制        │
│                                          │
│ 角色: 头车    ESP-NOW: 🟢 正常           │
├─────────────────────────────────────────┤
│ 本机MAC地址                              │
│ AA:BB:CC:DD:EE:FF                       │
│                                          │
│ 车辆角色: [头车（接收网页控制）▼]        │
│ 车队ID: [1]                             │
│ 启用ESP-NOW通信: [✓]                    │
│                                          │
│ [保存配置] [刷新状态]                    │
│ 保存后需要重启ESP32才能生效              │
├─────────────────────────────────────────┤
│ 从车在线: 2                              │
│                                          │
│ 🚗 从车 #1        🚗 从车 #2            │
│    AA:BB:CC:DD:EE:01  AA:BB:CC:DD:EE:02│
│    在线              在线                │
└─────────────────────────────────────────┘
```

## 调试信息

### 串口输出示例

**头车启动：**
```
[GROUP] Initializing group system...
[GROUP] Role: LEADER
[GROUP] Group ID: 1
[GROUP] ESP-NOW enabled: YES
[GROUP] ESP-NOW initialized successfully
[GROUP] My MAC: AA:BB:CC:DD:EE:FF
```

**接收到从车心跳：**
```
（无日志，静默接收）
```

**从车离线：**
```
[GROUP] Follower offline: AA:BB:CC:DD:EE:01
```

**从车启动：**
```
[GROUP] Initializing group system...
[GROUP] Role: FOLLOWER
[GROUP] Group ID: 1
[GROUP] Follower mode: leader MAC AA:BB:CC:DD:EE:FF
[GROUP] ESP-NOW initialized successfully
```

## 故障排查

### 从车不显示在列表中

1. 检查车队ID是否一致
2. 检查头车MAC地址是否正确
3. 查看从车串口是否显示"ESP-NOW initialized successfully"
4. 确认两车距离在100米内

### ESP-NOW状态灯显示灰色

1. 检查是否勾选"启用ESP-NOW通信"
2. 重启ESP32使配置生效
3. 查看串口是否有错误信息

### 从车显示离线但实际在运行

1. 检查从车是否正常发送心跳（查看串口）
2. 检查网络干扰
3. 尝试减少两车距离

## API参考

### WebSocket消息

**获取车队配置：**
```javascript
ws.send(JSON.stringify({
  type: "get_group_config"
}));
```

**设置车队配置：**
```javascript
ws.send(JSON.stringify({
  type: "group_config",
  param: {
    role: "leader",
    group_id: 1,
    espnow_enabled: true
  }
}));
```

**响应：**
```javascript
{
  type: "group_config",
  role: "leader",
  my_mac: "AA:BB:CC:DD:EE:FF",
  leader_mac: "FF:FF:FF:FF:FF:FF",
  group_id: 1,
  espnow_enabled: true,
  espnow_status: "ok"
}
```

## 下一步优化建议

1. **电池电量显示**：在从车列表中显示各从车电池电量
2. **信号强度**：显示ESP-NOW信号强度（RSSI）
3. **历史记录**：记录从车连接/断开历史
4. **报警功能**：从车离线时发出声音/视觉警告
5. **批量配置**：支持一键配置多辆从车

## 兼容性

- ✅ 向后兼容单机模式
- ✅ 不影响现有PID控制
- ✅ 不影响现有网页功能
- ✅ 可随时切换回单机模式

## 总结

本次更新完整实现了车队控制系统的前端界面和状态监控功能，为用户提供了：

- 🎨 直观的配置界面
- 📊 实时状态监控
- 🔄 自动心跳机制
- 🚨 离线检测与提示

所有功能已完整实现并可投入使用！

