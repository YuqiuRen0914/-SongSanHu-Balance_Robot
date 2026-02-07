#include <Arduino.h>
#include "my_I2C.h"
#include "my_screen.h"
#include "my_motion.h"
#include "my_net.h"
#include "my_config.h"
#include "my_rgb.h"
#include "my_bat.h"
#include "my_inspect.h"
#include "my_group.h"
#include "my_params.h"

static TaskHandle_t control_TaskHandle = nullptr;   // 运动控制
static TaskHandle_t data_send_TaskHandle = nullptr; // 网页任务
static TaskHandle_t screen_TaskHandle = nullptr; // 屏幕刷新任务
static TaskHandle_t rgb_TaskHandle = nullptr; // RGB任务
// put function declarations here:
void robot_control_Task(void *)
{
    for (;;)
    {
        my_motion_update();
        vTaskDelay(pdMS_TO_TICKS(robot.dt_ms));
    }
}

void data_send_Task(void *)
{
    for (;;)
    {
        my_web_data_update();
        vTaskDelay(pdMS_TO_TICKS(robot.data_ms));
    }
}

void screen_Task(void *)
{
    for (;;)
    {
        my_screen_update();
        vTaskDelay(pdMS_TO_TICKS(SCREEN_REFRESH_TIME));
    }
}

void rgb_Task(void *)
{
    for (;;)
    {
        my_rgb_update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
  //断电后重启 

  //串口初始化
  Serial.begin(115200);
  delay(500);  // 等待串口稳定
  
  //I2C初始化
  my_i2c_init();
  
  //初始化运动（包括电机死区校准）
  my_motion_init();
  
  //加载保存的PID参数和pitch零点
  my_params_load();
  
  //执行开机外设检查
  my_inspect_check_all();
  
  //车队系统初始化（在wifi之前，用于确定WiFi模式）
  my_group_init();
  //wifi初始化（使用group_init设置的WiFi模式）
  my_wifi_init();
  //ESP-NOW初始化（在WiFi之后）
  my_group_espnow_init();
  //初始化异步服务器
  my_web_asyn_init();
  //电池检测初始化
  my_bat_init();
  //屏幕初始化
  my_screen_init();
  //RGB初始化
  my_rgb_init();


  xTaskCreatePinnedToCore(robot_control_Task, "ctrl_2ms", 8192, nullptr, 15, &control_TaskHandle, 0); // 初始化运动任务
  xTaskCreatePinnedToCore(data_send_Task, "telem", 8192, nullptr, 5, &data_send_TaskHandle, 1);
  // 屏幕刷新放低优先级，避免阻塞网络/灯效任务
  xTaskCreatePinnedToCore(screen_Task, "screen", 8192, nullptr, 3, &screen_TaskHandle, 1);
  xTaskCreatePinnedToCore(rgb_Task, "rgb", 2048, nullptr, 4, &rgb_TaskHandle, 1);

}

void loop() {
  // put your main code here, to run repeatedly:
}
