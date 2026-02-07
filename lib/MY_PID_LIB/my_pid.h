#pragma once
#include <Arduino.h>

// 一阶低通滤波器，独立工具，可单独使用
struct LowPassFilter
{
    float tau;          // 时间常数（s），越大越平滑
    float state;        // 当前输出
    bool initialized;   // 是否已初始化
    uint32_t last_us;   // 上一次更新时间戳

    explicit LowPassFilter(float tau_s = 0.0f);
    void reset(float value = 0.0f);
    float apply(float input, float dt_s);
    float apply_auto(float input);    // 自动计算 dt（基于 micros）
    float operator()(float input);    // 兼容函数式调用：只传入输入值
};

// PID 配置参数
struct PIDParams
{
    float kp;
    float ki;
    float kd;
    float limit;             // 输出绝对值上限
    float integral_limit;    // 积分项绝对值上限
    float output_ramp;       // 输出斜率限制（单位：输出单位/秒），0 表示不限制
    float derivative_lpf_tau;// 微分低通时间常数

    PIDParams(float p = 0.0f,
              float i = 0.0f,
              float d = 0.0f,
              float out_lim = 1.0f,
              float int_lim = 1.0f,
              float ramp = 0.0f,
              float d_tau = 0.0f);
};

// 轻量 PID 控制器，微秒计时 + 梯形积分 + 斜率限制
class MyPID
{
public:
    explicit MyPID(const PIDParams &params = PIDParams());
    MyPID(float kp, float ki, float kd, float limit, float integral_limit, float output_ramp = 0.0f, float derivative_lpf_tau = 0.0f);

    // 兼容直接访问/赋值的写法：PID_x.P / PID_x.I / PID_x.D
    float P;
    float I;
    float D;

    float compute(float error);               // 直接输入误差
    float operator()(float error);            // 兼容函数式调用：输入误差

    void reset(float output = 0.0f, float error = 0.0f);
    float last_output() const { return last_output_; }

private:
    PIDParams cfg_;
    LowPassFilter d_filter_;
    float integral_;
    float prev_error_;
    float last_output_;
    uint32_t last_us_;
    bool has_state_;

    static float clamp(float v, float lo, float hi);
};

using PIDController = MyPID;
