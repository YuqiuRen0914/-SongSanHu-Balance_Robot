#pragma once

// 开机外设检查模块
// 检查屏幕、IMU、电机和编码器是否正常连接

// 执行开机检查
// 返回值：true表示所有外设正常，false表示有外设异常
bool my_inspect_check_all();

// 单独检查各个外设
bool my_inspect_check_screen();   // 检查屏幕（I2C设备）
bool my_inspect_check_imu();      // 检查IMU（I2C设备）
bool my_inspect_check_motors();   // 检查电机和编码器（通过死区测量）

