#pragma once

#include <Arduino.h>
#include "my_tool.h"
#include "my_control.h"
#include "freertos/FreeRTOS.h"
#include "my_config.h"
#include "my_motion.h"

// 函数定义
void my_wifi_init();       // 初始化网络
void my_web_asyn_init();   // 初始化网页
void my_web_data_update(); // 数据更新

struct wifi_runtime_config
{
    String ssid;
    String password;
    bool open;
};

// WiFi 配置（SoftAP）
const wifi_runtime_config &wifi_current_config();
bool wifi_update_and_apply(const String &ssid, const String &password, String &err);
IPAddress wifi_ap_ip();

// 图表显示模式
#define CHART_PID
// #define CHART_FOC

// 遥测推送数据============================================
// 推送数据
#define FALLEN robot.fallen.is
#define ANGLE_X robot.imu.angley
#define ANGLE_Y robot.imu.anglex
#define ANGLE_Z robot.imu.anglez

#ifdef CHART_PID
// 发送和接受数据定义
// 图表1：角度跟踪（直立环核心）- 同量级数据
#define CHART_NAME1 "角度跟踪"
#define CHART_NAME11 "tar"
#define CHART_11 robot.ang.tar
#define CHART_NAME12 "now"
#define CHART_12 robot.ang.now
#define CHART_NAME13 "err"
#define CHART_13 robot.ang.err

// 图表2：控制力矩输出 - 各环输出力矩
#define CHART_NAME2 "力矩输出"
#define CHART_NAME21 "ang"
#define CHART_21 robot.ang.duty
#define CHART_NAME22 "spd×10"
#define CHART_22 (robot.spd.duty * 10.0f)
#define CHART_NAME23 "pos×10"
#define CHART_23 (robot.pos.duty * 10.0f)

// 图表3：速度与位置跟踪
#define CHART_NAME3 "速度跟踪"
#define CHART_NAME31 "now"
#define CHART_31 robot.spd.now
#define CHART_NAME32 "tar"
#define CHART_32 robot.spd.tar
#define CHART_NAME33 "pos_err×10"
#define CHART_33 (robot.pos.err * 10.0f)

#endif

#ifdef CHART_FOC
#define CHART_NAME1 "L"
#define CHART_11 robot.tor.L
#define CHART_NAME11 "tor"
#define CHART_12 robot.wel.spd1
#define CHART_NAME12 "spd"
#define CHART_13 robot.wel.pos1
#define CHART_NAME13 "pos"

#define CHART_NAME2 "R"
#define CHART_21 robot.tor.R
#define CHART_NAME21 "tor"
#define CHART_22 robot.wel.spd2
#define CHART_NAME22 "spd"
#define CHART_23 robot.wel.pos2
#define CHART_NAME23 "pos"

#define CHART_NAME3 "0"
#define CHART_31 0
#define CHART_NAME31 "0"
#define CHART_32 0
#define CHART_NAME32 "0"
#define CHART_33 0
#define CHART_NAME33 "0"
#endif

// 滑块关联数据 =============================================
#define SLIDER_NAME1 "平衡控制参数"
#define SLIDER_NAME11 "P"
#define SLIDER_11 robot.ang_pid.p
#define SLIDER_NAME12 "I"
#define SLIDER_12 robot.ang_pid.i
#define SLIDER_NAME13 "D"
#define SLIDER_13 robot.ang_pid.d
#define SLIDER_NAME2 "速度控制参数"
#define SLIDER_NAME21 "P"
#define SLIDER_21 robot.spd_pid.p
#define SLIDER_NAME22 "I"
#define SLIDER_22 robot.spd_pid.i
#define SLIDER_NAME23 "D"
#define SLIDER_23 robot.spd_pid.d
#define SLIDER_NAME3 "位置控制参数"
#define SLIDER_NAME31 "P"
#define SLIDER_31 robot.pos_pid.p
#define SLIDER_NAME32 "I"
#define SLIDER_32 robot.pos_pid.i
#define SLIDER_NAME33 "D"
#define SLIDER_33 robot.pos_pid.d
#define SLIDER_NAME4 "转向控制参数"
#define SLIDER_NAME41 "P"
#define SLIDER_41 robot.yaw_pid.p
#define SLIDER_NAME42 "I"
#define SLIDER_42 robot.yaw_pid.i
#define SLIDER_NAME43 "D"
#define SLIDER_43 robot.yaw_pid.d
