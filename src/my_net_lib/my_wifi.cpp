#include <cstring>
#include <WiFi.h>
#include <Preferences.h>
#include "esp_timer.h"
#include "esp_wifi.h"
#include "my_net_config.h"

// NVS namespace & keys
static constexpr const char *WIFI_PREF_NS = "wifi_cfg";
static constexpr const char *WIFI_PREF_SSID = "ssid";
static constexpr const char *WIFI_PREF_PWD = "pwd";
static constexpr const char *WIFI_PREF_AP_IP = "ap_ip";
static constexpr size_t WIFI_SSID_MAX_LEN = 32;
static constexpr size_t WIFI_PWD_MIN_LEN = 8;
static constexpr size_t WIFI_PWD_MAX_LEN = 63;

// 当前运行配置（可能来自默认值或存储）
static wifi_runtime_config g_wifi_cfg{SSID, PASSWORD, strlen(PASSWORD) < WIFI_PWD_MIN_LEN};
static IPAddress g_ap_ip(192, 168, 4, 1);

/********** 工具函数实现 **********/

// 生成带MAC后缀的默认SSID
static String generate_default_ssid()
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char ssid[33];
    snprintf(ssid, sizeof(ssid), "%s_%02X%02X", SSID_PREFIX, mac[4], mac[5]);
    return String(ssid);
}

static IPAddress make_random_ap_ip()
{
    // 基于启动时间的简单随机，避开 0/255，并固定 /24 网段
    const uint32_t t = static_cast<uint32_t>(esp_timer_get_time() ^ millis());
    const uint8_t oct3 = static_cast<uint8_t>(10 + (t % 200));        // 10~209
    const uint8_t oct4 = static_cast<uint8_t>(1 + ((t / 7) % 200));   // 1~200，避免 0/255
    return IPAddress(192, 168, oct3, oct4);
}

// 保存AP IP地址到NVS
static void save_persistent_ip(IPAddress ip)
{
    Preferences pref;
    if (pref.begin(WIFI_PREF_NS, false))
    {
        pref.putUInt(WIFI_PREF_AP_IP, static_cast<uint32_t>(ip));
        pref.end();
        Serial.printf("[WIFI] AP IP saved: %s\n", ip.toString().c_str());
    }
}

// 从NVS加载AP IP地址
static IPAddress load_persistent_ip()
{
    Preferences pref;
    if (pref.begin(WIFI_PREF_NS, true))
    {
        uint32_t ip_uint = pref.getUInt(WIFI_PREF_AP_IP, 0);
        pref.end();
        if (ip_uint != 0)
        {
            IPAddress ip(ip_uint);
            Serial.printf("[WIFI] AP IP loaded from NVS: %s\n", ip.toString().c_str());
            return ip;
        }
    }
    // 首次启动，生成新的随机IP并保存
    IPAddress new_ip = make_random_ap_ip();
    save_persistent_ip(new_ip);
    Serial.printf("[WIFI] New random AP IP generated: %s\n", new_ip.toString().c_str());
    return new_ip;
}

/********** WiFi配置管理 **********/

static wifi_runtime_config normalize_wifi_config(String ssid, String password)
{
    ssid.trim();
    password.trim();
    if (ssid.length() > WIFI_SSID_MAX_LEN)
        ssid = ssid.substring(0, WIFI_SSID_MAX_LEN);
    if (password.length() > WIFI_PWD_MAX_LEN)
        password = password.substring(0, WIFI_PWD_MAX_LEN);
    wifi_runtime_config cfg{
        .ssid = ssid,
        .password = password,
        .open = password.length() < WIFI_PWD_MIN_LEN,
    };
    return cfg;
}

static bool validate_wifi_config(const wifi_runtime_config &cfg, String &err)
{
    if (cfg.ssid.isEmpty())
    {
        err = "SSID 不能为空";
        return false;
    }
    if (cfg.ssid.length() > WIFI_SSID_MAX_LEN)
    {
        err = "SSID 过长 (<=32)";
        return false;
    }
    if (!cfg.open && cfg.password.length() < WIFI_PWD_MIN_LEN)
    {
        err = "密码至少 8 位（或留空为开放热点）";
        return false;
    }
    if (cfg.password.length() > WIFI_PWD_MAX_LEN)
    {
        err = "密码过长 (<=63)";
        return false;
    }
    return true;
}

static wifi_runtime_config load_saved_wifi()
{
    Preferences pref;
    // 使用带MAC后缀的默认SSID
    String default_ssid = generate_default_ssid();
    wifi_runtime_config cfg = normalize_wifi_config(default_ssid, PASSWORD);
    
    if (pref.begin(WIFI_PREF_NS, true))
    {
        // 如果用户保存过自定义SSID，使用用户的设置
        String saved_ssid = pref.getString(WIFI_PREF_SSID, "");
        if (saved_ssid.length() > 0)
        {
            cfg = normalize_wifi_config(saved_ssid, pref.getString(WIFI_PREF_PWD, PASSWORD));
        }
        else
        {
            // 未保存过，使用默认的带MAC后缀SSID
            cfg = normalize_wifi_config(default_ssid, pref.getString(WIFI_PREF_PWD, PASSWORD));
        }
        pref.end();
    }
    
    String err;
    if (!validate_wifi_config(cfg, err))
        cfg = normalize_wifi_config(default_ssid, PASSWORD);
    return cfg;
}

static bool persist_wifi_config(const wifi_runtime_config &cfg)
{
    Preferences pref;
    if (!pref.begin(WIFI_PREF_NS, false))
        return false;
    pref.putString(WIFI_PREF_SSID, cfg.ssid);
    pref.putString(WIFI_PREF_PWD, cfg.password);
    pref.end();
    return true;
}

static bool start_softap(const wifi_runtime_config &cfg)
{
    const char *ap_password = cfg.open ? nullptr : cfg.password.c_str();
    IPAddress gw = g_ap_ip;
    IPAddress subnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(g_ap_ip, gw, subnet))
    {
        Serial.println("[WIFI] softAPConfig failed, fallback to 192.168.4.1");
        g_ap_ip = IPAddress(192, 168, 4, 1);
        gw = g_ap_ip;
        if (!WiFi.softAPConfig(g_ap_ip, gw, subnet))
            return false;
    }
    if (!WiFi.softAP(cfg.ssid.c_str(), ap_password))
        return false;
    g_wifi_cfg = cfg;
    return true;
}

const wifi_runtime_config &wifi_current_config()
{
    return g_wifi_cfg;
}

IPAddress wifi_ap_ip()
{
    return g_ap_ip;
}

bool wifi_update_and_apply(const String &ssid, const String &password, String &err)
{
    wifi_runtime_config cfg = normalize_wifi_config(ssid, password);
    if (!validate_wifi_config(cfg, err))
        return false;

    if (!persist_wifi_config(cfg))
    {
        err = "保存到 NVS 失败";
        return false;
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAPdisconnect(true);
    delay(50);
    if (!start_softap(cfg))
    {
        err = "热点重启失败";
        return false;
    }
    WiFi.setSleep(false);
    return true;
}

void my_wifi_init()
{
    // 注意：WiFi模式应该在my_group_init()中已经设置好
    // 这里不再调用WiFi.mode()，避免覆盖group设置的模式
    Serial.printf("[WIFI] Current WiFi mode: %d\n", WiFi.getMode());
    
    // 使用持久化IP（首次随机，后续保持不变）
    g_ap_ip = load_persistent_ip();
    
    // 加载WiFi配置（优先用户设置，否则使用带MAC后缀的默认SSID）
    wifi_runtime_config cfg = load_saved_wifi();

    if (!start_softap(cfg))
    {
        Serial.println("[WIFI] 热点启动失败，尝试默认值");
        String default_ssid = generate_default_ssid();
        cfg = normalize_wifi_config(default_ssid, PASSWORD);
        if (!start_softap(cfg))
        {
            Serial.println("[WIFI] 使用默认值启动热点失败");
            return;
        }
    }

    WiFi.setSleep(false);
    Serial.println("========== WiFi AP Started ==========");
    Serial.print("[WIFI] AP SSID: ");
    Serial.println(cfg.ssid);
    if (cfg.open)
        Serial.println("[WIFI] 开放热点（无密码）");
    else
    {
        Serial.print("[WIFI] AP Password: ");
        Serial.println(cfg.password);
    }
    Serial.print("[WIFI] AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    
    // 打印MAC地址便于识别
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    Serial.print("[WIFI] AP MAC Address: ");
    for (int i = 0; i < 6; i++)
    {
        Serial.printf("%02X", mac[i]);
        if (i < 5)
            Serial.print(":");
    }
    Serial.println();
    Serial.println("=====================================");
}
