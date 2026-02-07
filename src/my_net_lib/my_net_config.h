#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include "my_net.h"

// 变量暴露
// 文件系统
#define FSYS LittleFS
// 以下是参数定义
// web刷新限制
#define REFRESH_RATE_DEF 10
#define REFRESH_RATE_MAX 60
#define REFRESH_RATE_MIN 1

struct ChartConfig
{
    const char *title;
    const char *legend[3];
};

struct SliderGroup
{
    const char *group;
    const char *names[3];
};

// 异步服务器对象
extern AsyncWebServer server;
extern AsyncWebSocket ws;

void web_pid_set(JsonObject param);
void web_pid_get(AsyncWebSocketClient *c);
void web_joystick(float x, float y, float a);
void web_group_config_set(JsonObject param);
void web_group_config_get(AsyncWebSocketClient *c);
// fs函数
static String contentType(const String &path);
// webtool函数
void wsSendTo(AsyncWebSocketClient *c, const JsonDocument &doc);
void my_wsheart();
bool wsCanBroadcast();
void wsBroadcast(const JsonDocument &doc);
bool handleFileRead(AsyncWebServerRequest *req, String path);
// tool函数
float my_db(float value, float deadband);
float my_lim(float value, float min, float max);
void rtrim_inplace(char *s);
