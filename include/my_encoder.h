#pragma once
#include <stdint.h>
#include "my_config.h"

void my_encoder_init();     // 初始化编码器硬件
void my_encoder_update();   // 读取并复位本周期计数增量，并更新 wel 速度/位置

extern volatile int32_t Encoder_Left_Delta;  // 左编码器周期脉冲增量
extern volatile int32_t Encoder_Right_Delta; // 右编码器周期脉冲增量

//volatile把“本周期脉冲增量”作为一个跨模块共享的、随时可能被刷新/读取的值
