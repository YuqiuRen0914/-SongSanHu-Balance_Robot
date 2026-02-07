#include <Arduino.h>
#include <driver/i2c.h>
#include "my_I2C.h"
#include "my_config.h"

TwoWire ScreenWire = TwoWire(1);

void my_i2c_init()
{
    Wire.begin(I2C0_SDA, I2C0_SCL, I2C_FREQUENCY);
    ScreenWire.begin(SCREEN_SDA_PIN, SCREEN_SCL_PIN, I2C_FREQUENCY);

    const int scl_timeout = 1 << 19;
    i2c_set_timeout(I2C_NUM_0, scl_timeout);
    i2c_set_timeout(I2C_NUM_1, scl_timeout);

    Wire.setTimeOut(100);
    ScreenWire.setTimeOut(100);

    Serial.println("I2C初始化完成 (总线0: IMU等, 总线1: OLED)");
}
