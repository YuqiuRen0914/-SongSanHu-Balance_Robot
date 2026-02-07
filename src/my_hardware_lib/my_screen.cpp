#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "my_screen.h"
#include "my_config.h"
#include "my_bat.h"
#include "my_I2C.h"
#include "my_net.h"
#include "my_company_logo.h"

namespace
{
    constexpr uint32_t FRAME_INTERVAL_MS = SCREEN_REFRESH_TIME;
    constexpr uint8_t HEADER_HEIGHT = 14;
    constexpr uint8_t SSID_MAX_LEN = 14;
    constexpr uint8_t STRIPE_GAP = 6;
    constexpr uint8_t FRAME_THICKNESS = 2;
    constexpr uint8_t FLOW_SEG_LEN = 16;

    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &ScreenWire, -1);

    bool screen_ready = false;
    uint32_t last_frame_ms = 0;
    uint8_t anim_phase_fast = 0;

    uint8_t normalized_address(uint8_t raw)
    {
        if (raw == 0x78 || raw == 0x7A || raw > 0x7F)
        {
            return static_cast<uint8_t>(raw >> 1);
        }
        return raw;
    }

    String compact_ssid(const String &ssid)
    {
        if (ssid.length() <= SSID_MAX_LEN)
        {
            return ssid;
        }
        return ssid.substring(0, SSID_MAX_LEN - 2) + "..";
    }

    void draw_stripes(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t gap, uint8_t phase)
    {
        const uint8_t offset_seed = phase % gap;
        for (int16_t offset = height + offset_seed; offset < width + height + gap; offset += gap)
        {
            const int16_t x0 = x + offset - height;
            const int16_t y0 = y;
            const int16_t x1 = x + offset;
            const int16_t y1 = y + height;
            display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
        }
    }

    void draw_scanline(uint8_t phase)
    {
        const int16_t sweep = (phase * 3) % (SCREEN_WIDTH - 32);
        display.drawFastHLine(6 + sweep, HEADER_HEIGHT - 1, 22, SSD1306_WHITE);
    }

    void draw_flow_underline(uint8_t phase)
    {
        const int16_t lane_y = HEADER_HEIGHT + 1;
        const int16_t left = 6;
        const int16_t span = SCREEN_WIDTH - 30 - FLOW_SEG_LEN; // leave space for right-side animation
        const int16_t offset = (phase * 2) % span;
        display.drawFastHLine(left + offset, lane_y, FLOW_SEG_LEN, SSD1306_WHITE);
    }

    void draw_pulse_right(uint8_t phase)
    {
        // Sliding chevron on the right side; keep away from text area.
        const int16_t x_start = SCREEN_WIDTH - 26;
        const int16_t y_start = HEADER_HEIGHT + 2;
        const uint8_t travel = 14;
        const uint8_t pos = (phase * 2) % travel;
        display.drawTriangle(x_start + pos, y_start, x_start + pos + 8, y_start + 4, x_start + pos, y_start + 8, SSD1306_WHITE);
    }

    void draw_mecha_shell(uint8_t phase)
    {
        // Double frame for a thicker, bolder box
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
        display.drawRect(FRAME_THICKNESS - 1, FRAME_THICKNESS - 1, SCREEN_WIDTH - 2 * (FRAME_THICKNESS - 1), SCREEN_HEIGHT - 2 * (FRAME_THICKNESS - 1), SSD1306_WHITE);
        display.drawFastHLine(0, HEADER_HEIGHT, SCREEN_WIDTH, SSD1306_WHITE);
        display.drawFastHLine(0, HEADER_HEIGHT + 1, SCREEN_WIDTH, SSD1306_WHITE);

        // Corner and spine accents
        display.drawFastHLine(3, 3, 18, SSD1306_WHITE);
        display.drawFastVLine(3, 3, 8, SSD1306_WHITE);
        display.drawFastHLine(SCREEN_WIDTH - 21, SCREEN_HEIGHT - 4, 18, SSD1306_WHITE);
        display.drawFastVLine(SCREEN_WIDTH - 4, SCREEN_HEIGHT - 12, 10, SSD1306_WHITE);
        display.drawFastVLine(6, HEADER_HEIGHT + 2, SCREEN_HEIGHT - HEADER_HEIGHT - 6, SSD1306_WHITE);

        // Mecha stripes stay clear of text
        draw_stripes(SCREEN_WIDTH - 40, 3, 32, HEADER_HEIGHT - 7, STRIPE_GAP, phase);
        draw_scanline(phase);
        draw_flow_underline(phase);
    }

    void draw_net_info(const wifi_runtime_config &cfg, const IPAddress &ip, uint8_t phase)
    {
        (void)phase;
        const String ssid = compact_ssid(cfg.ssid);
        const uint8_t ap_y = 4;                  // top cluster
        const uint8_t ip_y = HEADER_HEIGHT + 4;  // bottom cluster

        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        display.setCursor(10, ap_y);
        display.print("AP>");
        display.drawFastHLine(8, ap_y + 7, 16, SSD1306_WHITE);
        display.setCursor(32, ap_y);
        display.print(ssid);

        display.setCursor(10, ip_y);
        display.print("IP>");
        display.drawFastHLine(8, ip_y + 7, 16, SSD1306_WHITE);
        display.setCursor(32, ip_y);
        display.print(ip);
    }

    // 绘制圆角矩形（手动实现）
    void draw_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
    {
        // 绘制圆角边框
        display.drawFastHLine(x + r, y, w - 2 * r, color);           // 上边
        display.drawFastHLine(x + r, y + h - 1, w - 2 * r, color);   // 下边
        display.drawFastVLine(x, y + r, h - 2 * r, color);           // 左边
        display.drawFastVLine(x + w - 1, y + r, h - 2 * r, color);   // 右边
        
        // 绘制圆角（简化版，使用对角线）
        display.drawPixel(x + r - 1, y + 1, color);
        display.drawPixel(x + 1, y + r - 1, color);
        display.drawPixel(x + w - r, y + 1, color);
        display.drawPixel(x + w - 2, y + r - 1, color);
        display.drawPixel(x + r - 1, y + h - 2, color);
        display.drawPixel(x + 1, y + h - r, color);
        display.drawPixel(x + w - r, y + h - 2, color);
        display.drawPixel(x + w - 2, y + h - r, color);
    }

    // 填充圆角矩形
    void fill_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
    {
        // 填充主体
        display.fillRect(x + r, y, w - 2 * r, h, color);
        display.fillRect(x, y + r, r, h - 2 * r, color);
        display.fillRect(x + w - r, y + r, r, h - 2 * r, color);
        
        // 填充圆角区域
        for (int16_t i = 0; i < r; ++i)
        {
            display.drawFastHLine(x + r - i, y + i, w - 2 * (r - i), color);
            display.drawFastHLine(x + r - i, y + h - 1 - i, w - 2 * (r - i), color);
        }
    }

    // 高级缓动函数：Ease-in-out-quint（五次方，超级丝滑）
    float ease_in_out_quint(float t)
    {
        return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 5.0f) / 2.0f;
    }

    // Ease-out-expo（指数衰减，自然停止）
    float ease_out_expo(float t)
    {
        return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
    }

    // Ease-out-elastic（弹性效果）
    float ease_out_elastic(float t)
    {
        const float c4 = (2.0f * PI) / 3.0f;
        return t <= 0.0f ? 0.0f : (t >= 1.0f ? 1.0f : powf(2.0f, -10.0f * t) * sin((t * 10.0f - 0.75f) * c4) + 1.0f);
    }

    // Ease-in-out-cubic（三次方，更平滑）
    float ease_in_out_cubic(float t)
    {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    // Ease-out-back（回弹效果）
    float ease_out_back(float t)
    {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
    }

    // 绘制数字雨效果
    void draw_digital_rain(uint8_t frame)
    {
        // 6列数字雨
        for (int8_t col = 0; col < 6; ++col)
        {
            // 每列有不同的起始偏移和速度
            int16_t speed = 1 + (col % 3);
            int16_t offset = (col * 17) % SCREEN_HEIGHT;
            int16_t y_pos = ((frame * speed + offset) % (SCREEN_HEIGHT + 15)) - 15;
            
            // 绘制该列的数字字符
            for (int8_t i = 0; i < 4; ++i)
            {
                int16_t y = y_pos + i * 8;
                if (y >= 2 && y < SCREEN_HEIGHT - 2)
                {
                    int16_t x = 10 + col * 20;
                    if (x < SCREEN_WIDTH - 10)
                    {
                        // 渐变效果：顶部最亮
                        if (i == 0)
                        {
                            // 随机"数字"效果（用点和线模拟）
                            uint8_t pattern = (frame + col + y) % 16;
                            if (pattern < 8)
                            {
                                display.drawPixel(x, y, SSD1306_WHITE);
                                display.drawPixel(x + 1, y, SSD1306_WHITE);
                            }
                        }
                        else if ((frame + i + col) % 3 == 0)
                        {
                            display.drawPixel(x, y, SSD1306_WHITE);
                        }
                    }
                }
            }
        }
    }

    // 绘制扫描波效果
    void draw_scan_wave(uint8_t frame, int16_t center_y)
    {
        float wave_t = (frame % 40) / 40.0f;
        int16_t wave_radius = static_cast<int16_t>(wave_t * 50.0f);
        float intensity = 1.0f - wave_t;
        
        if (intensity > 0.1f)
        {
            // 中心横向扫描线
            for (int16_t x = 0; x < SCREEN_WIDTH; ++x)
            {
                int16_t dist = abs(x - SCREEN_WIDTH / 2);
                if (dist < wave_radius)
                {
                    if ((x + frame) % 3 == 0)
                    {
                        display.drawPixel(x, center_y, SSD1306_WHITE);
                    }
                }
            }
            
            // 外扩波纹
            if (wave_radius > 5 && wave_radius < 45)
            {
                for (int16_t x = max(0, SCREEN_WIDTH/2 - wave_radius); 
                     x < min(SCREEN_WIDTH, SCREEN_WIDTH/2 + wave_radius); ++x)
                {
                    if ((x + frame) % 5 == 0)
                    {
                        int16_t y_offset = static_cast<int16_t>(3.0f * intensity);
                        if (center_y + y_offset < SCREEN_HEIGHT)
                            display.drawPixel(x, center_y + y_offset, SSD1306_WHITE);
                        if (center_y - y_offset >= 0)
                            display.drawPixel(x, center_y - y_offset, SSD1306_WHITE);
                    }
                }
            }
        }
    }

    // 绘制HUD风格的角落UI
    void draw_hud_corners(uint8_t frame, float progress)
    {
        uint8_t len = static_cast<uint8_t>(12 * progress);
        if (len < 3) return;
        
        // 左上角
        display.drawFastHLine(3, 3, len, SSD1306_WHITE);
        display.drawFastVLine(3, 3, len, SSD1306_WHITE);
        display.drawFastHLine(3, 4, len - 2, SSD1306_WHITE);
        display.drawFastVLine(4, 3, len - 2, SSD1306_WHITE);
        // 动态扫描点
        if ((frame / 3) % 2 == 0 && len > 6)
        {
            display.drawPixel(3 + len - 2, 5, SSD1306_WHITE);
            display.drawPixel(5, 3 + len - 2, SSD1306_WHITE);
        }
        
        // 右上角
        display.drawFastHLine(SCREEN_WIDTH - 3 - len, 3, len, SSD1306_WHITE);
        display.drawFastVLine(SCREEN_WIDTH - 4, 3, len, SSD1306_WHITE);
        display.drawFastHLine(SCREEN_WIDTH - 3 - len + 2, 4, len - 2, SSD1306_WHITE);
        display.drawFastVLine(SCREEN_WIDTH - 5, 3, len - 2, SSD1306_WHITE);
        
        // 左下角
        display.drawFastHLine(3, SCREEN_HEIGHT - 4, len, SSD1306_WHITE);
        display.drawFastVLine(3, SCREEN_HEIGHT - 3 - len, len, SSD1306_WHITE);
        display.drawFastHLine(3, SCREEN_HEIGHT - 5, len - 2, SSD1306_WHITE);
        display.drawFastVLine(4, SCREEN_HEIGHT - 3 - len + 2, len - 2, SSD1306_WHITE);
        
        // 右下角
        display.drawFastHLine(SCREEN_WIDTH - 3 - len, SCREEN_HEIGHT - 4, len, SSD1306_WHITE);
        display.drawFastVLine(SCREEN_WIDTH - 4, SCREEN_HEIGHT - 3 - len, len, SSD1306_WHITE);
        display.drawFastHLine(SCREEN_WIDTH - 3 - len + 2, SCREEN_HEIGHT - 5, len - 2, SSD1306_WHITE);
        display.drawFastVLine(SCREEN_WIDTH - 5, SCREEN_HEIGHT - 3 - len + 2, len - 2, SSD1306_WHITE);
    }

    // 绘制电路板风格装饰线
    void draw_circuit_lines(uint8_t frame, float progress)
    {
        if (progress < 0.2f) return;
        
        float line_progress = (progress - 0.2f) / 0.8f;
        
        // 顶部电路线（从左向右生长）
        int16_t top_len = static_cast<int16_t>((SCREEN_WIDTH - 20) * line_progress);
        for (int16_t x = 10; x < 10 + top_len; x += 3)
        {
            display.drawPixel(x, 2, SSD1306_WHITE);
            if ((x / 3) % 5 == 0)  // 节点
            {
                display.drawPixel(x, 1, SSD1306_WHITE);
                display.drawPixel(x, 3, SSD1306_WHITE);
            }
        }
        
        // 底部电路线（从右向左生长）
        int16_t bottom_start = SCREEN_WIDTH - 10;
        int16_t bottom_len = static_cast<int16_t>((SCREEN_WIDTH - 20) * line_progress);
        for (int16_t x = bottom_start; x > bottom_start - bottom_len; x -= 3)
        {
            display.drawPixel(x, SCREEN_HEIGHT - 3, SSD1306_WHITE);
            if ((x / 3) % 5 == 0)  // 节点
            {
                display.drawPixel(x, SCREEN_HEIGHT - 2, SSD1306_WHITE);
                display.drawPixel(x, SCREEN_HEIGHT - 4, SSD1306_WHITE);
            }
        }
        
        // 侧边电路线
        if (line_progress > 0.5f)
        {
            int16_t side_len = static_cast<int16_t>((SCREEN_HEIGHT - 12) * (line_progress - 0.5f) / 0.5f);
            for (int16_t y = 6; y < 6 + side_len; y += 3)
            {
                if ((y / 3) % 3 == 0)
                {
                    display.drawPixel(2, y, SSD1306_WHITE);
                    display.drawPixel(SCREEN_WIDTH - 3, y, SSD1306_WHITE);
                }
            }
        }
    }

    // 高级粒子系统
    struct Particle
    {
        float x, y;
        float vx, vy;
        uint8_t life;
    };

    void draw_advanced_particles(uint8_t frame)
    {
        // 8个粒子
        for (int8_t i = 0; i < 8; ++i)
        {
            // 使用伪随机但可重现的轨迹
            float t = (frame + i * 11) / 100.0f;
            float x = (SCREEN_WIDTH / 2) + 30.0f * sin(t * 2.0f + i);
            float y = (SCREEN_HEIGHT / 2) + 10.0f * cos(t * 3.0f + i * 0.7f);
            
            // 生命周期闪烁
            uint8_t life_phase = (frame + i * 13) % 30;
            if (life_phase < 20)
            {
                int16_t px = static_cast<int16_t>(x);
                int16_t py = static_cast<int16_t>(y);
                
                if (px >= 2 && px < SCREEN_WIDTH - 2 && py >= 2 && py < SCREEN_HEIGHT - 2)
                {
                    display.drawPixel(px, py, SSD1306_WHITE);
                    
                    // 粒子轨迹
                    if (life_phase < 15 && (frame + i) % 2 == 0)
                    {
                        display.drawPixel(px - 1, py, SSD1306_WHITE);
                    }
                    
                    // 粒子连接线（科技感）
                    if (i > 0 && life_phase < 10)
                    {
                        float prev_t = (frame + (i - 1) * 11) / 100.0f;
                        int16_t prev_x = static_cast<int16_t>((SCREEN_WIDTH / 2) + 30.0f * sin(prev_t * 2.0f + (i - 1)));
                        int16_t prev_y = static_cast<int16_t>((SCREEN_HEIGHT / 2) + 10.0f * cos(prev_t * 3.0f + (i - 1) * 0.7f));
                        
                        float dist = sqrt((px - prev_x) * (px - prev_x) + (py - prev_y) * (py - prev_y));
                        if (dist < 15.0f && (frame % 3) == 0)
                        {
                            display.drawLine(px, py, prev_x, prev_y, SSD1306_WHITE);
                        }
                    }
                }
            }
        }
    }

    // 电影级启动动画：极致细腻，多层次光效，科技风格
    void play_boot_animation()
    {
        constexpr uint8_t FRAME_COUNT = 100;     // 总帧数（约5秒）
        constexpr uint8_t FRAME_DELAY = 50;      // 每帧延迟
        constexpr uint8_t CARD_PADDING = 4;      // 卡片内边距
        
        // Logo 和进度条垂直居中对齐计算
        constexpr uint8_t CENTER_Y = SCREEN_HEIGHT / 2;
        constexpr uint8_t LOGO_X = CARD_PADDING + 3;
        constexpr uint8_t LOGO_Y = CENTER_Y - COMPANY_LOGO_HEIGHT / 2;
        
        constexpr uint8_t BAR_X = LOGO_X + COMPANY_LOGO_WIDTH + 10;
        constexpr uint8_t BAR_HEIGHT = 8;
        constexpr uint8_t BAR_Y = CENTER_Y - BAR_HEIGHT / 2;
        constexpr uint8_t BAR_WIDTH = SCREEN_WIDTH - BAR_X - CARD_PADDING - 3;
        constexpr uint8_t BAR_RADIUS = BAR_HEIGHT / 2;  // 圆角半径

        for (uint8_t frame = 0; frame < FRAME_COUNT; ++frame)
        {
            display.clearDisplay();

            // === 第0层：数字雨背景（科技感氛围） ===
            if (frame >= 2 && frame < 20)
            {
                // 早期阶段：数字雨效果
                draw_digital_rain(frame - 2);
            }

            // === 第1层：扫描波效果 ===
            if (frame >= 8 && frame < 50)
            {
                draw_scan_wave(frame - 8, SCREEN_HEIGHT / 2);
            }

            // === 第2层：电路板装饰线 ===
            if (frame >= 5 && frame < 90)
            {
                float circuit_progress = constrain((frame - 5.0f) / 30.0f, 0.0f, 1.0f);
                draw_circuit_lines(frame, circuit_progress);
            }

            // === 第3层：背景装饰动画（营造氛围） ===
            if (frame >= 5 && frame < 85)
            {
                // 流动的背景线条（斜向扫描）
                for (int8_t i = 0; i < 3; ++i)
                {
                    int16_t line_pos = ((frame * 3 + i * 20) % (SCREEN_WIDTH + SCREEN_HEIGHT)) - SCREEN_HEIGHT;
                    if ((frame + i) % 8 < 2)  // 间歇出现
                    {
                        for (int16_t y = 0; y < SCREEN_HEIGHT; ++y)
                        {
                            int16_t x = line_pos + y;
                            if (x > 2 && x < SCREEN_WIDTH - 2 && y > 2 && y < SCREEN_HEIGHT - 2)
                            {
                                if ((x + y) % 6 == 0)  // 点状线条
                                {
                                    display.drawPixel(x, y, SSD1306_WHITE);
                                }
                            }
                        }
                    }
                }
            }

            // === 第4层：高级粒子系统 ===
            if (frame >= 15 && frame < 85)
            {
                draw_advanced_particles(frame - 15);
            }

            // === 第5层：HUD风格边框和角落UI ===
            if (frame >= 3)
            {
                if (frame < 12)
                {
                    // HUD角落生长动画
                    float hud_progress = ease_out_back((frame - 3.0f) / 9.0f);
                    draw_hud_corners(frame, hud_progress);
                }
                else
                {
                    // 完整HUD角落
                    draw_hud_corners(frame, 1.0f);
                    
                    // 完整边框 + 脉冲效果
                    display.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, SSD1306_WHITE);
                    
                    // 边框扫描线效果
                    if (frame >= 15 && frame < 80)
                    {
                        int16_t scan_pos = ((frame - 15) * 4) % (2 * (SCREEN_WIDTH + SCREEN_HEIGHT));
                        
                        if (scan_pos < SCREEN_WIDTH)
                        {
                            // 顶边
                            display.drawPixel(scan_pos, 0, SSD1306_WHITE);
                            if (scan_pos > 0) display.drawPixel(scan_pos - 1, 0, SSD1306_WHITE);
                        }
                        else if (scan_pos < SCREEN_WIDTH + SCREEN_HEIGHT)
                        {
                            // 右边
                            display.drawPixel(SCREEN_WIDTH - 1, scan_pos - SCREEN_WIDTH, SSD1306_WHITE);
                        }
                        else if (scan_pos < 2 * SCREEN_WIDTH + SCREEN_HEIGHT)
                        {
                            // 底边
                            display.drawPixel(SCREEN_WIDTH - (scan_pos - SCREEN_WIDTH - SCREEN_HEIGHT), SCREEN_HEIGHT - 1, SSD1306_WHITE);
                        }
                        else
                        {
                            // 左边
                            display.drawPixel(0, SCREEN_HEIGHT - (scan_pos - 2 * SCREEN_WIDTH - SCREEN_HEIGHT), SSD1306_WHITE);
                        }
                    }
                    
                    // 角落高光脉冲
                    if (frame >= 15 && frame < 80)
                    {
                        float pulse = sin((frame - 15) * 0.25f) * 0.5f + 0.5f;
                        if (pulse > 0.65f)
                        {
                            // 角落光晕
                            display.drawPixel(2, 2, SSD1306_WHITE);
                            display.drawPixel(SCREEN_WIDTH - 3, 2, SSD1306_WHITE);
                            display.drawPixel(2, SCREEN_HEIGHT - 3, SSD1306_WHITE);
                            display.drawPixel(SCREEN_WIDTH - 3, SCREEN_HEIGHT - 3, SSD1306_WHITE);
                            
                            if (pulse > 0.8f)
                            {
                                display.drawPixel(1, 2, SSD1306_WHITE);
                                display.drawPixel(2, 1, SSD1306_WHITE);
                                display.drawPixel(SCREEN_WIDTH - 2, 2, SSD1306_WHITE);
                                display.drawPixel(SCREEN_WIDTH - 3, 1, SSD1306_WHITE);
                            }
                        }
                    }
                }
            }

            // === 第6层：Logo 超级华丽淡入 + 多重光芒效果 ===
            if (frame >= 12)
            {
                // 计算显示进度（使用超级平滑的缓动）
                float logo_progress = (frame - 12.0f) / 18.0f;
                logo_progress = constrain(logo_progress, 0.0f, 1.0f);
                float eased = ease_in_out_cubic(logo_progress);
                uint8_t visible_rows = static_cast<uint8_t>(COMPANY_LOGO_HEIGHT * eased);
                
                // 绘制 Logo
                for (uint8_t row = 0; row < visible_rows; ++row)
                {
                    for (uint8_t col = 0; col < COMPANY_LOGO_WIDTH; ++col)
                    {
                        uint8_t byte_index = row * 4 + col / 8;
                        uint8_t bit_index = 7 - (col % 8);
                        
                        if (pgm_read_byte(&company_logo_bitmap[byte_index]) & (1 << bit_index))
                        {
                            display.drawPixel(LOGO_X + col, LOGO_Y + row, SSD1306_WHITE);
                        }
                    }
                }
                
                // Logo 多层光芒效果（完整显示后）
                if (visible_rows >= COMPANY_LOGO_HEIGHT && frame < 45)
                {
                    // 外发光效（四周辉光）
                    float glow_intensity = sin((frame - 30) * 0.4f) * 0.5f + 0.5f;
                    
                    // 第1层：主光芒
                    if (glow_intensity > 0.3f)
                    {
                        // 顶部光线阵列
                        for (int8_t i = -3; i <= 3; ++i)
                        {
                            int16_t x = LOGO_X + COMPANY_LOGO_WIDTH / 2 + i * 3;
                            if (x > 0 && x < SCREEN_WIDTH)
                            {
                                if ((frame + i) % 3 == 0)
                            {
                                display.drawPixel(x, LOGO_Y - 2, SSD1306_WHITE);
                                    if (glow_intensity > 0.6f)
                                        display.drawPixel(x, LOGO_Y - 3, SSD1306_WHITE);
                            }
                        }
                        }
                        // 底部光线阵列
                        for (int8_t i = -3; i <= 3; ++i)
                        {
                            int16_t x = LOGO_X + COMPANY_LOGO_WIDTH / 2 + i * 3;
                            if (x > 0 && x < SCREEN_WIDTH)
                            {
                                if ((frame - i) % 3 == 0)
                            {
                                display.drawPixel(x, LOGO_Y + COMPANY_LOGO_HEIGHT + 1, SSD1306_WHITE);
                                    if (glow_intensity > 0.6f)
                                        display.drawPixel(x, LOGO_Y + COMPANY_LOGO_HEIGHT + 2, SSD1306_WHITE);
                                }
                            }
                        }
                        
                        // 左右侧光芒
                        for (int8_t i = -2; i <= 2; ++i)
                        {
                            int16_t y = LOGO_Y + COMPANY_LOGO_HEIGHT / 2 + i * 3;
                            if (y > 0 && y < SCREEN_HEIGHT)
                            {
                                if ((frame + i) % 2 == 0)
                                {
                                    display.drawPixel(LOGO_X - 2, y, SSD1306_WHITE);
                                    display.drawPixel(LOGO_X + COMPANY_LOGO_WIDTH + 1, y, SSD1306_WHITE);
                                }
                            }
                        }
                    }
                    
                    // 第2层：脉冲光环
                    if (glow_intensity > 0.7f)
                    {
                        // 四角光点
                        display.drawPixel(LOGO_X - 1, LOGO_Y - 1, SSD1306_WHITE);
                        display.drawPixel(LOGO_X + COMPANY_LOGO_WIDTH, LOGO_Y - 1, SSD1306_WHITE);
                        display.drawPixel(LOGO_X - 1, LOGO_Y + COMPANY_LOGO_HEIGHT, SSD1306_WHITE);
                        display.drawPixel(LOGO_X + COMPANY_LOGO_WIDTH, LOGO_Y + COMPANY_LOGO_HEIGHT, SSD1306_WHITE);
                    }
                }
                
                // 超级动态扫描线（多层科技感）
                if (visible_rows < COMPANY_LOGO_HEIGHT)
                {
                    // 主扫描线（最亮）
                    display.drawFastHLine(LOGO_X - 3, LOGO_Y + visible_rows, COMPANY_LOGO_WIDTH + 6, SSD1306_WHITE);
                    
                    // 第二层扫描线
                    if (visible_rows > 0)
                    {
                        display.drawFastHLine(LOGO_X - 2, LOGO_Y + visible_rows - 1, COMPANY_LOGO_WIDTH + 4, SSD1306_WHITE);
                    }
                    
                    // 尾随扫描线（渐弱效果）
                    if (visible_rows > 2)
                    {
                        for (uint8_t trail = 2; trail <= 4 && visible_rows > trail; ++trail)
                        {
                            if ((frame + trail) % (trail + 1) == 0)
                            {
                                int16_t trail_width = COMPANY_LOGO_WIDTH - (trail * 2);
                                if (trail_width > 0)
                                {
                                    display.drawFastHLine(LOGO_X + trail, LOGO_Y + visible_rows - trail, 
                                                        trail_width, SSD1306_WHITE);
                                }
                            }
                        }
                    }
                    
                    // 扫描线前方的光点预示
                    if ((frame % 3) < 2 && visible_rows < COMPANY_LOGO_HEIGHT - 1)
                    {
                        for (int8_t i = -2; i <= 2; i += 2)
                        {
                            int16_t x = LOGO_X + COMPANY_LOGO_WIDTH / 2 + i * 4;
                            if (x > LOGO_X && x < LOGO_X + COMPANY_LOGO_WIDTH)
                            {
                                display.drawPixel(x, LOGO_Y + visible_rows + 1, SSD1306_WHITE);
                            }
                        }
                    }
                }
            }

            // === 第7层：超级胶囊进度条 - 顶级多重特效叠加 ===
            if (frame >= 32)
            {
                // 外框优雅出现（超级弹性动画）
                if (frame < 40)
                {
                    float expand_t = (frame - 32.0f) / 8.0f;
                    expand_t = constrain(expand_t, 0.0f, 1.0f);
                    float eased_expand = ease_out_elastic(expand_t);
                    
                    uint8_t current_width = static_cast<uint8_t>(BAR_WIDTH * eased_expand);
                    current_width = constrain(current_width, 0, BAR_WIDTH);
                    uint8_t offset_x = BAR_X + (BAR_WIDTH - current_width) / 2;
                    
                    if (current_width > 10)
                    {
                        draw_rounded_rect(offset_x, BAR_Y, current_width, BAR_HEIGHT, BAR_RADIUS, SSD1306_WHITE);
                        
                        // 出现时的超级粒子爆发效果
                        for (int8_t i = -2; i <= 2; ++i)
                        {
                            int16_t px = offset_x + current_width / 2 + i * 6;
                                if (px > 2 && px < SCREEN_WIDTH - 2)
                            {
                                if ((frame + i) % 3 == 0)
                                {
                                    display.drawPixel(px, BAR_Y - 2, SSD1306_WHITE);
                                    display.drawPixel(px, BAR_Y + BAR_HEIGHT + 1, SSD1306_WHITE);
                                    
                                    // 额外粒子层
                                    if ((frame + i) % 6 == 0)
                                    {
                                        display.drawPixel(px - 1, BAR_Y - 3, SSD1306_WHITE);
                                        display.drawPixel(px + 1, BAR_Y + BAR_HEIGHT + 2, SSD1306_WHITE);
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    // 完整胶囊外框 + 四层边框（超强立体感）
                    draw_rounded_rect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, BAR_RADIUS, SSD1306_WHITE);
                    draw_rounded_rect(BAR_X + 1, BAR_Y + 1, BAR_WIDTH - 2, BAR_HEIGHT - 2, BAR_RADIUS - 1, SSD1306_WHITE);
                    
                    // 第三层微光边框（动态脉冲效果）
                    if (frame >= 45)
                    {
                        float pulse = sin(frame * 0.3f) * 0.5f + 0.5f;
                        if (pulse > 0.5f)
                    {
                        // 顶部高光线
                        display.drawFastHLine(BAR_X + BAR_RADIUS, BAR_Y + 2, BAR_WIDTH - BAR_RADIUS * 2, SSD1306_WHITE);
                            
                            if (pulse > 0.75f)
                            {
                                // 底部反光
                                for (int16_t x = BAR_X + BAR_RADIUS; x < BAR_X + BAR_WIDTH - BAR_RADIUS; x += 2)
                                {
                                    if ((x + frame) % 4 == 0)
                                        display.drawPixel(x, BAR_Y + BAR_HEIGHT - 3, SSD1306_WHITE);
                                }
                            }
                        }
                    }
                    
                    // 外围光晕效果
                    if (frame >= 50 && (frame / 5) % 2 == 0)
                    {
                        for (int8_t i = -1; i <= 1; ++i)
                        {
                            if ((BAR_X + i * 15) > 5 && (BAR_X + i * 15) < SCREEN_WIDTH - 5)
                            {
                                display.drawPixel(BAR_X + BAR_WIDTH / 2 + i * 20, BAR_Y - 3, SSD1306_WHITE);
                            }
                        }
                    }
                }
                
                // === 超顶级进度填充动画（极致丝滑） ===
                if (frame >= 42)
                {
                    // 计算进度（使用最丝滑的缓动）
                    float t = (frame - 42.0f) / 48.0f;
                    t = constrain(t, 0.0f, 1.0f);
                    float eased = ease_in_out_quint(t);
                    
                    uint8_t inner_width = BAR_WIDTH - 6;
                    uint8_t progress_width = static_cast<uint8_t>(inner_width * eased);
                    
                    if (progress_width > 0)
                    {
                        int16_t fill_x = BAR_X + 3;
                        int16_t fill_y = BAR_Y + 3;
                        int16_t fill_h = BAR_HEIGHT - 6;
                        
                        // === 第一层：基础填充 ===
                        if (progress_width <= BAR_RADIUS - 1)
                        {
                            for (uint8_t x = 0; x < progress_width; ++x)
                            {
                                display.drawFastVLine(fill_x + x, fill_y, fill_h, SSD1306_WHITE);
                            }
                        }
                        else
                        {
                            fill_rounded_rect(fill_x, fill_y, progress_width, fill_h, BAR_RADIUS - 2, SSD1306_WHITE);
                        }
                        
                        // === 第二层：超级多重流光效果（三层叠加） ===
                        if (progress_width > 8)
                        {
                            // 流光 A（快速，主流）
                            uint8_t wave_a = (frame * 4) % progress_width;
                            for (uint8_t w = 0; w < 4 && wave_a + w < progress_width; ++w)
                            {
                                display.drawPixel(fill_x + wave_a + w, fill_y + 1, SSD1306_BLACK);
                                if (w < 2)
                                    display.drawPixel(fill_x + wave_a + w, fill_y, SSD1306_BLACK);
                            }
                            
                            // 流光 B（中速，交错）
                            if (progress_width > 12)
                            {
                                uint8_t wave_b = ((frame * 2 + 5) % progress_width);
                                for (uint8_t y = 0; y < fill_h; y += 2)
                                {
                                    if (wave_b < progress_width && wave_b + 1 < progress_width)
                                    {
                                        display.drawPixel(fill_x + wave_b, fill_y + y, SSD1306_BLACK);
                                        if ((y % 4) == 0)
                                            display.drawPixel(fill_x + wave_b + 1, fill_y + y, SSD1306_BLACK);
                                    }
                                }
                            }
                            
                            // 流光 C（慢速，反向，最宽）
                            if (progress_width > 16)
                            {
                                uint8_t wave_c = progress_width - ((frame * 2) % progress_width);
                                for (uint8_t w = 0; w < 2 && wave_c + w < progress_width; ++w)
                                {
                                    for (uint8_t y = 0; y < fill_h; y += 2)
                                    {
                                        display.drawPixel(fill_x + wave_c + w, fill_y + y, SSD1306_BLACK);
                                    }
                                }
                            }
                            
                            // 主扫描线效果（垂直光束，更宽）
                            uint8_t scan_pos = (frame * 3) % progress_width;
                            if (scan_pos < progress_width - 2)
                            {
                                display.drawFastVLine(fill_x + scan_pos, fill_y, fill_h, SSD1306_BLACK);
                                if ((frame % 2) == 0)
                                    display.drawFastVLine(fill_x + scan_pos + 1, fill_y, fill_h, SSD1306_BLACK);
                            }
                        }
                        
                        // === 第三层：七层渐进超级光晕（更细腻） ===
                        if (progress_width > 4 && progress_width < inner_width - 2)
                        {
                            int16_t glow_x = fill_x + progress_width;
                            
                            // 主光晕（7层渐变）
                            for (int8_t layer = 0; layer < 7; ++layer)
                            {
                                int16_t px = glow_x + layer;
                                if (px < BAR_X + BAR_WIDTH - 3)
                                {
                                    uint8_t density = (layer / 2) + 1;
                                    float layer_intensity = 1.0f - (layer / 7.0f);
                                    
                                    for (uint8_t y = 0; y < fill_h; ++y)
                                    {
                                        // 更精细的动态密度控制
                                        uint8_t pattern = (y * density + frame + layer) % (density * 3);
                                        bool show = false;
                                        
                                        if (layer < 3)
                                        {
                                            // 前3层：高密度
                                            show = (pattern < density * 2);
                                        }
                                        else if (layer < 5)
                                        {
                                            // 中间层：中密度
                                            show = (pattern < density) || ((frame % (layer + 1)) == 0);
                                        }
                                        else
                                        {
                                            // 外层：低密度闪烁
                                            show = (pattern == 0) && ((frame % (layer + 2)) < 2);
                                        }
                                        
                                        if (show)
                                        {
                                            display.drawPixel(px, fill_y + y, SSD1306_WHITE);
                                        }
                                    }
                                }
                            }
                            
                            // 多重脉冲光晕（三层呼吸效果）
                            float pulse1 = sin(frame * 0.4f) * 0.5f + 0.5f;
                            float pulse2 = sin(frame * 0.25f + 1.0f) * 0.5f + 0.5f;
                            
                            if (pulse1 > 0.6f)
                            {
                                // 第一层脉冲
                                for (int8_t extra = 7; extra < 9; ++extra)
                                {
                                    int16_t px = glow_x + extra;
                                    if (px < BAR_X + BAR_WIDTH - 3)
                                    {
                                        if ((frame + extra) % 2 == 0)
                                        {
                                            display.drawPixel(px, fill_y + fill_h / 2, SSD1306_WHITE);
                                            if (pulse1 > 0.8f)
                                            {
                                                display.drawPixel(px, fill_y + 1, SSD1306_WHITE);
                                                display.drawPixel(px, fill_y + fill_h - 2, SSD1306_WHITE);
                                            }
                                        }
                                    }
                                }
                            }
                            
                            if (pulse2 > 0.7f)
                            {
                                // 第二层脉冲（更外层）
                                for (int8_t extra = 9; extra < 11; ++extra)
                                {
                                    int16_t px = glow_x + extra;
                                    if (px < BAR_X + BAR_WIDTH - 3 && (frame + extra * 2) % 4 < 2)
                                    {
                                        display.drawPixel(px, fill_y + fill_h / 2, SSD1306_WHITE);
                                    }
                                }
                            }
                        }
                        
                        // === 第四层：增强粒子尾迹效果（6个粒子） ===
                        if (progress_width > 12)
                        {
                            for (int8_t p = 0; p < 6; ++p)
                            {
                                uint8_t particle_offset = (frame + p * 3) % 6;
                                uint8_t particle_x = (progress_width - 8 - p * 2) + particle_offset;
                                
                                if (particle_x < progress_width && particle_x > 2)
                                {
                                    int8_t particle_y = fill_y + (p % 3 == 0 ? 0 : (p % 3 == 1 ? fill_h / 2 : fill_h - 1));
                                    display.drawPixel(fill_x + particle_x, particle_y, SSD1306_BLACK);
                                    
                                    // 粒子尾迹
                                    if (particle_offset < 3 && particle_x > 3)
                                    {
                                        display.drawPixel(fill_x + particle_x - 1, particle_y, SSD1306_BLACK);
                                    }
                                }
                            }
                        }
                    }
                }

                // 百分比数字 + 高级微动效
                if (frame >= 48)
                {
                    uint8_t percent = map(frame, 42, 90, 0, 100);
                    percent = constrain(percent, 0, 100);
                    
                    display.setTextSize(1);
                    char percent_str[5];
                    snprintf(percent_str, sizeof(percent_str), "%d%%", percent);
                    uint8_t text_width = strlen(percent_str) * 6;
                    
                    // 增强抖动效果（更生动的数字跳动）
                    int8_t shake_y = 0;
                    int8_t shake_x = 0;
                    if (percent < 100)
                    {
                        if ((frame % 15) < 2)
                        {
                            shake_y = (frame % 2) == 0 ? -1 : 1;
                        }
                        if ((frame % 23) < 1)
                        {
                            shake_x = ((frame / 23) % 2) == 0 ? -1 : 1;
                        }
                    }
                    
                    display.setCursor(BAR_X + (BAR_WIDTH - text_width) / 2 + shake_x, BAR_Y + BAR_HEIGHT + 2 + shake_y);
                    display.print(percent_str);
                    
                    // 百分比周围的动态装饰
                    uint8_t dot_x = BAR_X + (BAR_WIDTH - text_width) / 2 - 3;
                    
                    // 左右装饰点（闪烁）
                    if ((frame / 3) % 3 < 2)
                    {
                        display.drawPixel(dot_x, BAR_Y + BAR_HEIGHT + 4, SSD1306_WHITE);
                        display.drawPixel(dot_x + text_width + 6, BAR_Y + BAR_HEIGHT + 4, SSD1306_WHITE);
                    }
                    
                    // 额外装饰线（进度感）
                    if (percent > 30 && (frame / 5) % 2 == 0)
                    {
                        display.drawFastHLine(dot_x - 2, BAR_Y + BAR_HEIGHT + 5, 3, SSD1306_WHITE);
                        display.drawFastHLine(dot_x + text_width + 8, BAR_Y + BAR_HEIGHT + 5, 3, SSD1306_WHITE);
                    }
                }
            }

            // === 第8层：品牌装饰元素 + 增强动态粒子 ===
            if (frame >= 12)
            {
                // === 角落标识（超级动态生长 + 呼吸效果） ===
                uint8_t corner_phase = min((uint8_t)(frame - 12), (uint8_t)8);
                float breath_intensity = sin(frame * 0.2f) * 0.3f + 0.7f;
                
                // 左上角（科技感L形 + 多层扫描线）
                for (uint8_t i = 0; i <= corner_phase; ++i)
                {
                    display.drawPixel(3 + i, 3, SSD1306_WHITE);
                    display.drawPixel(3, 3 + i, SSD1306_WHITE);
                    
                    // 双层线条
                    if (i > 0 && corner_phase >= 4)
                    {
                        display.drawPixel(3 + i, 4, SSD1306_WHITE);
                        display.drawPixel(4, 3 + i, SSD1306_WHITE);
                    }
                }
                
                // 动态扫描点
                if (corner_phase >= 4)
                {
                    if ((frame / 6) % 3 < 2)
                    {
                        display.drawPixel(3 + corner_phase, 3, SSD1306_WHITE);
                        display.drawPixel(3, 3 + corner_phase, SSD1306_WHITE);
                    }
                    if (breath_intensity > 0.8f)
                    {
                        display.drawPixel(3 + corner_phase + 1, 4, SSD1306_WHITE);
                        display.drawPixel(4, 3 + corner_phase + 1, SSD1306_WHITE);
                    }
                }
                
                // 右下角（镜像，更复杂）
                for (uint8_t i = 0; i <= corner_phase; ++i)
                {
                    display.drawPixel(SCREEN_WIDTH - 4 - i, SCREEN_HEIGHT - 4, SSD1306_WHITE);
                    display.drawPixel(SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4 - i, SSD1306_WHITE);
                    
                    if (i > 0 && corner_phase >= 4)
                    {
                        display.drawPixel(SCREEN_WIDTH - 4 - i, SCREEN_HEIGHT - 5, SSD1306_WHITE);
                        display.drawPixel(SCREEN_WIDTH - 5, SCREEN_HEIGHT - 4 - i, SSD1306_WHITE);
                    }
                }
                
                if (corner_phase >= 4 && (frame / 6) % 3 < 2)
                {
                    display.drawPixel(SCREEN_WIDTH - 4 - corner_phase, SCREEN_HEIGHT - 4, SSD1306_WHITE);
                    display.drawPixel(SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4 - corner_phase, SSD1306_WHITE);
                }
                
                // === 增强漂浮粒子系统（更多粒子） ===
                if (frame >= 25 && frame < 88)
                {
                    // 增强粒子参数（9个粒子，更复杂运动）
                    for (int8_t i = 0; i < 9; ++i)
                    {
                        // 使用frame和粒子索引计算位置（更复杂的运动模式）
                        float t = (frame + i * 13) / 40.0f;
                        int16_t px = static_cast<int16_t>(((frame * (i + 2)) % (SCREEN_WIDTH - 8)) + 4 + 3.0f * sin(t));
                        int16_t py = static_cast<int16_t>(4 + ((i * 5 + frame / 3) % (SCREEN_HEIGHT - 8)) + 2.0f * cos(t * 1.5f));
                        
                        // 确保在边界内
                        px = constrain(px, 3, SCREEN_WIDTH - 4);
                        py = constrain(py, 3, SCREEN_HEIGHT - 4);
                        
                        // 粒子生命周期闪烁（更复杂的模式）
                        uint8_t life_cycle = (frame + i * 7) % 24;
                        if (life_cycle < 16)
                        {
                            display.drawPixel(px, py, SSD1306_WHITE);
                            
                            // 增强粒子尾迹
                            if (life_cycle < 12)
                            {
                                if ((frame + i) % 3 == 0)
                                {
                                    display.drawPixel(px - 1, py, SSD1306_WHITE);
                                }
                                if ((frame + i) % 5 == 0 && life_cycle < 8)
                                {
                                    display.drawPixel(px, py - 1, SSD1306_WHITE);
                                }
                            }
                            
                            // 粒子光晕（最亮时）
                            if (life_cycle < 6 && (frame % 4) < 2)
                            {
                                display.drawPixel(px + 1, py, SSD1306_WHITE);
                                display.drawPixel(px, py + 1, SSD1306_WHITE);
                            }
                        }
                    }
                }
            }

            // === 第9层：状态文字（底部中央，超级呼吸式显示） ===
            if (frame >= 45 && frame < 80)
            {
                // 超级呼吸效果：使用正弦波控制显示，极度平滑
                float breath = sin((frame - 45) * 0.3f);
                if (breath > -0.5f)
                {
                    const char* status_text = "INITIALIZING";
                    uint8_t text_len = strlen(status_text);
                    display.setTextSize(1);
                    
                    // 动画点（1-3个点循环）
                    uint8_t dot_count = ((frame - 45) / 8) % 4;
                    char full_text[16];
                    strcpy(full_text, status_text);
                    for (uint8_t i = 0; i < dot_count; ++i)
                    {
                        strcat(full_text, ".");
                    }
                    
                    uint8_t full_width = strlen(full_text) * 6;
                    display.setCursor((SCREEN_WIDTH - full_width) / 2, SCREEN_HEIGHT - 9);
                    display.print(full_text);
                    
                    // 文字下方装饰线（扫描效果）
                    if (breath > 0.3f)
                    {
                        int16_t line_len = static_cast<int16_t>(full_width * (breath + 0.5f) / 1.5f);
                        int16_t line_x = (SCREEN_WIDTH - line_len) / 2;
                        display.drawFastHLine(line_x, SCREEN_HEIGHT - 7, line_len, SSD1306_WHITE);
                        
                        // 端点闪烁
                        if ((frame / 4) % 2 == 0)
                        {
                            display.drawPixel(line_x, SCREEN_HEIGHT - 8, SSD1306_WHITE);
                            display.drawPixel(line_x + line_len - 1, SCREEN_HEIGHT - 8, SSD1306_WHITE);
                        }
                    }
                }
            }

            // === 第10层：完成提示（华丽展开动画） ===
            if (frame >= 82)
            {
                display.setTextSize(1);
                
                // 逐字符展开动画（更慢，更优雅）
                uint8_t reveal_chars = min((uint8_t)((frame - 82) / 2 + 1), (uint8_t)5);
                const char* ready_text = "READY";
                char revealed[6] = {0};
                strncpy(revealed, ready_text, reveal_chars);
                
                uint8_t full_text_width = 5 * 6;  // 完整文字宽度
                int16_t center_x = (SCREEN_WIDTH - full_text_width) / 2;
                display.setCursor(center_x, SCREEN_HEIGHT - 9);
                display.print(revealed);
                
                // 字符出现时的闪光效果
                if (reveal_chars < 5 && (frame % 4) < 2)
                {
                    int16_t flash_x = center_x + reveal_chars * 6;
                    for (int8_t i = -1; i <= 1; ++i)
                    {
                        display.drawPixel(flash_x + i, SCREEN_HEIGHT - 10, SSD1306_WHITE);
                        display.drawPixel(flash_x + i, SCREEN_HEIGHT - 2, SSD1306_WHITE);
                    }
                }
                
                // 完成时的超级强调效果
                if (frame >= 92)
                {
                    uint8_t line_width = 40;
                    int16_t line_x = (SCREEN_WIDTH - line_width) / 2;
                    
                    // 三层下划线（逐层出现）
                    display.drawFastHLine(line_x, SCREEN_HEIGHT - 7, line_width, SSD1306_WHITE);
                    
                    if (frame >= 94)
                    {
                        display.drawFastHLine(line_x + 3, SCREEN_HEIGHT - 6, line_width - 6, SSD1306_WHITE);
                    }
                    
                    if (frame >= 96)
                    {
                        display.drawFastHLine(line_x + 6, SCREEN_HEIGHT - 5, line_width - 12, SSD1306_WHITE);
                    }
                    
                    // 两侧装饰光点
                    if ((frame / 3) % 2 == 0)
                    {
                        // 左侧
                        display.drawPixel(line_x - 2, SCREEN_HEIGHT - 7, SSD1306_WHITE);
                        display.drawPixel(line_x - 3, SCREEN_HEIGHT - 8, SSD1306_WHITE);
                        display.drawPixel(line_x - 2, SCREEN_HEIGHT - 9, SSD1306_WHITE);
                        
                        // 右侧
                        display.drawPixel(line_x + line_width + 1, SCREEN_HEIGHT - 7, SSD1306_WHITE);
                        display.drawPixel(line_x + line_width + 2, SCREEN_HEIGHT - 8, SSD1306_WHITE);
                        display.drawPixel(line_x + line_width + 1, SCREEN_HEIGHT - 9, SSD1306_WHITE);
                    }
                    
                    // 顶部光芒
                    if (frame >= FRAME_COUNT - 4)
                    {
                        for (int8_t i = -2; i <= 2; ++i)
                        {
                            if ((frame + i) % 3 == 0)
                            {
                                int16_t ray_x = center_x + full_text_width / 2 + i * 8;
                                display.drawPixel(ray_x, SCREEN_HEIGHT - 11, SSD1306_WHITE);
                                if ((frame + i) % 6 == 0)
                                    display.drawPixel(ray_x, SCREEN_HEIGHT - 12, SSD1306_WHITE);
                            }
                        }
                    }
                }
            }

            display.display();
            delay(FRAME_DELAY);
        }

        // === 华丽退场动画序列 ===
        
        // 1. 短暂停留，让用户欣赏完成画面
        delay(100);
        
        // 2. 三次强烈闪光效果（科技感爆发）
        constexpr uint8_t FLASH_COUNT = 3;
        constexpr uint8_t FLASH_DURATION_MS = 40;
        
        for (uint8_t flash = 0; flash < FLASH_COUNT; ++flash)
        {
            // 全屏闪白
            display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
            display.display();
            delay(FLASH_DURATION_MS);
            
            // 闪烁间隔（显示完成画面）
            display.clearDisplay();
            
            // 重新绘制完成画面（简化版）
            draw_hud_corners(100, 1.0f);
            display.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, SSD1306_WHITE);
            
            // READY文字
            display.setTextSize(1);
            const char* ready_text = "READY";
            uint8_t text_width = strlen(ready_text) * 6;
            display.setCursor((SCREEN_WIDTH - text_width) / 2, SCREEN_HEIGHT - 9);
            display.print(ready_text);
            
            // 下划线
            uint8_t line_width = 40;
            int16_t line_x = (SCREEN_WIDTH - line_width) / 2;
            display.drawFastHLine(line_x, SCREEN_HEIGHT - 7, line_width, SSD1306_WHITE);
            
            display.display();
            delay(FLASH_DURATION_MS + flash * 20);  // 间隔逐渐变长
        }
        
        // 3. 粒子爆炸退场效果（20帧）
        constexpr uint8_t EXIT_FRAMES = 20;
        constexpr uint8_t EXIT_PARTICLE_COUNT = 30;
        
        for (uint8_t exit_frame = 0; exit_frame < EXIT_FRAMES; ++exit_frame)
        {
            display.clearDisplay();
            
            float exit_progress = exit_frame / static_cast<float>(EXIT_FRAMES);
            float fade_intensity = 1.0f - exit_progress;
            
            // 中心收缩的边框
            float shrink = ease_in_out_cubic(exit_progress);
            int16_t shrink_offset = static_cast<int16_t>(20 * shrink);
            
            if (fade_intensity > 0.3f)
            {
                display.drawRect(shrink_offset, shrink_offset, 
                               SCREEN_WIDTH - 2 * shrink_offset, 
                               SCREEN_HEIGHT - 2 * shrink_offset, 
                               SSD1306_WHITE);
            }
            
            // 爆炸粒子效果（从中心向外飞散）
            int16_t center_x = SCREEN_WIDTH / 2;
            int16_t center_y = SCREEN_HEIGHT / 2;
            
            for (uint8_t p = 0; p < EXIT_PARTICLE_COUNT; ++p)
            {
                // 每个粒子有不同的角度和速度
                float angle = (p * 2.0f * PI) / EXIT_PARTICLE_COUNT;
                float speed = 1.5f + (p % 5) * 0.5f;
                
                // 粒子位置（从中心向外扩散）
                int16_t px = center_x + static_cast<int16_t>(speed * exit_frame * cos(angle));
                int16_t py = center_y + static_cast<int16_t>(speed * exit_frame * 0.6f * sin(angle));
                
                // 粒子生命周期（外围粒子先消失）
                float particle_life = 1.0f - (exit_progress * (1.0f + p / (float)EXIT_PARTICLE_COUNT));
                
                if (particle_life > 0.0f && px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                {
                    display.drawPixel(px, py, SSD1306_WHITE);
                    
                    // 粒子尾迹
                    if (particle_life > 0.5f && exit_frame > 2)
                    {
                        int16_t trail_x = center_x + static_cast<int16_t>(speed * (exit_frame - 2) * cos(angle));
                        int16_t trail_y = center_y + static_cast<int16_t>(speed * (exit_frame - 2) * 0.6f * sin(angle));
                        
                        if (trail_x >= 0 && trail_x < SCREEN_WIDTH && trail_y >= 0 && trail_y < SCREEN_HEIGHT)
                        {
                            display.drawPixel(trail_x, trail_y, SSD1306_WHITE);
                        }
                    }
                }
            }
            
            // 中心光球（逐渐缩小）
            if (fade_intensity > 0.4f)
            {
                int16_t core_size = static_cast<int16_t>(8 * fade_intensity);
                for (int16_t dx = -core_size; dx <= core_size; ++dx)
                {
                    for (int16_t dy = -core_size; dy <= core_size; ++dy)
                    {
                        if (dx * dx + dy * dy <= core_size * core_size)
                        {
                            int16_t px = center_x + dx;
                            int16_t py = center_y + dy;
                            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                            {
                                // 闪烁效果
                                if ((exit_frame + dx + dy) % 3 != 0)
                                {
                                    display.drawPixel(px, py, SSD1306_WHITE);
                                }
                            }
                        }
                    }
                }
            }
            
            // 外扩光环（冲击波效果）
            if (exit_frame < 12)
            {
                int16_t wave_radius = exit_frame * 4;
                for (int16_t angle_deg = 0; angle_deg < 360; angle_deg += 10)
                {
                    float angle_rad = angle_deg * PI / 180.0f;
                    int16_t wx = center_x + static_cast<int16_t>(wave_radius * cos(angle_rad));
                    int16_t wy = center_y + static_cast<int16_t>(wave_radius * 0.6f * sin(angle_rad));
                    
                    if (wx >= 0 && wx < SCREEN_WIDTH && wy >= 0 && wy < SCREEN_HEIGHT)
                    {
                        if ((exit_frame + angle_deg) % 20 < 10)
                        {
                            display.drawPixel(wx, wy, SSD1306_WHITE);
                        }
                    }
                }
            }
            
            // 文字淡出效果
            if (exit_frame < 8)
            {
                display.setTextSize(1);
                const char* ready_text = "READY";
                uint8_t text_width = strlen(ready_text) * 6;
                int16_t text_y = SCREEN_HEIGHT - 9 - exit_frame * 2;  // 向上飘
                
                if (text_y > 0)
                {
                    display.setCursor((SCREEN_WIDTH - text_width) / 2, text_y);
                    display.print(ready_text);
                }
            }
            
            display.display();
            delay(50);
        }
        
        // 4. 最终淡出（10帧，点阵逐渐消失）
        for (uint8_t fade_frame = 0; fade_frame < 10; ++fade_frame)
        {
            display.clearDisplay();
            
            // 随机点阵淡出效果
            for (int16_t x = 0; x < SCREEN_WIDTH; x += 2)
            {
                for (int16_t y = 0; y < SCREEN_HEIGHT; y += 2)
                {
                    // 使用伪随机但可重现的模式
                    if (((x * 7 + y * 13 + fade_frame * 5) % 10) > fade_frame)
                    {
                        display.drawPixel(x, y, SSD1306_WHITE);
                    }
                }
            }
            
            display.display();
            delay(40);
        }
        
        // 5. 完全清屏
        display.clearDisplay();
        display.display();
        delay(100);
    }
}

void my_screen_init()
{
    const uint8_t addr = normalized_address(SCREEN_I2C_ADDRESS);
    if (!display.begin(SSD1306_SWITCHCAPVCC, addr))
    {
        Serial.println("SSD1306 init failed");
        return;
    }

    // 播放品牌启动动画
    play_boot_animation();

    screen_ready = true;
    last_frame_ms = millis() - FRAME_INTERVAL_MS;
}

void my_screen_update()
{
    if (!screen_ready)
    {
        return;
    }

    const uint32_t now_ms = millis();
    if (now_ms - last_frame_ms < FRAME_INTERVAL_MS)
    {
        return;
    }
    last_frame_ms = now_ms;

    my_bat_update();

    display.clearDisplay();
    anim_phase_fast++;
    draw_mecha_shell(anim_phase_fast);
    draw_net_info(wifi_current_config(), wifi_ap_ip(), anim_phase_fast);
    display.display();
}
