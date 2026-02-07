#pragma once
#include <MPU6050_tockn.h>

extern MPU6050 mpu6050;
// MPU6050实例
extern void my_mpu6050_init();
extern void my_mpu6050_setzero();
extern void my_mpu6050_update();