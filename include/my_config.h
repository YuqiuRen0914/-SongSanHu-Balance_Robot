#pragma once

#include <stdint.h>

/********** MPU6050 **********/
#define I2C0_SDA 42
#define I2C0_SCL 41
#define I2C_FREQUENCY 400000

/********** RGB(WS2812) **********/
#define RGB_LED_PIN         38
#define RGB_LED_COUNT       5

/********** 板载RGB(WS2812) **********/
#define BOARD_RGB_PIN       48  // 板载WS2812 LED
#define BOARD_RGB_COUNT     1   // 板载LED数量

/********** AB编码器  **********/
/* 左轮编码器 */
#define ENCODER_B1A_PIN      GPIO_NUM_17  
#define ENCODER_B1B_PIN      GPIO_NUM_18    

/* 右轮编码器 */
#define ENCODER_B2A_PIN      GPIO_NUM_3
#define ENCODER_B2B_PIN      GPIO_NUM_8

/********** 电机驱动 TB6612 **********/
/* 左轮电机 */
#define MOTOR_A_IN1_PIN      7
#define MOTOR_A_IN2_PIN      15
#define MOTOR_A_PWM_PIN      16

/* 右轮电机 */
#define MOTOR_B_IN1_PIN      6
#define MOTOR_B_IN2_PIN      5
#define MOTOR_B_PWM_PIN      4

/********** PWM 参数 (LEDC) **********/
#define PWM_RESOLUTION      12  // PWM 分辨率 12 位
#define PWM_FREQ            5000
#define MIN_DUTY            0
#define MAX_DUTY            ((1UL << PWM_RESOLUTION) - 1)

/********** 屏幕配置 **********/
#define SCREEN_SDA_PIN      46
#define SCREEN_SCL_PIN      9
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       32
#define SCREEN_REFRESH_TIME 100   // ms
#define SCREEN_I2C_ADDRESS  0x78  // 0x78 on label; driver will shift to 7-bit (0x3C)

/********** 控制死区 **********/
#define PITCH_ANG_DEADBAND 0.0f   // pitch角度死区，单位：度
#define PITCH_SPD_DEADBAND 0.0f   // 速度环死区，单位：rad/s
#define PITCH_TOR_DEADBAND 0.0f  // 力矩输出死区

/********** 速度环转换为Pitch角度配置 **********/
static constexpr float RAD_TO_DEG_F = 57.29577951308232f;
static constexpr float PITCH_ANGLE_OFFSET_LIMIT = 10.0f;   // 最大前倾/后仰角度修正

/********** Yaw配置 **********/
static constexpr float YAW_RATE_MAX_DEG_S = 200.0f;        // 摇杆满量程对应的偏航角速度
static constexpr float YAW_RATE_CMD_DEADBAND = 0.5f;       // 摇杆转换的角速度死区
static constexpr float YAW_TORQUE_DEADBAND = 0.02f;        // 偏航输出死区，避免轻微抖动

/********** wifi配置 **********/
#define SSID_PREFIX "BalBot"  // SSID前缀，后面会自动加MAC后缀（例：BalBot_AABB）
#define SSID "Balance_Robot"  // 兼容旧代码的默认SSID
#define PASSWORD "123456789"

/********** 车队配置 **********/
// ESP-NOW配置常量已在my_group.h中定义
// 这里仅保留应用层配置
#define GROUP_COMMAND_SEND_INTERVAL_MS 2  // 头车发送指令间隔（与dt_ms一致）

/********** 电池检测 **********/
#define BAT_PIN 10

/********** 结构数据体 **********/
struct imu_data
{
    float anglex;
    float angley;
    float anglez;
    float gyrox;
    float gyroy;
    float gyroz;
};

struct pid_config
{
    float p, i, d;
    float k; // 斜率
    float l; // 最值限制
};

struct motion_state
{
    float now;
    float last;
    float tar;
    float err;
    float duty;
};
struct joy_state
{
    float x;
    float y;
    float a; // 方向角
    float r;
    float x_coef;
    float y_coef;
};
struct fallen_state
{
    bool is;     // 是否摔倒
    int count;   // 计数
    bool enable; // 检测是否启用
};


struct motor_duty
{
    float base_duty;
    float yaw_duty;
    float L_duty;            // 实际写入 PWM（含死区补偿）
    float R_duty;
    float L_cmd;             // 归一化控制指令（-1~1）
    float R_cmd;
    float L_deadzone_fwd;    // 左轮正向起转占空比
    float L_deadzone_rev;    // 左轮反向起转占空比
    float R_deadzone_fwd;    // 右轮正向起转占空比
    float R_deadzone_rev;    // 右轮反向起转占空比
};

struct wel_data
{
    float spd1;
    float spd2;
    float pos1;
    float pos2;
};

struct rgb_state
{
    int rgb_count; // 灯珠数量
    int mode;      // 灯光模式 0-3
};

struct robot_state
{
    int dt_ms;
    int data_ms;

    bool run;              //运行指示位
    bool chart_enable;     //图表推送位
    bool joy_stop_control; //原地停车标志
    bool wel_up;           //轮部离地标志

    float pitch_zero;
    wel_data wel;
    motor_duty motor;

    imu_data imu_zero;
    imu_data imu_l;
    imu_data imu;

    joy_state joy;
    joy_state joy_l;

    fallen_state fallen;

    rgb_state rgb;

    motion_state ang;
    motion_state spd;
    motion_state pos;
    motion_state yaw;

    pid_config ang_pid;
    pid_config spd_pid;
    pid_config pos_pid;
    pid_config yaw_pid;
};
