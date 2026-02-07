#include "my_motion.h"
#include "my_mpu6050.h"
#include "my_motor.h"
#include "my_control.h"
#include "my_tool.h"
#include "my_group.h"
#include <LittleFS.h>

robot_state robot = {
    // 状态指示位
    .dt_ms = 2,                // 运动控制频率
    .data_ms = 100,            // 网页推送频率
    .run = false,              // 运行指示位
    .chart_enable = false,     // 图表推送位
    .joy_stop_control = false, // 原地停车标志
    .wel_up = false,           // 轮部离地标志
    .pitch_zero = -2.1,       // pitch零点
    // 轮子数据
    .wel = {0, 0, 0, 0},                                 // 轮子数据 wel1 , wel2, pos1, pos2
    .motor = {
        .base_duty = 0.0f,
        .yaw_duty = 0.0f,
        .L_duty = 0.0f,
        .R_duty = 0.0f,
        .L_cmd = 0.0f,
        .R_cmd = 0.0f,
        .L_deadzone_fwd = 0.0f,
        .L_deadzone_rev = 0.0f,
        .R_deadzone_fwd = 0.0f,
        .R_deadzone_rev = 0.0f,
    },                             
    // IMU数据 anglex, angley, anglez, gyrox, gyroy, gyroz
    .imu_zero = {0, 0, 0, 0, 0, 0},
    .imu_l = {0, 0, 0, 0, 0, 0},
    .imu = {0, 0, 0, 0, 0, 0},
    // 摇杆控制 x, y, a, r, x_coef, y_coef
    .joy = {0, 0, 0, 0, 0.1, 10.0},
    .joy_l = {0, 0, 0, 0, 0.1, 10.0},
    // 摔倒检测 is, count, enable
    .fallen = {false, 0, false},
    // 灯珠数量，模式
    .rgb = {0, 0},
    // pid状态检测 now,last,target,error,duty
    .ang = {0, 0, 0, 0, 0}, // 直立环状态
    .spd = {0, 0, 0, 0, 0}, // 速度环状态
    .pos = {0, 0, 0, 0, 0}, // 位置环状态
    .yaw = {0, 0, 0, 0, 0}, // 偏航环状态
    // pid参数设定
    .ang_pid = {0.6f, 10.0f, 0.016f, 100000, 250}, // 直立环参数
    .spd_pid = {0.003f, 0.00f, 0.00f, 100000, 5}, // 速度环参数
    .pos_pid = {0.00f, 0.00f, 0.00f, 100000, 5}, // 位置环参数
    .yaw_pid = {0.025f, 0.00f, 0.00f, 100000, 5}, // 偏航环参数：P为转向力度，D为阻尼
};

void my_motion_init()
{
    my_mpu6050_init();

    my_motor_init();
}

void my_motion_update()
{
    my_mpu6050_update();
    // 更新robot状态数据
    robot_state_update();

    // 获取当前车辆角色
    const group_config &group_cfg = my_group_get_config();

    // 编队模式（头车或从车）：关闭PID，只根据摇杆直接控制
    if (group_cfg.espnow_enabled && group_cfg.role != VehicleRole::STANDALONE)
    {
        // 从车模式：使用接收到的指令
        if (group_cfg.role == VehicleRole::FOLLOWER)
        {
            // 检查指令超时
            if (my_group_is_command_timeout())
            {
                // 超时保护：停车
                motor_left_u = 0.0f;
                motor_right_u = 0.0f;
                robot.run = false;
            }
            else
            {
                // ESP-NOW回调已更新robot.motor.L_duty和R_duty
                // 需要同步到motor_left_u和motor_right_u供my_motor_update使用
                motor_left_u = robot.motor.L_duty;
                motor_right_u = robot.motor.R_duty;
            }
        }
        // 头车模式：根据摇杆直接生成duty
        else if (group_cfg.role == VehicleRole::LEADER)
        {
            // 直接摇杆控制，不经过PID
            // 前后：joy.y (-1.0 ~ 1.0)
            // 左右：joy.x (-1.0 ~ 1.0)
            float base_duty = robot.joy.y;  // 基础前后控制
            float turn_duty = robot.joy.x;  // 转向控制
            
            // 混合控制
            float left_cmd = base_duty + turn_duty;
            float right_cmd = base_duty - turn_duty;
            
            // 限幅
            left_cmd = constrain(left_cmd, -1.0f, 1.0f);
            right_cmd = constrain(right_cmd, -1.0f, 1.0f);
            
            // 重要：更新motor_left_u和motor_right_u，my_motor_update会使用这些值
            motor_left_u = left_cmd;
            motor_right_u = right_cmd;
            
            // 广播指令给从车
            my_group_send_command(left_cmd, right_cmd);
        }

        // 运行检查
        if (!robot.run)
        {
            motor_left_u = 0.0f;
            motor_right_u = 0.0f;
        }

        // 保留摔倒检测（安全保护）
        fall_check();
        if (robot.fallen.is)
        {
            motor_left_u = 0.0f;
            motor_right_u = 0.0f;
        }
    }
    else
    {
        // 单机模式：正常PID控制
        robot_pos_control();
        pitch_control();
        yaw_control();
        duty_add();
        pitch_zero_adapt();

        // 摔倒检测
        fall_check();

        // 运行检查
        if (!robot.run)
        {
            control_idle_reset(); // 清积分并重置目标，避免停机时积分累积导致启用瞬间大力输出
            robot.motor.L_duty = 0.0f;
            robot.motor.R_duty = 0.0f;
        }
    }

    // 电机执行（所有模式）
    my_motor_update();

    // 从车：发送心跳给头车
    if (group_cfg.role == VehicleRole::FOLLOWER && group_cfg.espnow_enabled)
    {
        my_group_send_heartbeat();
    }

    // 头车：更新从车在线状态
    if (group_cfg.role == VehicleRole::LEADER && group_cfg.espnow_enabled)
    {
        my_group_update_followers_status();
    }

    // 记录本帧摇杆，用于下次检测松杆/回零
    robot.joy_l = robot.joy;
}
