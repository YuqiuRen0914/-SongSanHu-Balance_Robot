#pragma once

#include <stdint.h>

// 板载RGB LED控制模块
// 用于设备状态指示

// LED颜色定义
enum BoardRGBColor
{
    BOARD_RGB_OFF = 0,      // 关闭
    BOARD_RGB_RED,          // 红色 - 错误/故障
    BOARD_RGB_GREEN,        // 绿色 - 正常/成功
    BOARD_RGB_BLUE,         // 蓝色 - 运行中
    BOARD_RGB_YELLOW,       // 黄色 - 警告
    BOARD_RGB_CYAN,         // 青色 - 信息
    BOARD_RGB_MAGENTA,      // 品红色 - 待机
    BOARD_RGB_WHITE         // 白色 - 测试
};

// 初始化板载RGB LED
void my_board_rgb_init();

// 设置板载RGB LED颜色
void my_board_rgb_set_color(BoardRGBColor color);

// 设置自定义RGB值 (0-255)
void my_board_rgb_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// 闪烁指定次数 (阻塞式)
void my_board_rgb_blink(BoardRGBColor color, uint8_t times, uint16_t delay_ms);

// 呼吸灯效果 (阻塞式，持续指定时间)
void my_board_rgb_breathe(BoardRGBColor color, uint16_t duration_ms);

