#include "my_motion.h"
#include "my_mpu6050.h"
#include "Arduino.h"


MPU6050 mpu6050 = MPU6050(Wire);
void my_mpu6050_setzero()
{
    my_mpu6050_update();
    robot.imu_zero.anglex = robot.imu.anglex;
    robot.imu_zero.angley = robot.imu.angley;
    robot.imu_zero.anglez = robot.imu.anglez;
    robot.imu_zero.gyrox = robot.imu.gyrox;
    robot.imu_zero.gyroy = robot.imu.gyroy;
    robot.imu_zero.gyroz = robot.imu.gyroz;
}

void my_mpu6050_init()
{
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);
    Serial.println("MPU6050初始化完成");
    delay(1000);
    my_mpu6050_setzero();
    Serial.println("MPU6050初始状态设置完毕");
}

void my_mpu6050_update()
{
    robot.imu_l.anglex = robot.imu.anglex;
    robot.imu_l.angley = robot.imu.angley;
    robot.imu_l.anglez = robot.imu.anglez;
    robot.imu_l.gyrox = robot.imu.gyrox;
    robot.imu_l.gyroy = robot.imu.gyroy;
    robot.imu_l.gyroz = robot.imu.gyroz;
    mpu6050.update();
    robot.imu.anglex = mpu6050.getAngleX();
    robot.imu.angley = mpu6050.getAngleY();
    robot.imu.anglez = mpu6050.getAngleZ();
    robot.imu.gyrox = mpu6050.getGyroX();
    robot.imu.gyroy = mpu6050.getGyroY();
    robot.imu.gyroz = mpu6050.getGyroZ();
}
