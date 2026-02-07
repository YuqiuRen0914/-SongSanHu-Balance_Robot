#include <cmath>
#include <LittleFS.h>
#include "my_net_config.h"
#include "my_bat.h"
#include "my_group.h"
#include "my_params.h"

static constexpr float JOY_X_DEADBAND = 0.10f;
static constexpr float JOY_Y_DEADBAND = 0.02f;
static constexpr float JOY_A_DEADBAND = 0.02f;
static constexpr float JOY_AXIS_LOCK_FRACTION = 0.2f; // 副轴必须超过主轴的比例才放行
static constexpr float JOY_AXIS_LOCK_FLOOR = 0.05f;    // 副轴绝对值低于该值直接清零

// 12+2 路遥测数据
void my_web_data_update()
{
    JsonDocument doc;

    // 组包 -> 广播
    doc["type"] = "telemetry";
    doc["fallen"] = FALLEN;
    doc["pitch"] = ANGLE_X;
    doc["roll"] = ANGLE_Y;
    doc["yaw"] = ANGLE_Z;
    doc["battery"] = battery_voltage;
    
    // 根据 charts_send 决定是否打包 n 路曲线数据
    if (robot.chart_enable)
    {
        JsonArray arr = doc["d"].to<JsonArray>();
        arr.add(CHART_11);
        arr.add(CHART_12);
        arr.add(CHART_13);
        arr.add(CHART_21);
        arr.add(CHART_22);
        arr.add(CHART_23);
        arr.add(CHART_31);
        arr.add(CHART_32);
        arr.add(CHART_33);
    }

    // 车队状态（如果启用）
    const group_config &cfg = my_group_get_config();
    if (cfg.espnow_enabled)
    {
        JsonObject group = doc["group_status"].to<JsonObject>();
        group["espnow_status"] = my_group_espnow_is_ready() ? "ok" : "error";
        
        // 头车：包含从车列表
        if (cfg.role == VehicleRole::LEADER)
        {
            JsonArray followers = group["followers"].to<JsonArray>();
            follower_info flist[MAX_FOLLOWERS];
            int count = my_group_get_followers(flist, MAX_FOLLOWERS);
            
            for (int i = 0; i < count; i++)
            {
                JsonObject f = followers.add<JsonObject>();
                char mac_str[18];
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                        flist[i].mac[0], flist[i].mac[1], flist[i].mac[2],
                        flist[i].mac[3], flist[i].mac[4], flist[i].mac[5]);
                f["mac"] = mac_str;
                f["last_seen_ms"] = millis() - flist[i].last_seen;
            }
        }
    }
    
    wsBroadcast(doc);
}
// PID 设置（顺序：角度P/I/D，速度P/I/D，位置P/I/D）
void web_pid_set(JsonObject param)
{
    SLIDER_11 = param["key01"].as<float>();
    SLIDER_12 = param["key02"].as<float>();
    SLIDER_13 = param["key03"].as<float>();
    SLIDER_21 = param["key04"].as<float>();
    SLIDER_22 = param["key05"].as<float>();
    SLIDER_23 = param["key06"].as<float>();
    SLIDER_31 = param["key07"].as<float>();
    SLIDER_32 = param["key08"].as<float>();
    SLIDER_33 = param["key09"].as<float>();
    SLIDER_41 = param["key10"].as<float>();
    SLIDER_42 = param["key11"].as<float>();
    SLIDER_43 = param["key12"].as<float>();
    pid_state_update();
    
    // 保存PID参数到NVS
    my_params_save();
}
// PID 读取
void web_pid_get(AsyncWebSocketClient *c)
{
    JsonDocument out;
    JsonObject pr = out["param"].to<JsonObject>();
    out["type"] = "pid";
    pr["key01"] = SLIDER_11;
    pr["key02"] = SLIDER_12;
    pr["key03"] = SLIDER_13;
    pr["key04"] = SLIDER_21;
    pr["key05"] = SLIDER_22;
    pr["key06"] = SLIDER_23;
    pr["key07"] = SLIDER_31;
    pr["key08"] = SLIDER_32;
    pr["key09"] = SLIDER_33;
    pr["key10"] = SLIDER_41;
    pr["key11"] = SLIDER_42;
    pr["key12"] = SLIDER_43;

    wsSendTo(c, out);
}
// 摇杆
void web_joystick(float x, float y, float a)
{
    const float x_clamped = my_lim(x, -1.0f, 1.0f);
    const float y_clamped = my_lim(y, -1.0f, 1.0f);
    const float a_clamped = my_lim(a, -1.0f, 1.0f);

    float x_filtered = my_db(x_clamped, JOY_X_DEADBAND);
    float y_filtered = my_db(y_clamped, JOY_Y_DEADBAND);
    float a_filtered = my_db(a_clamped, JOY_A_DEADBAND);

    // 轴向锁定：主轴明显占优时抑制副轴的小幅偏移
    if (fabsf(y_filtered) > JOY_AXIS_LOCK_FLOOR &&
        fabsf(x_filtered) < fmaxf(JOY_AXIS_LOCK_FLOOR, fabsf(y_filtered) * JOY_AXIS_LOCK_FRACTION))
    {
        x_filtered = 0.0f;
    }
    if (fabsf(x_filtered) > JOY_AXIS_LOCK_FLOOR &&
        fabsf(y_filtered) < fmaxf(JOY_AXIS_LOCK_FLOOR, fabsf(x_filtered) * JOY_AXIS_LOCK_FRACTION))
    {
        y_filtered = 0.0f;
    }

    robot.joy.x = x_filtered;
    robot.joy.y = y_filtered * 0.7f;
    robot.joy.a = a_filtered;
}

// 车队配置设置
void web_group_config_set(JsonObject param)
{
    const char *role_str = param["role"] | "standalone";
    VehicleRole new_role = VehicleRole::STANDALONE;
    
    if (strcmp(role_str, "leader") == 0)
        new_role = VehicleRole::LEADER;
    else if (strcmp(role_str, "follower") == 0)
        new_role = VehicleRole::FOLLOWER;
    
    group_config new_cfg = my_group_get_config();
    new_cfg.role = new_role;
    new_cfg.group_id = param["group_id"] | 0;
    
    // 处理espnow_enabled：可能是布尔值或数字
    if (param.containsKey("espnow_enabled"))
    {
        JsonVariant v = param["espnow_enabled"];
        if (v.is<bool>())
            new_cfg.espnow_enabled = v.as<bool>();
        else if (v.is<int>())
            new_cfg.espnow_enabled = v.as<int>() != 0;
        else
            new_cfg.espnow_enabled = false;
    }
    else
    {
        new_cfg.espnow_enabled = false;
    }
    
    // 如果是从车，需要设置头车MAC
    if (new_role == VehicleRole::FOLLOWER)
    {
        const char *mac_str = param["leader_mac"] | "";
        if (strlen(mac_str) > 0)
        {
            // 解析MAC地址字符串 "AA:BB:CC:DD:EE:FF"
            int values[6];
            if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
                      &values[0], &values[1], &values[2],
                      &values[3], &values[4], &values[5]) == 6)
            {
                for (int i = 0; i < 6; i++)
                {
                    new_cfg.leader_mac[i] = (uint8_t)values[i];
                }
            }
        }
    }
    
    // 保存配置
    my_group_config_save(new_cfg);
    
    Serial.println("[WEB] Group config updated, reboot required");
}

// 车队配置读取
void web_group_config_get(AsyncWebSocketClient *c)
{
    JsonDocument out;
    out["type"] = "group_config";
    
    const group_config &cfg = my_group_get_config();
    
    // 角色
    switch (cfg.role)
    {
    case VehicleRole::LEADER:
        out["role"] = "leader";
        break;
    case VehicleRole::FOLLOWER:
        out["role"] = "follower";
        break;
    default:
        out["role"] = "standalone";
        break;
    }
    
    // 本机MAC
    uint8_t my_mac[6];
    my_group_get_mac(my_mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    out["my_mac"] = mac_str;
    
    // 头车MAC
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             cfg.leader_mac[0], cfg.leader_mac[1], cfg.leader_mac[2],
             cfg.leader_mac[3], cfg.leader_mac[4], cfg.leader_mac[5]);
    out["leader_mac"] = mac_str;
    
    // 其他配置
    out["group_id"] = cfg.group_id;
    out["espnow_enabled"] = cfg.espnow_enabled;
    out["espnow_status"] = my_group_espnow_is_ready() ? "ok" : "error";
    
    wsSendTo(c, out);
}
