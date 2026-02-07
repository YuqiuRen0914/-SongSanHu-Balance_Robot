#pragma once
#include "Arduino.h"
#include "my_pid.h"

// 摔倒检测参数
#define COUNT_FALL_MAX 3 // 连续3次采样超限才算倒地
#define FALL_MAX_PITCH +30.0f
#define FALL_MIN_PITCH -30.0f
#define DUTY_SUM_LIM 10.0f

extern void pid_state_update();
extern void robot_state_update();
extern void robot_pos_control();
extern void pitch_control();
extern void yaw_control();
extern void pitch_zero_adapt();
extern void fall_check();
extern void control_idle_reset();
extern void duty_add();
