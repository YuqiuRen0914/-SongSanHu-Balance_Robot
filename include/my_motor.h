#pragma once

#include <stdint.h>

extern volatile float motor_left_u;   // 左轮归一化指令（-1~1）
extern volatile float motor_right_u;  // 右轮归一化指令（-1~1）

void my_motor_init();                               // 初始化电机引脚、PWM通道以及死区校准流程
void my_motor_update();    
