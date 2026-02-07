# WiFi识别与IP稳定性优化更新说明

## 更新概述

本次更新解决了多车场景下的两个关键问题：
1. ✅ **WiFi名称冲突** - 所有车SSID相同无法区分
2. ✅ **IP地址变化** - 重启后IP改变导致需要重新查找

## 新功能

### 1. 自动WiFi命名（MAC后缀）

**功能说明**
- 每辆车的WiFi热点自动添加MAC地址后4位作为后缀
- 格式：`BalBot_AABB`（AABB为MAC地址最后2字节）
- 完全自动，无需手动配置

**示例**
```
车1 MAC: AA:BB:CC:DD:EE:FF → SSID: BalBot_EEFF
车2 MAC: 11:22:33:44:0A:1B → SSID: BalBot_0A1B  
车3 MAC: 55:66:77:88:3C:4D → SSID: BalBot_3C4D
```

**优势**
- 多车同时启动，每辆车WiFi名称各不相同
- 用户可通过SSID快速识别车辆
- MAC后缀便于记录和管理

### 2. IP地址持久化

**功能说明**
- 首次启动：生成随机IP地址（避免多车网段冲突）
- 保存到NVS：IP地址持久化存储
- 后续启动：从NVS读取，保持不变

**工作流程**
```
首次启动:
  生成随机IP → 保存到NVS → 使用该IP

后续启动:
  从NVS读取IP → 使用相同IP
```

**优势**
- 配置车队后重启，无需重新查找IP
- 每辆车IP稳定，便于记录和访问
- 避免多车同时启动时的IP冲突

## 使用方法

### 查看车辆信息

启动ESP32后，查看串口输出：

```
========== WiFi AP Started ==========
[WIFI] AP SSID: BalBot_EEFF
[WIFI] AP Password: 123456789
[WIFI] AP IP Address: 192.168.137.89
[WIFI] AP MAC Address: AA:BB:CC:DD:EE:FF
=====================================
```

记录以下信息：
- **SSID**：用于连接WiFi
- **IP地址**：用于访问网页
- **MAC地址**：用于配置车队

### 多车场景操作流程

#### 步骤1：启动所有车辆
```
车1启动 → BalBot_EEFF (192.168.137.89)
车2启动 → BalBot_0A1B (192.168.201.45)
车3启动 → BalBot_3C4D (192.168.89.123)
```

#### 步骤2：配置头车
1. 连接 `BalBot_EEFF`
2. 访问 `http://192.168.137.89`
3. 配置为头车，记录MAC地址
4. 重启

#### 步骤3：配置从车
1. 连接 `BalBot_0A1B`
2. 访问 `http://192.168.201.45`（IP不变）
3. 配置为从车，输入头车MAC
4. 重启

#### 步骤4：验证
- 头车IP仍为 `192.168.137.89`
- 从车IP仍为 `192.168.201.45`
- 无需重新查找IP地址

## 技术实现

### SSID生成

```cpp
String generate_default_ssid() {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char ssid[33];
    snprintf(ssid, sizeof(ssid), "%s_%02X%02X", 
             SSID_PREFIX, mac[4], mac[5]);
    return String(ssid);
}
```

### IP持久化

```cpp
// 保存IP到NVS
void save_persistent_ip(IPAddress ip) {
    Preferences pref;
    if (pref.begin("wifi_cfg", false)) {
        pref.putUInt("ap_ip", (uint32_t)ip);
        pref.end();
    }
}

// 从NVS加载IP
IPAddress load_persistent_ip() {
    Preferences pref;
    if (pref.begin("wifi_cfg", true)) {
        uint32_t ip_uint = pref.getUInt("ap_ip", 0);
        pref.end();
        if (ip_uint != 0) {
            return IPAddress(ip_uint);
        }
    }
    // 首次启动，生成并保存
    IPAddress new_ip = make_random_ap_ip();
    save_persistent_ip(new_ip);
    return new_ip;
}
```

## 串口输出示例

### 首次启动
```
[WIFI] New random AP IP generated: 192.168.137.89
[WIFI] AP IP saved: 192.168.137.89
========== WiFi AP Started ==========
[WIFI] AP SSID: BalBot_EEFF
[WIFI] AP Password: 123456789
[WIFI] AP IP Address: 192.168.137.89
[WIFI] AP MAC Address: AA:BB:CC:DD:EE:FF
=====================================
```

### 后续启动
```
[WIFI] AP IP loaded from NVS: 192.168.137.89
========== WiFi AP Started ==========
[WIFI] AP SSID: BalBot_EEFF
[WIFI] AP Password: 123456789
[WIFI] AP IP Address: 192.168.137.89
[WIFI] AP MAC Address: AA:BB:CC:DD:EE:FF
=====================================
```

## 配置文件变更

### include/my_config.h
```cpp
// 新增SSID前缀定义
#define SSID_PREFIX "BalBot"  // 后面自动加MAC后缀
#define SSID "Balance_Robot"  // 兼容旧代码
#define PASSWORD "123456789"
```

### src/my_net_lib/my_wifi.cpp
- 新增 `generate_default_ssid()` 函数
- 新增 `save_persistent_ip()` 函数
- 新增 `load_persistent_ip()` 函数
- 修改 `load_saved_wifi()` 优先使用带MAC后缀的SSID
- 修改 `my_wifi_init()` 使用持久化IP

## 用户自定义

### 修改SSID
用户仍可通过网页界面修改SSID：
1. 访问WiFi配置页面
2. 输入自定义SSID
3. 保存并重启

修改后的SSID会覆盖默认的MAC后缀版本。

### 重置IP地址
如需生成新的随机IP：
```bash
pio run -t erase  # 清除所有NVS数据
pio run -t upload # 重新上传固件
```

**注意**：这会清除所有配置，包括WiFi设置和车队配置。

## 兼容性

- ✅ 向后兼容：现有功能保持不变
- ✅ 用户可自定义SSID（覆盖默认）
- ✅ 用户可自定义密码
- ✅ 不影响车队控制功能

## 故障排查

### 问题1：SSID显示为旧格式
**原因**：用户之前保存过自定义SSID  
**解决**：
1. 通过网页清空SSID设置
2. 或执行 `pio run -t erase` 清除NVS
3. 重启后将使用新的MAC后缀格式

### 问题2：IP地址仍在变化
**原因**：NVS未正确保存  
**解决**：
1. 检查串口是否显示 "AP IP saved"
2. 确认NVS分区正常
3. 尝试重新烧录固件

### 问题3：多车IP冲突
**原因**：极低概率的随机冲突  
**解决**：
1. 重置其中一辆车的IP（清除NVS）
2. 或通过网页手动配置IP（未来功能）

## 测试建议

### 单车测试
1. 全新烧录固件
2. 检查SSID格式是否为 `BalBot_XXXX`
3. 记录IP地址
4. 重启3次，验证IP不变

### 多车测试
1. 同时启动3辆车
2. 验证SSID各不相同
3. 验证IP不冲突
4. 分别配置车队
5. 重启后验证IP稳定

### 配置测试
1. 配置头车和从车
2. 重启所有车辆
3. 验证无需重新查找IP
4. 验证车队功能正常

## 总结

本次更新显著提升了多车场景下的用户体验：

✅ **即插即用**：每辆车自动有独特WiFi名称  
✅ **IP稳定**：配置后重启无需重新查找  
✅ **避免冲突**：多车同时启动互不干扰  
✅ **用户友好**：MAC后缀便于识别和记录  
✅ **向后兼容**：现有功能保持不变  

所有功能已完整实现并可立即使用！

