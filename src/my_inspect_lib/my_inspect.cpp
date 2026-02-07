#include "my_inspect.h"
#include "my_BoardRGB.h"
#include "my_I2C.h"
#include "my_config.h"
#include "my_motion.h"
#include <Arduino.h>
#include <Wire.h>

// I2C设备地址
#define MPU6050_ADDRESS     0x68  // MPU6050的I2C地址
#define SCREEN_ADDRESS_7BIT 0x3C  // 屏幕的7位地址

namespace
{
    // 检查I2C设备是否存在
    bool check_i2c_device(TwoWire &wire, uint8_t address)
    {
        wire.beginTransmission(address);
        uint8_t error = wire.endTransmission();
        return (error == 0);
    }
}

bool my_inspect_check_screen()
{
    Serial.print("检查屏幕...");
    
    bool result = check_i2c_device(ScreenWire, SCREEN_ADDRESS_7BIT);
    
    if (result)
    {
        Serial.println(" [OK]");
    }
    else
    {
        Serial.println(" [FAIL]");
    }
    
    return result;
}

bool my_inspect_check_imu()
{
    Serial.print("检查IMU(MPU6050)...");
    
    bool result = check_i2c_device(Wire, MPU6050_ADDRESS);
    
    if (result)
    {
        Serial.println(" [OK]");
    }
    else
    {
        Serial.println(" [FAIL]");
    }
    
    return result;
}

bool my_inspect_check_motors()
{
    Serial.print("检查电机和编码器（死区测量）...");
    
    // 检查电机和编码器的方法：
    // 在my_motor_init()中会进行死区校准
    // 如果校准后的死区值接近1.0（最大值），说明没有检测到编码器脉冲
    // 这表示电机或编码器未连接
    
    // 获取死区测量结果
    float l_fwd = robot.motor.L_deadzone_fwd;
    float l_rev = robot.motor.L_deadzone_rev;
    float r_fwd = robot.motor.R_deadzone_fwd;
    float r_rev = robot.motor.R_deadzone_rev;
    
    Serial.println();
    Serial.printf("  左轮死区: 前向=%.3f, 反向=%.3f\n", l_fwd, l_rev);
    Serial.printf("  右轮死区: 前向=%.3f, 反向=%.3f\n", r_fwd, r_rev);
    
    // 判断标准：如果死区值大于0.95，认为测量失败（没有检测到编码器脉冲）
    const float DEADZONE_FAIL_THRESHOLD = 0.95f;
    
    bool left_ok = (l_fwd < DEADZONE_FAIL_THRESHOLD) && (l_rev < DEADZONE_FAIL_THRESHOLD);
    bool right_ok = (r_fwd < DEADZONE_FAIL_THRESHOLD) && (r_rev < DEADZONE_FAIL_THRESHOLD);
    
    bool result = left_ok && right_ok;
    
    if (result)
    {
        Serial.println("  电机和编码器 [OK]");
    }
    else
    {
        if (!left_ok)
        {
            Serial.println("  左轮电机/编码器 [FAIL]");
        }
        if (!right_ok)
        {
            Serial.println("  右轮电机/编码器 [FAIL]");
        }
    }
    
    return result;
}

bool my_inspect_check_all()
{
    Serial.println("========================================");
    Serial.println("开始外设检查...");
    Serial.println("========================================");
    
    // 初始化板载RGB LED
    my_board_rgb_init();
    
    bool all_ok = true;
    
    // 1. 检查IMU（在I2C总线0上）
    bool imu_ok = my_inspect_check_imu();
    all_ok = all_ok && imu_ok;
    
    // 2. 检查屏幕（在I2C总线1上）
    bool screen_ok = my_inspect_check_screen();
    all_ok = all_ok && screen_ok;
    
    // 3. 检查电机和编码器
    // 注意：此检查应该在my_motor_init()之后进行
    // 因为死区校准是在motor_init中完成的
    bool motor_ok = my_inspect_check_motors();
    all_ok = all_ok && motor_ok;
    
    // 显示总体结果
    Serial.println("========================================");
    if (all_ok)
    {
        Serial.println("外设检查完成：所有设备正常");
        // 成功：绿灯闪烁5次
        my_board_rgb_blink(BOARD_RGB_GREEN, 5, 400);
        my_board_rgb_set_color(BOARD_RGB_OFF);
    }
    else
    {
        Serial.println("外设检查完成：部分设备异常");
        // 失败：红灯闪烁5次
        my_board_rgb_blink(BOARD_RGB_RED, 5, 500);
        my_board_rgb_set_color(BOARD_RGB_OFF);
    }
    Serial.println("========================================");
    
    return all_ok;
}
