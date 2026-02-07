# ESP32系统重启功能说明

## 功能概述

在网页控制台添加了**系统重启**按钮，允许用户通过网页界面远程重启ESP32控制器，无需物理操作设备。

## 技术实现

### 1. 前端实现

#### 1.1 用户界面（`data/home.html`）

在工具栏中添加了重启按钮：

```html
<button class="btn ghost" id="btnSystemRestart" title="重启ESP32控制器">系统重启</button>
```

- **位置**：顶部工具栏，防摔保护开关和连接状态之间
- **样式**：使用`ghost`样式（次要按钮），避免误触
- **提示**：鼠标悬停显示"重启ESP32控制器"

#### 1.2 DOM元素注册（`data/js/config.js`）

```javascript
domElements: {
  // ...
  btnSystemRestart: getElement("btnSystemRestart"),
}
```

#### 1.3 事件处理（`data/js/ui.js`）

```javascript
domElements.btnSystemRestart.onclick = () => {
  if (confirm("确认要重启ESP32控制器吗？\n\n重启后需要等待约10秒重新连接。")) {
    sendWebSocketMessage({ type: "system_restart" });
    appendLog("[SEND] system_restart - ESP32 正在重启...");
    setStatus("系统重启中...");
    // 5秒后尝试重新连接
    setTimeout(() => {
      appendLog("[INFO] 尝试重新连接...");
      location.reload();
    }, 5000);
  }
};
```

**安全机制**：
- ✅ 二次确认对话框，防止误操作
- ✅ 清晰的提示信息
- ✅ 5秒后自动刷新页面重新连接
- ✅ 状态提示更新

### 2. 后端实现（`src/my_net_lib/my_web.cpp`）

在WebSocket消息处理中添加：

```cpp
// 11) 系统重启
else if (!strcmp(typeStr, "system_restart"))
{
    Serial.println("[WEB] System restart requested");
    // 发送确认消息给客户端
    JsonDocument resp;
    resp["type"] = "info";
    resp["text"] = "ESP32正在重启...";
    wsSendTo(c, resp);
    // 延迟100ms让消息发送完成，然后重启
    delay(100);
    ESP.restart();
}
```

**工作流程**：
1. 接收到`system_restart`消息
2. 打印调试日志到串口
3. 发送确认消息给客户端
4. 延迟100ms确保消息发送完成
5. 调用`ESP.restart()`执行软件重启

## ESP.restart() API说明

### 函数原型
```cpp
void ESP.restart(void)
```

### 功能描述
- **软件重启**：通过软件方式重启ESP32，等效于按下Reset按钮
- **清除状态**：清除所有RAM数据，重新初始化系统
- **保留存储**：NVS（非易失存储）中的数据不会丢失
- **重新启动**：重新执行`setup()`和`loop()`

### 重启过程
1. 关闭所有外设（WiFi、串口、定时器等）
2. 复位CPU和外设寄存器
3. 重新加载bootloader
4. 重新初始化系统
5. 执行用户代码

### 重启时间
- **重启耗时**：约2-5秒
- **WiFi连接**：约3-8秒
- **总计时间**：约10秒完全恢复

## 使用场景

### 1. 配置更新后重启
当修改以下配置后，需要重启才能生效：
- WiFi SSID/密码修改
- 车队配置（头车/从车角色）
- ESP-NOW通信设置
- NVS存储的参数修改

### 2. 异常恢复
当系统出现以下问题时，可通过重启恢复：
- 传感器数据异常
- 网络连接不稳定
- 控制响应异常
- 内存泄漏（长时间运行）

### 3. 调试开发
- 快速重启测试代码修改
- 验证启动流程
- 清除临时状态

## 操作步骤

1. **打开网页控制台**
   - 连接到机器人WiFi
   - 访问控制网页

2. **点击重启按钮**
   - 位置：顶部工具栏"系统重启"按钮
   - 样式：灰色次要按钮

3. **确认操作**
   - 弹出确认对话框
   - 提示：需等待约10秒重新连接
   - 点击"确定"执行重启

4. **等待重启**
   - 网页显示"系统重启中..."
   - ESP32重启（约5秒）
   - 网页自动刷新重新连接

5. **恢复连接**
   - WiFi重新连接（约5秒）
   - WebSocket重新建立
   - 恢复正常控制

## 注意事项

### ⚠️ 安全提醒

1. **运动状态下**：
   - 机器人运动时重启会导致失控
   - 建议先关闭"启动机甲"开关
   - 确保机器人处于安全位置

2. **数据保存**：
   - 重启会清除RAM中的临时数据
   - NVS中的PID参数、WiFi配置会保留
   - 未保存的PID调整会丢失

3. **重新连接**：
   - 重启后WiFi可能需要10秒才能恢复
   - 网页会自动刷新重新连接
   - 如果连接失败，请手动刷新页面

### 💡 最佳实践

1. **重启前准备**：
   ```
   ✓ 关闭机甲运行开关
   ✓ 确保机器人平稳放置
   ✓ 保存重要的PID参数
   ✓ 记录当前配置
   ```

2. **替代方案**：
   - 如果只需重置姿态，使用"归零姿态"按钮
   - 如果只需重连WiFi，重启路由器即可
   - 如果只需清除临时状态，断电重启更彻底

3. **故障排查**：
   - 重启后无法连接：检查WiFi配置
   - 重启失败：尝试物理按Reset按钮
   - 反复重启：检查代码中的启动错误

## 技术细节

### WebSocket消息格式

**客户端 → 服务器**：
```json
{
  "type": "system_restart"
}
```

**服务器 → 客户端**（确认）：
```json
{
  "type": "info",
  "text": "ESP32正在重启..."
}
```

### 与物理Reset按钮的区别

| 特性 | 软件重启(ESP.restart()) | 硬件Reset按钮 |
|------|------------------------|---------------|
| 触发方式 | 软件调用 | 物理按钮 |
| 重启速度 | 快（约2-5秒） | 稍慢（约3-6秒） |
| 外设状态 | 优雅关闭 | 强制断电 |
| 副作用 | 无 | 可能影响闪存寿命 |
| 远程控制 | 支持 ✅ | 不支持 ❌ |

### 其他重启方式

ESP32提供多种重启方式：

```cpp
// 1. 软件重启（推荐）
ESP.restart();

// 2. 深度睡眠重启
esp_deep_sleep_start();

// 3. 看门狗超时重启
while(1); // 触发看门狗超时

// 4. 强制重启（不推荐）
esp_restart();
```

**本项目选择`ESP.restart()`的原因**：
- ✅ API简洁易用
- ✅ 兼容Arduino框架
- ✅ 优雅关闭所有外设
- ✅ 重启速度快
- ✅ 不需要额外配置

## 修改的文件

### 前端文件
1. `data/home.html` - 添加重启按钮UI
2. `data/js/config.js` - 注册DOM元素
3. `data/js/ui.js` - 添加按钮事件处理

### 后端文件
1. `src/my_net_lib/my_web.cpp` - 添加WebSocket消息处理

## 测试验证

### 测试步骤

1. **编译上传**：
   ```bash
   pio run -t upload
   ```

2. **打开网页控制台**：
   - 访问机器人IP地址
   - 观察连接状态

3. **测试重启功能**：
   - 点击"系统重启"按钮
   - 确认对话框点击"确定"
   - 观察串口输出：`[WEB] System restart requested`
   - 等待ESP32重启（约5秒）
   - 网页自动刷新并重新连接

4. **验证配置保留**：
   - 重启前调整PID参数并保存
   - 执行重启
   - 重启后检查PID参数是否保留

### 预期结果

- ✅ 点击按钮后弹出确认对话框
- ✅ 确认后ESP32立即重启
- ✅ 串口打印重启日志
- ✅ 约5秒后WiFi恢复
- ✅ 网页自动重新连接
- ✅ NVS中的配置数据保留
- ✅ PID参数、WiFi配置等保持不变

## 故障排查

### 问题1：点击按钮无反应

**可能原因**：
- WebSocket未连接
- 按钮DOM元素未正确注册

**解决方法**：
1. 检查浏览器控制台是否有JavaScript错误
2. 确认WebSocket连接状态
3. 刷新页面重新连接

### 问题2：重启后无法连接

**可能原因**：
- ESP32启动失败
- WiFi配置错误
- 路由器未分配IP

**解决方法**：
1. 检查串口输出的错误信息
2. 物理按Reset按钮重启
3. 重新扫描WiFi网络
4. 检查路由器DHCP设置

### 问题3：重启过程卡住

**可能原因**：
- 代码中有阻塞性错误
- 看门狗超时未启用

**解决方法**：
1. 等待30秒看是否自动恢复
2. 物理按Reset按钮强制重启
3. 检查`setup()`函数中的初始化代码
4. 启用硬件看门狗

## 扩展功能建议

### 1. 添加重启原因记录

```cpp
#include <rom/rtc.h>

void printResetReason() {
  RESET_REASON reason = rtc_get_reset_reason(0);
  switch(reason) {
    case POWERON_RESET: Serial.println("Power on reset"); break;
    case SW_RESET: Serial.println("Software reset"); break;
    case OWDT_RESET: Serial.println("Watchdog reset"); break;
    // ...
  }
}
```

### 2. 添加延迟重启功能

```cpp
// 延迟N秒后重启
void delayedRestart(int seconds) {
  Serial.printf("System will restart in %d seconds\n", seconds);
  delay(seconds * 1000);
  ESP.restart();
}
```

### 3. 添加重启计数器

```cpp
#include <Preferences.h>

void incrementRestartCounter() {
  Preferences prefs;
  prefs.begin("system", false);
  int count = prefs.getInt("restart_count", 0);
  prefs.putInt("restart_count", count + 1);
  prefs.end();
  Serial.printf("Restart count: %d\n", count + 1);
}
```

## 参考资料

- [ESP32 Arduino Core API](https://github.com/espressif/arduino-esp32)
- [ESP-IDF System API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html)
- [AsyncWebSocket文档](https://github.com/me-no-dev/ESPAsyncWebServer)

## 版本历史

- **v1.0** (2026-02-03)
  - 初始实现
  - 添加网页重启按钮
  - 添加WebSocket消息处理
  - 添加安全确认机制

## 作者

实现者：AI助手 (Claude)  
项目：hw-balance-bot  
日期：2026-02-03

