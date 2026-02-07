#include "my_group.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include "my_config.h"

/********** 全局变量 **********/
group_config g_group_cfg = {
    .role = VehicleRole::STANDALONE,
    .leader_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    .group_id = 0,
    .espnow_enabled = false
};

volatile uint32_t g_last_command_time = 0;
static bool g_espnow_initialized = false;
static motion_command g_last_received_cmd = {0};

// 广播地址用于头车发送
static uint8_t g_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 从车追踪
follower_info g_followers[MAX_FOLLOWERS];
int g_follower_count = 0;
static uint32_t g_last_heartbeat_time = 0;
#define HEARTBEAT_INTERVAL_MS 1000  // 从车每1秒发送一次心跳
#define FOLLOWER_TIMEOUT_MS 3000    // 3秒无心跳视为离线

/********** 内部函数声明 **********/
static uint8_t calculate_checksum(const motion_command &cmd);
static bool verify_checksum(const motion_command &cmd);
static uint8_t calculate_heartbeat_checksum(const follower_heartbeat &hb);
static bool verify_heartbeat_checksum(const follower_heartbeat &hb);
static void espnow_send_callback(const uint8_t *mac, esp_now_send_status_t status);
static void espnow_receive_callback(const uint8_t *mac, const uint8_t *data, int len);
static void handle_motion_command(const uint8_t *data, int len);
static void handle_follower_heartbeat(const uint8_t *mac, const uint8_t *data, int len);
static int find_or_add_follower(const uint8_t *mac);

/********** 配置管理 **********/
bool my_group_config_save(const group_config &cfg)
{
    Preferences pref;
    if (!pref.begin(GROUP_NVS_NAMESPACE, false))
    {
        Serial.println("[GROUP] Failed to open NVS for writing");
        return false;
    }

    pref.putUChar("role", static_cast<uint8_t>(cfg.role));
    pref.putBytes("leader_mac", cfg.leader_mac, 6);
    pref.putUChar("group_id", cfg.group_id);
    pref.putBool("espnow_en", cfg.espnow_enabled);

    pref.end();
    Serial.println("[GROUP] Configuration saved to NVS");
    return true;
}

bool my_group_config_load(group_config &cfg)
{
    Preferences pref;
    if (!pref.begin(GROUP_NVS_NAMESPACE, true))
    {
        Serial.println("[GROUP] No saved configuration, using defaults");
        return false;
    }

    cfg.role = static_cast<VehicleRole>(pref.getUChar("role", 0));
    size_t len = pref.getBytes("leader_mac", cfg.leader_mac, 6);
    if (len != 6)
    {
        // 默认MAC
        for (int i = 0; i < 6; i++)
            cfg.leader_mac[i] = 0xFF;
    }
    cfg.group_id = pref.getUChar("group_id", 0);
    cfg.espnow_enabled = pref.getBool("espnow_en", false);

    pref.end();
    Serial.println("[GROUP] Configuration loaded from NVS");
    return true;
}

void my_group_set_role(VehicleRole role, const uint8_t *leader_mac)
{
    g_group_cfg.role = role;
    if (leader_mac != nullptr)
    {
        memcpy(g_group_cfg.leader_mac, leader_mac, 6);
    }
    my_group_config_save(g_group_cfg);
}

const group_config &my_group_get_config()
{
    return g_group_cfg;
}

void my_group_get_mac(uint8_t *mac)
{
    esp_wifi_get_mac(WIFI_IF_STA, mac);
}

/********** 校验和计算 **********/
static uint8_t calculate_checksum(const motion_command &cmd)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&cmd);
    uint8_t sum = 0;
    // 计算除checksum字段外的所有字节
    for (size_t i = 0; i < sizeof(motion_command) - 1; i++)
    {
        sum ^= data[i];
    }
    return sum;
}

static bool verify_checksum(const motion_command &cmd)
{
    return cmd.checksum == calculate_checksum(cmd);
}

static uint8_t calculate_heartbeat_checksum(const follower_heartbeat &hb)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&hb);
    uint8_t sum = 0;
    for (size_t i = 0; i < sizeof(follower_heartbeat) - 1; i++)
    {
        sum ^= data[i];
    }
    return sum;
}

static bool verify_heartbeat_checksum(const follower_heartbeat &hb)
{
    return hb.checksum == calculate_heartbeat_checksum(hb);
}

/********** ESP-NOW回调函数 **********/
static void espnow_send_callback(const uint8_t *mac, esp_now_send_status_t status)
{
    // 可选：记录发送状态用于调试
    // if (status != ESP_NOW_SEND_SUCCESS)
    // {
    //     Serial.println("[GROUP] Send failed");
    // }
}

static void espnow_receive_callback(const uint8_t *mac, const uint8_t *data, int len)
{
    // 判断数据类型并分发
    if (len == sizeof(motion_command))
    {
        handle_motion_command(data, len);
    }
    else if (len == sizeof(follower_heartbeat))
    {
        handle_follower_heartbeat(mac, data, len);
    }
}

static void handle_motion_command(const uint8_t *data, int len)
{
    motion_command cmd;
    memcpy(&cmd, data, sizeof(motion_command));

    // 校验数据
    if (!verify_checksum(cmd))
    {
        Serial.println("[GROUP] Motion command checksum error");
        return;
    }

    // 检查车队ID
    if (cmd.group_id != g_group_cfg.group_id)
    {
        return; // 不是本车队的指令
    }

    // 保存接收到的指令
    g_last_received_cmd = cmd;
    g_last_command_time = millis();

    // 应用到robot状态（仅从车）
    if (g_group_cfg.role == VehicleRole::FOLLOWER)
    {
        extern robot_state robot;
        robot.motor.L_duty = cmd.L_duty;
        robot.motor.R_duty = cmd.R_duty;
    }
}

static void handle_follower_heartbeat(const uint8_t *mac, const uint8_t *data, int len)
{
    // 仅头车处理心跳
    if (g_group_cfg.role != VehicleRole::LEADER)
    {
        return;
    }

    follower_heartbeat hb;
    memcpy(&hb, data, sizeof(follower_heartbeat));

    // 校验
    if (!verify_heartbeat_checksum(hb))
    {
        Serial.println("[GROUP] Heartbeat checksum error");
        return;
    }

    // 检查车队ID
    if (hb.group_id != g_group_cfg.group_id)
    {
        return;
    }

    // 更新或添加从车
    int idx = find_or_add_follower(mac);
    if (idx >= 0)
    {
        g_followers[idx].last_seen = millis();
        g_followers[idx].online = true;
    }
}

static int find_or_add_follower(const uint8_t *mac)
{
    // 查找现有从车
    for (int i = 0; i < g_follower_count; i++)
    {
        if (memcmp(g_followers[i].mac, mac, 6) == 0)
        {
            return i;
        }
    }

    // 添加新从车
    if (g_follower_count < MAX_FOLLOWERS)
    {
        memcpy(g_followers[g_follower_count].mac, mac, 6);
        g_followers[g_follower_count].last_seen = millis();
        g_followers[g_follower_count].online = true;
        return g_follower_count++;
    }

    return -1; // 已满
}

/********** ESP-NOW初始化 **********/
static bool espnow_init()
{
    if (g_espnow_initialized)
    {
        return true;
    }

    // 初始化ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[GROUP] ESP-NOW init failed");
        return false;
    }

    // 注册回调
    esp_now_register_send_cb(espnow_send_callback);
    esp_now_register_recv_cb(espnow_receive_callback);

    // 根据角色添加peer
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = 0; // 使用当前信道
    peerInfo.encrypt = false;

    if (g_group_cfg.role == VehicleRole::LEADER)
    {
        // 头车添加广播地址
        memcpy(peerInfo.peer_addr, g_broadcast_mac, 6);
        if (esp_now_add_peer(&peerInfo) != ESP_OK)
        {
            Serial.println("[GROUP] Failed to add broadcast peer");
            return false;
        }
        Serial.println("[GROUP] Leader mode: broadcast peer added");
    }
    else if (g_group_cfg.role == VehicleRole::FOLLOWER)
    {
        // 从车添加头车MAC
        memcpy(peerInfo.peer_addr, g_group_cfg.leader_mac, 6);
        if (esp_now_add_peer(&peerInfo) != ESP_OK)
        {
            Serial.println("[GROUP] Failed to add leader peer");
            return false;
        }
        Serial.print("[GROUP] Follower mode: leader MAC ");
        for (int i = 0; i < 6; i++)
        {
            Serial.printf("%02X", g_group_cfg.leader_mac[i]);
            if (i < 5)
                Serial.print(":");
        }
        Serial.println();
    }

    g_espnow_initialized = true;
    Serial.println("[GROUP] ESP-NOW initialized successfully");
    return true;
}

/********** WiFi模式配置 **********/
static void configure_wifi_mode()
{
    // ESP-NOW需要STA或AP+STA模式才能工作
    // 纯AP模式下ESP-NOW无法发送数据
    if (g_group_cfg.espnow_enabled && g_group_cfg.role != VehicleRole::STANDALONE)
    {
        // 头车和从车都使用AP+STA模式来支持ESP-NOW
        WiFi.mode(WIFI_AP_STA);
        Serial.println("[GROUP] WiFi mode: AP+STA (ESP-NOW enabled)");
    }
    else
    {
        // 单机模式或ESP-NOW未启用时使用纯AP模式
        WiFi.mode(WIFI_AP);
        Serial.println("[GROUP] WiFi mode: AP (Standalone/ESP-NOW disabled)");
    }
}

/********** 公共接口实现 **********/
void my_group_init()
{
    Serial.println("[GROUP] Initializing group system...");

    // 加载配置
    if (!my_group_config_load(g_group_cfg))
    {
        // 使用默认配置（STANDALONE）
        g_group_cfg.role = VehicleRole::STANDALONE;
        g_group_cfg.espnow_enabled = false;
        g_group_cfg.group_id = 0;
    }

    // 打印当前配置
    Serial.print("[GROUP] Role: ");
    switch (g_group_cfg.role)
    {
    case VehicleRole::STANDALONE:
        Serial.println("STANDALONE");
        break;
    case VehicleRole::LEADER:
        Serial.println("LEADER");
        break;
    case VehicleRole::FOLLOWER:
        Serial.println("FOLLOWER");
        break;
    }
    Serial.printf("[GROUP] Group ID: %d\n", g_group_cfg.group_id);
    Serial.printf("[GROUP] ESP-NOW enabled: %s\n", g_group_cfg.espnow_enabled ? "YES" : "NO");

    // 配置WiFi模式（在WiFi初始化前设置）
    configure_wifi_mode();
    Serial.printf("[GROUP] WiFi mode set, current mode: %d\n", WiFi.getMode());

    g_last_command_time = millis();
}

// 在WiFi初始化后调用，初始化ESP-NOW
void my_group_espnow_init()
{
    Serial.println("[GROUP] Initializing ESP-NOW...");
    
    // 如果未启用ESP-NOW，直接返回
    if (!g_group_cfg.espnow_enabled || g_group_cfg.role == VehicleRole::STANDALONE)
    {
        Serial.println("[GROUP] ESP-NOW not enabled or standalone mode");
        return;
    }
    
    // 确保WiFi已启动
    if (WiFi.getMode() == WIFI_OFF)
    {
        Serial.println("[GROUP] WiFi is OFF, cannot init ESP-NOW");
        g_group_cfg.espnow_enabled = false;
        return;
    }
    
    if (!espnow_init())
    {
        Serial.println("[GROUP] ESP-NOW initialization failed");
        g_group_cfg.espnow_enabled = false;
        return;
    }
    
    // 打印本机MAC地址
    uint8_t mac[6];
    my_group_get_mac(mac);
    Serial.print("[GROUP] My MAC: ");
    for (int i = 0; i < 6; i++)
    {
        Serial.printf("%02X", mac[i]);
        if (i < 5)
            Serial.print(":");
    }
    Serial.println();
}

void my_group_send_command(float left_duty, float right_duty)
{
    // 仅头车发送
    if (g_group_cfg.role != VehicleRole::LEADER || !g_espnow_initialized)
    {
        return;
    }

    motion_command cmd;
    cmd.L_duty = left_duty;
    cmd.R_duty = right_duty;
    cmd.timestamp = millis();
    cmd.group_id = g_group_cfg.group_id;
    cmd.checksum = calculate_checksum(cmd);

    esp_err_t result = esp_now_send(g_broadcast_mac, (uint8_t *)&cmd, sizeof(cmd));
    if (result != ESP_OK)
    {
        // 发送失败（可选：记录错误）
    }
}

bool my_group_is_command_timeout()
{
    if (g_group_cfg.role != VehicleRole::FOLLOWER)
    {
        return false; // 非从车永不超时
    }

    uint32_t now = millis();
    uint32_t elapsed = now - g_last_command_time;

    // 处理millis()溢出
    if (elapsed > GROUP_COMMAND_TIMEOUT_MS)
    {
        return true;
    }

    return false;
}

bool my_group_espnow_is_ready()
{
    return g_espnow_initialized;
}

void my_group_send_heartbeat()
{
    // 仅从车发送心跳
    if (g_group_cfg.role != VehicleRole::FOLLOWER || !g_espnow_initialized)
    {
        return;
    }

    uint32_t now = millis();
    if (now - g_last_heartbeat_time < HEARTBEAT_INTERVAL_MS)
    {
        return; // 未到发送时间
    }

    follower_heartbeat hb;
    my_group_get_mac(hb.follower_mac);
    hb.group_id = g_group_cfg.group_id;
    hb.timestamp = now;
    hb.battery_level = 0; // TODO: 获取实际电池电量
    hb.checksum = calculate_heartbeat_checksum(hb);

    esp_err_t result = esp_now_send(g_group_cfg.leader_mac, (uint8_t *)&hb, sizeof(hb));
    if (result == ESP_OK)
    {
        g_last_heartbeat_time = now;
    }
}

int my_group_get_followers(follower_info *followers, int max_count)
{
    if (g_group_cfg.role != VehicleRole::LEADER || followers == nullptr)
    {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < g_follower_count && count < max_count; i++)
    {
        if (g_followers[i].online)
        {
            followers[count++] = g_followers[i];
        }
    }

    return count;
}

void my_group_update_followers_status()
{
    if (g_group_cfg.role != VehicleRole::LEADER)
    {
        return;
    }

    uint32_t now = millis();
    for (int i = 0; i < g_follower_count; i++)
    {
        if (g_followers[i].online)
        {
            if (now - g_followers[i].last_seen > FOLLOWER_TIMEOUT_MS)
            {
                g_followers[i].online = false;
                Serial.print("[GROUP] Follower offline: ");
                for (int j = 0; j < 6; j++)
                {
                    Serial.printf("%02X", g_followers[i].mac[j]);
                    if (j < 5)
                        Serial.print(":");
                }
                Serial.println();
            }
        }
    }
}

