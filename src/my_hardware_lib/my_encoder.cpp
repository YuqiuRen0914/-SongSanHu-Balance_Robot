#include "Arduino.h"
#include "my_encoder.h"
#include "my_config.h"
#include "my_motion.h"
#include "driver/gpio.h"  
#include "driver/pcnt.h" 

volatile int32_t Encoder_Left_Delta = 0; 
volatile int32_t Encoder_Right_Delta = 0;

namespace
{
    //这些值（PCNT 单元号、计数上下限、滤波阈值等）是固定配置，不会在运行时变化，用 constexpr 明确它们是常量。
    constexpr pcnt_unit_t LeftUnit = PCNT_UNIT_0;   // 左轮使用 PCNT 单元 0
    constexpr pcnt_unit_t RightUnit = PCNT_UNIT_1;  // 右轮使用 PCNT 单元 1
    constexpr int16_t CountLimit = 30000;           // 计数上下限，留出溢出余量
    constexpr uint16_t FilterValue = 100;           // PCNT 滤波阈值，单位 APB 周期
    constexpr float GearRatio = 35.0f;              // 减速比：电机 35 圈 = 轮子 1 圈
    constexpr float EncoderLines = 385.0f;          // 编码器线数
    constexpr float CountsPerMotorRev = EncoderLines * 2.0f; // A 相上下沿计数
    constexpr float CountsPerWheelRev = CountsPerMotorRev * GearRatio;
    constexpr float RadPerCount = (2.0f * PI) / CountsPerWheelRev; // 单个脉冲对应的轮子转角(rad)

    bool encoder_ready = false;                     // 记录初始化是否完成
    int64_t LeftTotalCount = 0;                     // 左轮累计脉冲数
    int64_t RightTotalCount = 0;                    // 右轮累计脉冲数

    bool my_pcnt_init(pcnt_unit_t unit, gpio_num_t pin_a, gpio_num_t pin_b) // 配置某个 PCNT 单元
    {
        pcnt_config_t channel_config = {};
        channel_config.pulse_gpio_num = pin_a;
        channel_config.ctrl_gpio_num = pin_b; 
        channel_config.unit = unit;                  
        channel_config.channel = PCNT_CHANNEL_0; 

        // A 相上升沿记为 +1,A 相下降沿记为 -1      
        channel_config.pos_mode = PCNT_COUNT_INC;
        channel_config.neg_mode = PCNT_COUNT_DEC;  

        //控制脚低电平时保持方向,控制脚高电平时翻转方向
        channel_config.lctrl_mode = PCNT_MODE_KEEP;
        channel_config.hctrl_mode = PCNT_MODE_REVERSE; 

        // 设置上下限，避免硬件溢出
        channel_config.counter_h_lim = CountLimit;  
        channel_config.counter_l_lim = -CountLimit; 

        // 应用配置到硬件
        esp_err_t ret = pcnt_unit_config(&channel_config);
        if (ret == ESP_ERR_INVALID_STATE)
        {
            // 已配置过该单元，视为成功继续清零
            ret = ESP_OK;
        }
        if (ret != ESP_OK)
        {
            Serial.printf("[ENC] pcnt_unit_config unit=%d failed (%d)\n", unit, ret);
            return false;
        }

        (void)pcnt_set_filter_value(unit, FilterValue); // 设置毛刺滤波阈值
        (void)pcnt_filter_enable(unit);                  // 打开滤波单元
        (void)pcnt_counter_pause(unit);                  // 暂停计数器以确保清零
        (void)pcnt_counter_clear(unit);                  // 将当前计数清零
        (void)pcnt_counter_resume(unit);                 // 重新启动计数器 
        return true;
    }
}

void my_encoder_init()
{ 
    pinMode(ENCODER_B1A_PIN, INPUT_PULLUP); 
    pinMode(ENCODER_B1B_PIN, INPUT_PULLUP); 
    pinMode(ENCODER_B2A_PIN, INPUT_PULLUP); 
    pinMode(ENCODER_B2B_PIN, INPUT_PULLUP);

    const bool left_ok = my_pcnt_init(LeftUnit, ENCODER_B1A_PIN, ENCODER_B1B_PIN);
    const bool right_ok = my_pcnt_init(RightUnit, ENCODER_B2A_PIN, ENCODER_B2B_PIN);

    Encoder_Left_Delta = 0;  // 清零左轮增量缓存
    Encoder_Right_Delta = 0; // 清零右轮增量缓存
    LeftTotalCount = 0;
    RightTotalCount = 0;
    encoder_ready = left_ok && right_ok;    
}

// 刷新并清零编码器计数
void my_encoder_update() 
{ 
    if (!encoder_ready)
    { 
        return; 
    } 

    int16_t left_count = 0;  // 临时变量存储左轮读数
    int16_t right_count = 0; // 临时变量存储右轮读数
    (void)pcnt_get_counter_value(LeftUnit, &left_count);   
    (void)pcnt_get_counter_value(RightUnit, &right_count); 
    LeftTotalCount += left_count;
    RightTotalCount += right_count;

    Encoder_Left_Delta = left_count;
    Encoder_Right_Delta = right_count; 

    (void)pcnt_counter_clear(LeftUnit);
    (void)pcnt_counter_clear(RightUnit); 
    const float dt_s = robot.dt_ms * 0.001f;
    if (dt_s > 0.0f)
    {
        robot.wel.spd1 = static_cast<float>(Encoder_Left_Delta) * RadPerCount / dt_s;
        robot.wel.spd2 = static_cast<float>(Encoder_Right_Delta) * RadPerCount / dt_s;
    }
    else
    {
        robot.wel.spd1 = 0.0f;
        robot.wel.spd2 = 0.0f;
    }

    robot.wel.pos1 = static_cast<float>(LeftTotalCount) * RadPerCount;
    robot.wel.pos2 = static_cast<float>(RightTotalCount) * RadPerCount;
}
