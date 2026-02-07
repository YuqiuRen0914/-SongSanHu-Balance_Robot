#include "Arduino.h"
#include "my_motor.h"
#include "my_config.h"
#include "my_encoder.h"
#include "my_motion.h"

volatile float motor_left_u = 0.0f;
volatile float motor_right_u = 0.0f;

namespace
{
    enum class MotorSide
    {
        Left,
        Right
    };

    enum class MotorState
    {
        Brake = 0,
        Forward,
        Reverse
    };

    // LEDC 通道分配
    constexpr uint8_t LEFT_PWM_CH = 0;
    constexpr uint8_t RIGHT_PWM_CH = 1;
    constexpr float CALI_STEP = 0.05f;   // 起转测试步长（占空比）
    constexpr uint32_t CALI_DELAY_MS = 40; // 每档等待时间

    // 起转死区占空比（正向/反向），运行中用于补偿
    float left_forward_start = 0.1f;
    float left_reverse_start = 0.1f;
    float right_forward_start = 0.1f;
    float right_reverse_start = 0.1f;

    bool is_left_side(MotorSide side)
    {
        return side == MotorSide::Left;
    }

    // 设置方向：正转/反转/制动
    void set_dir(MotorSide side, MotorState state)
    {
        const int in1 = is_left_side(side) ? MOTOR_A_IN1_PIN : MOTOR_B_IN1_PIN;
        const int in2 = is_left_side(side) ? MOTOR_A_IN2_PIN : MOTOR_B_IN2_PIN;
        switch (state)
        {
        case MotorState::Forward:
            digitalWrite(in1, HIGH);
            digitalWrite(in2, LOW);
            break;
        case MotorState::Reverse:
            digitalWrite(in1, LOW);
            digitalWrite(in2, HIGH);
            break;
        case MotorState::Brake:
        default:
            digitalWrite(in1, LOW);
            digitalWrite(in2, LOW); // 悬空/滑行
            break;
        }
    }

    void write_pwm(MotorSide side, float duty)
    {
        duty = constrain(duty, 0.0f, 1.0f);
        const uint32_t counts = static_cast<uint32_t>(duty * static_cast<float>(MAX_DUTY));
        ledcWrite(is_left_side(side) ? LEFT_PWM_CH : RIGHT_PWM_CH, counts);
    }

    // 逐级递增占空比，检测到编码器有脉冲即返回当前占空比
    float calibrate_duty(MotorSide side, MotorState state)
    {
        // 设定方向
        set_dir(side, state);

        // 清零编码器
        my_encoder_update();
        float duty = CALI_STEP;
        while (duty <= 1.0f)
        {
            write_pwm(side, duty);

            delay(CALI_DELAY_MS);
            my_encoder_update(); // 读出增量

            const int32_t delta = is_left_side(side) ? Encoder_Left_Delta : Encoder_Right_Delta;
            if (abs(delta) > 0)
            {
                // 一旦检测到有转动，立即停转并返回当前 duty
                write_pwm(side, 0.0f);
                set_dir(side, MotorState::Brake);
                return duty;
            }

            duty += CALI_STEP;
        }

        // 没检测到脉冲，则返回最大值，防止后续补偿时超限
        write_pwm(side, 0.0f);
        set_dir(side, MotorState::Brake);
        return 1.0f;
    }

    // 施加死区补偿并输出 PWM，返回实际写入的占空比（带符号）
    float drive_motor(MotorSide side, float cmd)
    {
        cmd = constrain(cmd, -1.0f, 1.0f);
        if (abs(cmd) < 1e-5f)
        {
            // 停车
            set_dir(side, MotorState::Brake);
            write_pwm(side, 0.0f);
            return 0.0f;
        }

        const bool forward = cmd > 0;
        const float mag = abs(cmd);
        const float start = forward
                                ? (is_left_side(side) ? left_forward_start : right_forward_start)
                                : (is_left_side(side) ? left_reverse_start : right_reverse_start);
        const float duty = start + (1.0f - start) * mag;

        // 设置方向与 PWM
        set_dir(side, forward ? MotorState::Forward : MotorState::Reverse);
        write_pwm(side, duty);
        return forward ? duty : -duty;
    }
}

void my_motor_init()
{
    // 引脚模式
    pinMode(MOTOR_A_IN1_PIN, OUTPUT);
    pinMode(MOTOR_A_IN2_PIN, OUTPUT);
    pinMode(MOTOR_B_IN1_PIN, OUTPUT);
    pinMode(MOTOR_B_IN2_PIN, OUTPUT);

    // PWM 配置
    ledcSetup(LEFT_PWM_CH, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(RIGHT_PWM_CH, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(MOTOR_A_PWM_PIN, LEFT_PWM_CH);
    ledcAttachPin(MOTOR_B_PWM_PIN, RIGHT_PWM_CH);

    // 确保编码器可用并清零
    my_encoder_init();
    my_encoder_update();

    // 起转死区校准：左/右轮正反向
    left_forward_start = calibrate_duty(MotorSide::Left, MotorState::Forward);
    left_reverse_start = calibrate_duty(MotorSide::Left, MotorState::Reverse);
    right_forward_start = calibrate_duty(MotorSide::Right, MotorState::Forward);
    right_reverse_start = calibrate_duty(MotorSide::Right, MotorState::Reverse);

    // 记录死区占空比，方便遥测查看
    robot.motor.L_deadzone_fwd = left_forward_start;
    robot.motor.L_deadzone_rev = left_reverse_start;
    robot.motor.R_deadzone_fwd = right_forward_start;
    robot.motor.R_deadzone_rev = right_reverse_start;

    // 校准后重置编码器计数，避免位置有偏移
    my_encoder_init();

    // 初始化输出为 0
    set_dir(MotorSide::Left, MotorState::Brake);
    set_dir(MotorSide::Right, MotorState::Brake);
    write_pwm(MotorSide::Left, 0.0f);
    write_pwm(MotorSide::Right, 0.0f);

    motor_left_u = 0.0f;
    motor_right_u = 0.0f;
    robot.motor.L_duty = 0.0f;
    robot.motor.R_duty = 0.0f;
    robot.motor.L_cmd = 0.0f;
    robot.motor.R_cmd = 0.0f;
}

void my_motor_update()
{
    // 读取指令并驱动，两侧互不干扰
    robot.motor.L_cmd = motor_left_u;
    robot.motor.R_cmd = motor_right_u;
    const float left_applied = drive_motor(MotorSide::Left, motor_left_u);
    const float right_applied = drive_motor(MotorSide::Right, motor_right_u);

    // 存入全局状态，便于监控/上报
    robot.motor.L_duty = left_applied;
    robot.motor.R_duty = right_applied;
}
