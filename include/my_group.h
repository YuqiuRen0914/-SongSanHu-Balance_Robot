#pragma once

#include <Arduino.h>
#include <cstdint>

/********** 车队配置 **********/
#define GROUP_NVS_NAMESPACE "group_cfg"
#define GROUP_COMMAND_TIMEOUT_MS 500  // 从车超时时间
#define GROUP_MAX_FOLLOWERS 20        // ESP-NOW最大peer数

/********** 车辆角色枚举 **********/
enum class VehicleRole : uint8_t
{
    STANDALONE = 0,  // 单机模式
    LEADER = 1,      // 头车
    FOLLOWER = 2     // 从车
};

/********** 车队配置结构 **********/
struct group_config
{
    VehicleRole role;           // 车辆角色
    uint8_t leader_mac[6];      // 头车MAC地址（从车需要）
    uint8_t group_id;           // 车队ID
    bool espnow_enabled;        // ESP-NOW启用状态
};

/********** 运动指令结构（ESP-NOW传输） **********/
struct motion_command
{
    float L_duty;               // 左轮占空比 (-1.0 ~ 1.0)
    float R_duty;               // 右轮占空比 (-1.0 ~ 1.0)
    uint32_t timestamp;         // 时间戳（毫秒）
    uint8_t group_id;           // 车队ID
    uint8_t checksum;           // 简单校验和
} __attribute__((packed));

/********** 从车心跳结构（从车 -> 头车） **********/
struct follower_heartbeat
{
    uint8_t follower_mac[6];    // 从车MAC地址
    uint8_t group_id;           // 车队ID
    uint32_t timestamp;         // 时间戳
    uint8_t battery_level;      // 电池电量 (0-100)
    uint8_t checksum;           // 校验和
} __attribute__((packed));

/********** 从车状态结构 **********/
struct follower_info
{
    uint8_t mac[6];             // MAC地址
    uint32_t last_seen;         // 最后心跳时间
    bool online;                // 是否在线
};

/********** 全局变量 **********/
extern group_config g_group_cfg;
extern volatile uint32_t g_last_command_time;  // 最后接收指令时间

#define MAX_FOLLOWERS 10  // 最大从车追踪数量
extern follower_info g_followers[MAX_FOLLOWERS];
extern int g_follower_count;

/********** 函数接口 **********/
// 初始化组队系统（必须在WiFi初始化前调用）
void my_group_init();

// 初始化ESP-NOW（在WiFi初始化后调用）
void my_group_espnow_init();

// 配置管理
bool my_group_config_save(const group_config &cfg);
bool my_group_config_load(group_config &cfg);
void my_group_set_role(VehicleRole role, const uint8_t *leader_mac = nullptr);

// 通信接口
void my_group_send_command(float left_duty, float right_duty);
bool my_group_is_command_timeout();

// 获取当前配置
const group_config &my_group_get_config();

// 获取本机MAC地址
void my_group_get_mac(uint8_t *mac);

// ESP-NOW状态
bool my_group_espnow_is_ready();

// 从车心跳（从车调用）
void my_group_send_heartbeat();

// 获取从车列表（头车调用）
int my_group_get_followers(follower_info *followers, int max_count);

// 更新从车在线状态
void my_group_update_followers_status();

