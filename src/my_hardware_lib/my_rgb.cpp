#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <string.h>
#include "my_rgb.h"
#include "my_motion.h"

static Adafruit_NeoPixel rgb_strip = Adafruit_NeoPixel(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

struct rgb_anim_state
{
    // 霓虹
    uint32_t neon_ms;
    uint16_t neon_offset;
    // 呼吸
    uint32_t breath_ms;
    uint16_t breath_phase;
    // 流星
    uint32_t meteor_ms;
    float meteor_pos_f;
    float meteor_trail[RGB_LED_COUNT];
    // 心跳
    uint32_t beat_ms;
    uint8_t beat_stage;
    uint32_t beat_stage_start;
};

static rgb_anim_state anim_state = {};

static inline uint16_t pixel_count()
{
    const uint16_t max_count = rgb_strip.numPixels();
    if (robot.rgb.rgb_count <= 0)
        return max_count;
    return robot.rgb.rgb_count > max_count ? max_count : robot.rgb.rgb_count;
}

static uint32_t color_wheel(uint8_t pos)
{
    pos = 255 - pos;
    if (pos < 85)
    {
        return rgb_strip.Color(255 - pos * 3, 0, pos * 3);
    }
    if (pos < 170)
    {
        pos -= 85;
        return rgb_strip.Color(0, pos * 3, 255 - pos * 3);
    }
    pos -= 170;
    return rgb_strip.Color(pos * 3, 255 - pos * 3, 0);
}

// 霓虹灯
static void effect_neon(uint32_t now_ms)
{
    const uint16_t interval_ms = 20;
    if (now_ms - anim_state.neon_ms < interval_ms)
        return;
    anim_state.neon_ms = now_ms;
    anim_state.neon_offset = (anim_state.neon_offset + 1) & 0xFF;

    const uint16_t count = pixel_count();
    if (count == 0)
        return;

    for (uint16_t i = 0; i < count; i++)
    {
        const uint8_t wheel_pos = (uint8_t)((i * 256 / count + anim_state.neon_offset) & 0xFF);
        rgb_strip.setPixelColor(i, color_wheel(wheel_pos));
    }
    rgb_strip.show();
}

// 呼吸灯
static void effect_breath(uint32_t now_ms)
{
    const uint16_t interval_ms = 8;
    if (now_ms - anim_state.breath_ms < interval_ms)
        return;
    anim_state.breath_ms = now_ms;
    anim_state.breath_phase = (anim_state.breath_phase + 1) % 512; // 0-511三角波

    const uint16_t count = pixel_count();
    if (count == 0)
        return;

    const uint8_t level = anim_state.breath_phase < 256 ? anim_state.breath_phase : 511 - anim_state.breath_phase;
    // 偏蓝绿色的呼吸色
    const uint8_t r = level / 10;
    const uint8_t g = level;
    const uint8_t b = (uint8_t)(level * 0.8f);

    for (uint16_t i = 0; i < count; i++)
    {
        rgb_strip.setPixelColor(i, rgb_strip.Color(r, g, b));
    }
    rgb_strip.show();
}

// 流星灯
static void effect_meteor(uint32_t now_ms)
{
    const uint32_t dt_ms = now_ms - anim_state.meteor_ms;
    if (dt_ms < 12)
        return;
    anim_state.meteor_ms = now_ms;

    const uint16_t count = pixel_count();
    if (count == 0)
        return;

    const float dt = dt_ms / 1000.0f;
    const float speed_px_per_s = 6.0f; // 平滑移动的速度（像素/秒）
    anim_state.meteor_pos_f += dt * speed_px_per_s;
    while (anim_state.meteor_pos_f >= count)
        anim_state.meteor_pos_f -= count;

    const int head_idx = (int)anim_state.meteor_pos_f;
    const float frac = anim_state.meteor_pos_f - (float)head_idx;

    const float decay_factor = expf(-dt * 7.5f); // 平滑尾迹衰减
    for (uint16_t i = 0; i < count; i++)
    {
        anim_state.meteor_trail[i] *= decay_factor;
        if (anim_state.meteor_trail[i] < 0.01f)
            anim_state.meteor_trail[i] = 0.0f;
    }

    // 头部能量双线性插值到相邻像素，尾部再补柔和衰减
    const uint8_t tail = count > 5 ? 5 : 3;
    const int idx_next = (head_idx + 1) % count;
    const float head_energy = 1.0f;
    const float next_energy = head_energy * (0.6f + 0.4f * frac);

    if (head_energy > anim_state.meteor_trail[head_idx])
        anim_state.meteor_trail[head_idx] = head_energy;
    if (next_energy > anim_state.meteor_trail[idx_next])
        anim_state.meteor_trail[idx_next] = next_energy;

    for (uint8_t j = 1; j <= tail; j++)
    {
        const int idx = (head_idx - j + count) % count;
        const float decay = expf(-(float)j * 0.55f);
        if (decay > anim_state.meteor_trail[idx])
            anim_state.meteor_trail[idx] = decay;
    }

    for (uint16_t i = 0; i < count; i++)
    {
        const float v = anim_state.meteor_trail[i];
        // 稍微加暖色和伽马以获得更丝滑的视觉
        const float gamma = powf(v, 0.85f);
        const uint8_t r = (uint8_t)(gamma * 255.0f);
        const uint8_t g = (uint8_t)(powf(v, 1.05f) * 180.0f);
        const uint8_t b = (uint8_t)(powf(v, 1.2f) * 120.0f);
        rgb_strip.setPixelColor(i, rgb_strip.Color(r, g, b));
    }
    rgb_strip.show();
}

// 心跳灯
struct beat_stage_cfg
{
    uint16_t duration_ms;
    uint8_t start_level;
    uint8_t end_level;
};

static const beat_stage_cfg HEART_STAGES[] = {
    {70, 0, 255},  // 快速上升
    {120, 255, 40}, // 瞬间回落
    {90, 40, 0},    // 间隙
    {160, 0, 200}, // 第二下
    {180, 200, 0}, // 回落
    {520, 0, 0}    // 长间隔
};
static void effect_heartbeat(uint32_t now_ms)
{
    const uint8_t stage_count = sizeof(HEART_STAGES) / sizeof(HEART_STAGES[0]);
    const beat_stage_cfg &cfg = HEART_STAGES[anim_state.beat_stage];

    const uint32_t elapsed = now_ms - anim_state.beat_stage_start;
    if (elapsed >= cfg.duration_ms)
    {
        anim_state.beat_stage = (anim_state.beat_stage + 1) % stage_count;
        anim_state.beat_stage_start = now_ms;
    }

    const beat_stage_cfg &cur = HEART_STAGES[anim_state.beat_stage];
    const uint32_t cur_elapsed = now_ms - anim_state.beat_stage_start;
    const float t = cur.duration_ms == 0 ? 1.0f : (float)cur_elapsed / (float)cur.duration_ms;
    const float level_f = (float)cur.start_level + (float)(cur.end_level - cur.start_level) * t;
    const uint8_t level = (uint8_t)level_f;

    const uint16_t count = pixel_count();
    if (count == 0)
        return;

    for (uint16_t i = 0; i < count; i++)
    {
        rgb_strip.setPixelColor(i, rgb_strip.Color(level, 0, 0));
    }
    rgb_strip.show();
}

void my_rgb_init()
{
    rgb_strip.begin();
    rgb_strip.setBrightness(96);
    rgb_strip.show();

    memset(&anim_state, 0, sizeof(anim_state));

    if (robot.rgb.rgb_count <= 0 || robot.rgb.rgb_count > RGB_LED_COUNT)
        robot.rgb.rgb_count = RGB_LED_COUNT;
    if (robot.rgb.mode < RGB_MODE_NEON || robot.rgb.mode > RGB_MODE_HEARTBEAT)
        robot.rgb.mode = RGB_MODE_NEON;

    anim_state.beat_stage_start = millis();
}

void my_rgb_update()
{
    const uint32_t now_ms = millis();
    switch (robot.rgb.mode)
    {
    case RGB_MODE_NEON:
        effect_neon(now_ms);
        break;
    case RGB_MODE_BREATH:
        effect_breath(now_ms);
        break;
    case RGB_MODE_METEOR:
        effect_meteor(now_ms);
        break;
    case RGB_MODE_HEARTBEAT:
        effect_heartbeat(now_ms);
        break;
    default:
        break;
    }
}
