#include "my_control.h"
#include "my_motion.h"
#include "my_mpu6050.h"
#include "my_encoder.h"
#include "my_tool.h"
#include "my_motor.h"

PIDController PID_ANG{robot.ang_pid.p, robot.ang_pid.i, 0, robot.ang_pid.k, robot.ang_pid.l};               // 直立控制
PIDController PID_SPD{robot.spd_pid.p, robot.spd_pid.i, robot.spd_pid.d, robot.spd_pid.k, robot.spd_pid.l}; // 速度控制
PIDController PID_YAW{robot.yaw_pid.p, robot.yaw_pid.i, 0, robot.yaw_pid.k, robot.yaw_pid.l};               // 偏航控制
PIDController PID_POS{robot.pos_pid.p, robot.pos_pid.i, robot.pos_pid.d, robot.pos_pid.k, robot.pos_pid.l}; // 位置控制

LowPassFilter LQF_ZEROPOINT{0.1};
LowPassFilter LQF_JOY{0.2};

namespace
{
    // 清零归一化指令并同步到监测字段
    inline void clear_motor_commands()
    {
        motor_left_u = 0.0f;
        motor_right_u = 0.0f;
        robot.motor.L_cmd = 0.0f;
        robot.motor.R_cmd = 0.0f;
    }
}

void pid_state_update()
{
    // 更新PID控制器状态
    PID_ANG.P = robot.ang_pid.p;
    PID_ANG.I = robot.ang_pid.i;
    // PID_ANG.D = robot.ang_pid.d; 直接使用陀螺仪

    PID_SPD.P = robot.spd_pid.p;
    PID_SPD.I = robot.spd_pid.i;
    PID_SPD.D = robot.spd_pid.d;

    PID_POS.P = robot.pos_pid.p;
    PID_POS.I = robot.pos_pid.i;
    PID_POS.D = robot.pos_pid.d;

    PID_YAW.P = robot.yaw_pid.p;
    PID_YAW.I = robot.yaw_pid.i;
    PID_YAW.D = robot.yaw_pid.d;
}

void robot_state_update()
{
    // 上一时刻状态
    robot.ang.last = robot.ang.now;
    robot.spd.last = robot.spd.now;
    robot.pos.last = robot.pos.now;
    robot.yaw.last = robot.yaw.now;
    // 同步机器人状态，spd 和 pos 状态由编码器更新
    my_encoder_update();
    // 更新当前状态
    robot.ang.now = robot.imu.angley;
    robot.spd.now = -0.5f * (robot.wel.spd1 + robot.wel.spd2); // rad/s
    robot.pos.now = -0.5f * (robot.wel.pos1 + robot.wel.pos2); // rad
    robot.yaw.now = robot.imu.gyroz; // 将 yaw 环的状态改为角速度
}

void robot_pos_control()
{
    if (robot.joy.y != 0) // 有前后方向运动指令时的处理
    {
        robot.pos.tar = robot.pos.now; // 位移零点重置
    }
    if ((robot.joy_l.x != 0 && robot.joy.x == 0) || (robot.joy_l.y != 0 && robot.joy.y == 0)) // 运动指令复零时的原地停车处理
        robot.joy_stop_control = true;
    if ((robot.joy_stop_control == true) && (abs(robot.spd.now) < 0.5))
    {
        robot.pos.tar = robot.pos.now; // 位移零点重置
        robot.joy_stop_control = false;
    }
    if (abs(robot.spd.now) > 15)       // 被快速推动时的原地停车处理
        robot.pos.tar = robot.pos.now; // 位移零点重置
}

void pitch_control()
{
    // 串级：位置 -> 速度 -> 角度
    robot.pos.err = robot.pos.now - robot.pos.tar;
    robot.pos.duty = PID_POS(robot.pos.err); // 位置环输出作为速度目标修正量

    float joy_spd_tar = robot.joy.y_coef * LQF_JOY(robot.joy.y);
    robot.spd.tar = joy_spd_tar - robot.pos.duty; // 速度目标 = 摇杆期望 - 位置环修正

    robot.spd.err = robot.spd.now - robot.spd.tar;
    if (fabsf(robot.spd.err) < PITCH_SPD_DEADBAND)
        robot.spd.err = 0.0f;
    robot.spd.duty = PID_SPD(robot.spd.err); // 速度环输出用作角度目标修正

    float pitch_offset = my_lim(robot.spd.duty * RAD_TO_DEG_F, PITCH_ANGLE_OFFSET_LIMIT);
    robot.ang.tar = robot.pitch_zero - pitch_offset;
    robot.ang.err = robot.ang.now - robot.ang.tar;
    if (fabsf(robot.ang.err) < PITCH_ANG_DEADBAND)
        robot.ang.err = 0.0f;
    robot.ang.duty = PID_ANG(robot.ang.err) + my_lim(robot.ang_pid.d * robot.imu.gyroy, robot.ang_pid.l);

    // 轮部离地检测
    if (abs(robot.spd.now - robot.spd.last) > 10 || abs(robot.spd.now) > 50) // 若轮部角速度、角加速度过大或处于跳跃后的恢复时期，认为出现轮部离地现象，需要特殊处理
    {
        robot.pos.tar = robot.pos.now; // 位移零点重置
        robot.motor.base_duty = robot.ang.duty;
    }
    else
        robot.motor.base_duty = robot.ang.duty;
    if (fabsf(robot.motor.base_duty) < PITCH_TOR_DEADBAND)
        robot.motor.base_duty = 0.0f;
}

void yaw_control()
{
    // 只在有明确转向指令时输出，不做航向保持
    const float yaw_cmd_rate = robot.joy.x * robot.joy.x_coef * YAW_RATE_MAX_DEG_S;
    const float yaw_rate_now = robot.imu.gyroz;

    robot.yaw.tar = yaw_cmd_rate;      // 记录目标角速度便于遥测显示
    robot.yaw.now = yaw_rate_now;
    robot.yaw.err = yaw_cmd_rate - yaw_rate_now; // 仅用于图表显示

    if (fabsf(yaw_cmd_rate) < YAW_RATE_CMD_DEADBAND)
    {
        PID_YAW.reset();               // 避免累积
        robot.motor.yaw_duty = -robot.yaw_pid.d * yaw_rate_now; // 只做一点阻尼
        if (fabsf(robot.motor.yaw_duty) < YAW_TORQUE_DEADBAND)
            robot.motor.yaw_duty = 0.0f;
        return;
    }

    // 前馈比例转向 + 角速度阻尼，不维持航向
    const float yaw_ff = robot.yaw_pid.p * yaw_cmd_rate;
    const float yaw_damp = robot.yaw_pid.d * yaw_rate_now;

    robot.motor.yaw_duty = my_lim(yaw_ff - yaw_damp, robot.yaw_pid.l);
}

void pitch_zero_adapt()
{
    // 触发条件：遥控器无信号输入、位置环已介入且左右归一化指令均很小
    float cmd_abs = fabsf(motor_left_u);
    const float right_abs = fabsf(motor_right_u);
    if (right_abs > cmd_abs)
        cmd_abs = right_abs;

    if (cmd_abs < 0.1f && robot.joy.y == 0 && fabsf(robot.pos.duty) < 4)
    {
        robot.pitch_zero -= my_lim(0.002f * LQF_ZEROPOINT(robot.pos.duty), 4); // 重心自适应
    }
}

void duty_add()
{
    const float base = robot.motor.base_duty;
    const float yaw = robot.motor.yaw_duty;
    const float left_mix = my_lim(base + yaw, DUTY_SUM_LIM);
    const float right_mix = my_lim(base - yaw, DUTY_SUM_LIM);

    motor_left_u = my_lim(-left_mix / DUTY_SUM_LIM, 1.0f);
    motor_right_u = my_lim(-right_mix / DUTY_SUM_LIM, 1.0f);

    robot.motor.L_cmd = motor_left_u;
    robot.motor.R_cmd = motor_right_u;
}

void control_idle_reset()
{
    PID_ANG.reset();
    PID_SPD.reset();
    PID_POS.reset();
    PID_YAW.reset();

    robot.pos.tar = robot.pos.now;
    robot.spd.tar = 0.0f;
    robot.motor.base_duty = 0.0f;
    robot.motor.yaw_duty = 0.0f;
    robot.motor.L_duty = 0.0f;
    robot.motor.R_duty = 0.0f;
    clear_motor_commands();
}

void fall_check()
{
    // 摔倒检测
    if (!robot.fallen.enable) // 没有启用直接返回
        return;

    const bool tipped = robot.imu.angley > FALL_MAX_PITCH || robot.imu.angley < FALL_MIN_PITCH;
    if (tipped)
        robot.fallen.count++;
    else
    {
        robot.fallen.count = 0;
        robot.fallen.is = false;
    }
    if (robot.fallen.count >= COUNT_FALL_MAX)
        robot.fallen.is = true;

    if (robot.fallen.is)
    {
        // 摔倒后清指令/积分，避免立起后暴冲
        robot.joy.x = 0.0f;
        robot.joy.y = 0.0f;
        control_idle_reset();
        robot.motor.L_duty = 0.0f;
        robot.motor.R_duty = 0.0f;
        clear_motor_commands();
    }
}
