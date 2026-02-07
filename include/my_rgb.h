#pragma once

#include "my_config.h"

// 灯光模式枚举，方便外部设置 robot.rgb.mode
enum rgb_mode
{
    RGB_MODE_NEON = 0,     // 霓虹
    RGB_MODE_BREATH = 1,   // 呼吸
    RGB_MODE_METEOR = 2,   // 流星
    RGB_MODE_HEARTBEAT = 3 // 心跳
};

void my_rgb_init();
void my_rgb_update();
