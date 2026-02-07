# ESP32车队控制系统使用说明

## 系统概述

本系统实现了基于ESP-NOW的平衡车车队控制功能，支持三种运行模式：
- **单机模式 (STANDALONE)**：独立运行，保持原有功能
- **头车模式 (LEADER)**：接收网页控制，通过ESP-NOW广播指令给从车
- **从车模式 (FOLLOWER)**：关闭PID控制，接收头车指令直接控制电机

## 🆕 WiFi识别与IP稳定性

### 自动WiFi命名
每辆车的WiFi热点名称自动添加MAC地址后缀，便于多车场景识别：
- **格式**：`BalBot_AABB`（最后4位MAC地址）
- **示例**：
  - 车1：`BalBot_EEFF` (MAC: AA:BB:CC:DD:EE:FF)
  - 车2：`BalBot_0A1B` (MAC: 11:22:33:44:0A:1B)
  - 车3：`BalBot_3C4D` (MAC: 55:66:77:88:3C:4D)

### IP地址持久化
- **首次启动**：生成随机IP（避免多车冲突）
- **后续启动**：IP地址保持不变（存储在NVS）
- **优势**：配置车队后重启无需重新查找IP地址

### 查看车辆信息
启动时串口会打印完整信息：
```
========== WiFi AP Started ==========
[WIFI] AP SSID: BalBot_EEFF
[WIFI] AP Password: 123456789
[WIFI] AP IP Address: 192.168.137.89
[WIFI] AP MAC Address: AA:BB:CC:DD:EE:FF
=====================================
```

## 通信架构

```
网页终端 (WebSocket) 
    ↓
头车 (PID控制 + ESP-NOW发送)
    ↓ (广播，<10ms延迟)
从车1, 从车2, ... (直接电机控制)
```

## 配置步骤

### 1. 头车配置

1. 烧录程序到头车ESP32
2. 查看串口输出，记录WiFi信息：
   - SSID（例：`BalBot_EEFF`）
   - IP地址（例：`192.168.137.89`）
   - MAC地址（例：`AA:BB:CC:DD:EE:FF`）
3. 连接头车WiFi热点
4. 打开网页界面（使用串口显示的IP地址）
5. 通过WebSocket发送配置命令：

```javascript
// 发送: 设置为头车
{
  "type": "group_config",
  "param": {
    "role": "leader",
    "group_id": 1,
    "espnow_enabled": true
  }
}

// 发送: 获取配置（查看MAC地址）
{
  "type": "get_group_config"
}

// 响应示例:
{
  "type": "group_config",
  "role": "leader",
  "my_mac": "AA:BB:CC:DD:EE:FF",  // 记录此MAC地址
  "group_id": 1,
  "espnow_enabled": true,
  "espnow_status": "ok"
}
```

5. **记录头车的MAC地址**（从响应中的`my_mac`字段获取，或从串口输出获取）
6. 重启头车使配置生效
7. **重要**：重启后IP地址保持不变，可继续使用相同地址访问

### 2. 从车配置

1. 烧录程序到从车ESP32
2. 查看串口输出，记录WiFi信息（SSID和IP地址）
3. 连接从车WiFi热点
4. 打开网页界面（使用串口显示的IP地址）
5. 通过WebSocket发送配置命令：

```javascript
// 发送: 设置为从车
{
  "type": "group_config",
  "param": {
    "role": "follower",
    "group_id": 1,
    "leader_mac": "AA:BB:CC:DD:EE:FF",  // 使用步骤1中记录的头车MAC
    "espnow_enabled": true
  }
}
```

5. 重启从车使配置生效
6. **重要**：重启后IP地址保持不变

### 3. 运行车队

1. **物理连接**：将车辆用连接件刚性连接（重要！）
2. **启动顺序**：
   - 先启动从车
   - 再启动头车
3. **控制**：
   - 只需连接头车的网页界面
   - 使用摇杆控制头车
   - 从车会自动跟随头车运动

## WebSocket API

### 设置车队配置

```json
{
  "type": "group_config",
  "param": {
    "role": "standalone|leader|follower",
    "leader_mac": "AA:BB:CC:DD:EE:FF",  // 仅从车需要
    "group_id": 0-255,
    "espnow_enabled": true
  }
}
```

### 获取车队配置

```json
// 请求
{
  "type": "get_group_config"
}

// 响应
{
  "type": "group_config",
  "role": "leader|follower|standalone",
  "my_mac": "AA:BB:CC:DD:EE:FF",
  "leader_mac": "AA:BB:CC:DD:EE:FF",
  "group_id": 1,
  "espnow_enabled": true,
  "espnow_status": "ok|error"
}
```

## 串口调试信息

启动时会打印类似以下信息：

```
========== WiFi AP Started ==========
[WIFI] AP IP loaded from NVS: 192.168.137.89
[WIFI] AP SSID: BalBot_EEFF
[WIFI] AP Password: 123456789
[WIFI] AP IP Address: 192.168.137.89
[WIFI] AP MAC Address: AA:BB:CC:DD:EE:FF
=====================================

[GROUP] Initializing group system...
[GROUP] Configuration loaded from NVS
[GROUP] Role: LEADER
[GROUP] Group ID: 1
[GROUP] ESP-NOW enabled: YES
[GROUP] WiFi mode: AP (Leader/Standalone)
[GROUP] Leader mode: broadcast peer added
[GROUP] ESP-NOW initialized successfully
[GROUP] My MAC: AA:BB:CC:DD:EE:FF
```

**首次启动会显示**：
```
[WIFI] New random AP IP generated: 192.168.137.89
[WIFI] AP IP saved: 192.168.137.89
```

**后续启动会显示**：
```
[WIFI] AP IP loaded from NVS: 192.168.137.89
```

## 故障排查

### 从车不响应

1. 检查头车MAC地址是否正确配置
2. 检查`group_id`是否一致
3. 检查串口输出的`espnow_status`
4. 确认从车串口显示 "Follower mode: leader MAC XX:XX:..."

### 从车超时停车

- 从车会在500ms内未收到指令时自动停车
- 检查头车是否正常运行
- 检查两车之间距离（ESP-NOW有效范围约100米）

### WiFi名称相同

- 每辆车的SSID自动添加MAC后缀，不会重复
- 如果看到相同SSID，检查是否使用了旧版本固件

### IP地址变化

- IP地址在首次启动后会持久化保存
- 重启后IP保持不变
- 如需重置IP，可通过PlatformIO执行 `pio run -t erase` 清除NVS

### 无法连接WiFi

- 检查SSID和密码是否正确
- 查看串口输出确认WiFi是否成功启动
- 尝试重启ESP32

## 技术参数

- **通信协议**：ESP-NOW
- **通信延迟**：<10ms
- **控制频率**：500Hz (2ms周期)
- **超时保护**：500ms
- **最大从车数**：20辆（ESP-NOW限制）
- **有效距离**：约100米（无障碍物）

## 安全注意事项

1. **物理连接**：车辆必须刚性连接，否则可能发生碰撞
2. **PID关闭**：从车已关闭所有PID控制，完全依赖头车指令
3. **摔倒保护**：从车保留摔倒检测，倾倒时会自动停车
4. **超时保护**：通信中断500ms后从车自动停车
5. **启动顺序**：建议先启动从车，再启动头车

## 恢复单机模式

如需恢复单机模式：

```javascript
{
  "type": "group_config",
  "param": {
    "role": "standalone",
    "espnow_enabled": false
  }
}
```

重启后即可独立运行。

## 配置持久化

所有配置保存在NVS（非易失性存储）中，断电后不会丢失。如需完全重置，可通过PlatformIO执行：

```bash
pio run -t erase
```

注意：这会清除所有配置，包括WiFi设置和PID参数。

