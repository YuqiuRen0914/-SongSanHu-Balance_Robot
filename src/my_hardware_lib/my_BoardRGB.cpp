#include "my_BoardRGB.h"
#include "my_config.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// 板载RGB LED对象
static Adafruit_NeoPixel board_strip(BOARD_RGB_COUNT, BOARD_RGB_PIN, NEO_GRB + NEO_KHZ800);

namespace
{
    // 预定义颜色（RGB格式）
    struct ColorRGB
    {
        uint8_t r, g, b;
    };

    const ColorRGB COLOR_MAP[] = {
        {0, 0, 0},       // BOARD_RGB_OFF - 关闭
        {255, 0, 0},     // BOARD_RGB_RED - 红色（错误/故障）
        {0, 255, 0},     // BOARD_RGB_GREEN - 绿色（正常/成功）
        {0, 0, 255},     // BOARD_RGB_BLUE - 蓝色（运行中）
        {255, 255, 0},   // BOARD_RGB_YELLOW - 黄色（警告）
        {0, 255, 255},   // BOARD_RGB_CYAN - 青色（信息）
        {255, 0, 255},   // BOARD_RGB_MAGENTA - 品红色（待机）
        {255, 255, 255}  // BOARD_RGB_WHITE - 白色（测试）
    };

    // 当前颜色状态
    uint8_t current_r = 0;
    uint8_t current_g = 0;
    uint8_t current_b = 0;
}

void my_board_rgb_init()
{
    // 初始化NeoPixel库
    board_strip.begin();
    
    // 设置亮度（0-255），降低亮度以免刺眼
    board_strip.setBrightness(50);
    
    // 初始化时关闭LED
    board_strip.clear();
    board_strip.show();
    
    Serial.println("板载RGB LED初始化完成 (WS2812B)");
}

void my_board_rgb_set_color(BoardRGBColor color)
{
    if (color >= 0 && color <= BOARD_RGB_WHITE)
    {
        const ColorRGB &c = COLOR_MAP[color];
        my_board_rgb_set_rgb(c.r, c.g, c.b);
    }
    else
    {
        // 未知颜色，关闭LED
        my_board_rgb_set_rgb(0, 0, 0);
    }
}

void my_board_rgb_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    current_r = r;
    current_g = g;
    current_b = b;
    
    // 设置LED颜色（索引0是第一个LED）
    board_strip.setPixelColor(0, board_strip.Color(r, g, b));
    board_strip.show();
}

void my_board_rgb_blink(BoardRGBColor color, uint8_t times, uint16_t delay_ms)
{
    for (uint8_t i = 0; i < times; i++)
    {
        my_board_rgb_set_color(color);
        delay(delay_ms);
        my_board_rgb_set_color(BOARD_RGB_OFF);
        delay(delay_ms);
    }
}

void my_board_rgb_breathe(BoardRGBColor color, uint16_t duration_ms)
{
    if (color < 0 || color > BOARD_RGB_WHITE)
    {
        return;
    }
    
    const ColorRGB &c = COLOR_MAP[color];
    uint32_t start_ms = millis();
    
    while (millis() - start_ms < duration_ms)
    {
        // 呼吸效果：从暗到亮再到暗
        // 使用正弦波模拟呼吸效果
        uint32_t elapsed = millis() - start_ms;
        float phase = (elapsed % 2000) / 2000.0f;  // 2秒一个周期
        
        // 使用简化的呼吸曲线（0到1再到0）
        float brightness;
        if (phase < 0.5f)
        {
            brightness = phase * 2.0f;  // 0 -> 0.5 => 0 -> 1
        }
        else
        {
            brightness = (1.0f - phase) * 2.0f;  // 0.5 -> 1 => 1 -> 0
        }
        
        // 应用亮度
        uint8_t r = (uint8_t)(c.r * brightness);
        uint8_t g = (uint8_t)(c.g * brightness);
        uint8_t b = (uint8_t)(c.b * brightness);
        
        board_strip.setPixelColor(0, board_strip.Color(r, g, b));
        board_strip.show();
        
        delay(20);  // 更新间隔
    }
    
    // 结束时关闭
    my_board_rgb_set_color(BOARD_RGB_OFF);
}
