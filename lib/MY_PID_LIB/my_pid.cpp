#include "my_pid.h"
#include <math.h>

LowPassFilter::LowPassFilter(float tau_s)
    : tau(tau_s), state(0.0f), initialized(false), last_us(0)
{
}

void LowPassFilter::reset(float value)
{
    state = value;
    initialized = true;
    last_us = micros();
}

float LowPassFilter::apply(float input, float dt_s)
{
    if (tau <= 0.0f || dt_s <= 0.0f)
    {
        state = input;
        initialized = true;
        return input;
    }

    if (!initialized)
    {
        reset(input);
        return input;
    }

    const float alpha = dt_s / (tau + dt_s); // 一阶低通滤波器系数
    state += alpha * (input - state);
    return state;
}

float LowPassFilter::apply_auto(float input)
{
    const uint32_t now = micros();
    float dt_s = initialized ? static_cast<float>(now - last_us) * 1e-6f : 0.0f;
    if (dt_s <= 0.0f || dt_s > 0.5f)
    {
        dt_s = 1e-3f; // 默认最小步长，防止异常 dt
    }
    last_us = now;
    return apply(input, dt_s);
}

float LowPassFilter::operator()(float input)
{
    return apply_auto(input);
}

PIDParams::PIDParams(float p, float i, float d, float out_lim, float int_lim, float ramp, float d_tau)
    : kp(p),
      ki(i),
      kd(d),
      limit(out_lim),
      integral_limit(int_lim),
      output_ramp(ramp),
      derivative_lpf_tau(d_tau)
{
}

MyPID::MyPID(const PIDParams &params)
    : cfg_(params),
      d_filter_(params.derivative_lpf_tau),
      integral_(0.0f),
      prev_error_(0.0f),
      last_output_(0.0f),
      last_us_(0),
      has_state_(false),
      P(params.kp),
      I(params.ki),
      D(params.kd)
{
    cfg_.limit = fabsf(cfg_.limit);
    cfg_.integral_limit = fabsf(cfg_.integral_limit);
    cfg_.output_ramp = params.output_ramp;
    cfg_.derivative_lpf_tau = params.derivative_lpf_tau;
    d_filter_.tau = cfg_.derivative_lpf_tau;
    d_filter_.initialized = false;
}

MyPID::MyPID(float kp, float ki, float kd, float limit, float integral_limit, float output_ramp, float derivative_lpf_tau)
    : MyPID(PIDParams(kp, ki, kd, limit, integral_limit, output_ramp, derivative_lpf_tau))
{
}

float MyPID::clamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void MyPID::reset(float output, float error)
{
    const float out_lim = fabsf(cfg_.limit);
    const float int_lim = fabsf(cfg_.integral_limit);
    integral_ = 0.0f;
    prev_error_ = error;
    last_output_ = clamp(output, -out_lim, out_lim);
    last_us_ = micros();
    has_state_ = false;
    d_filter_.reset(0.0f);
    integral_ = clamp(integral_, -int_lim, int_lim);
}

float MyPID::operator()(float error)
{
    return compute(error);
}

float MyPID::compute(float error)
{
    // 先同步外部赋值的 P/I/D
    cfg_.kp = P;
    cfg_.ki = I;
    cfg_.kd = D;

    const uint32_t now_us = micros();
    if (!has_state_)
    {
        // 首次调用：仅用当前误差初始化，避免微分尖峰
        prev_error_ = error;
        last_us_ = now_us;
        has_state_ = true;
        const float out_lim = fabsf(cfg_.limit);
        const float int_lim = fabsf(cfg_.integral_limit);
        last_output_ = clamp(cfg_.kp * error, -out_lim, out_lim);
        integral_ = clamp(0.0f, -int_lim, int_lim);
        d_filter_.initialized = false;
        return last_output_;
    }

    float dt = static_cast<float>(now_us - last_us_) * 1e-6f;
    if (dt <= 0.0f || dt > 0.5f)
    {
        dt = 1e-3f; // 防止异常大间隔
    }

    // 梯形积分
    integral_ += 0.5f * (error + prev_error_) * dt * cfg_.ki;
    const float int_lim = fabsf(cfg_.integral_limit);
    integral_ = clamp(integral_, -int_lim, int_lim);

    // 微分（可选低通）
    const float derr = (error - prev_error_) / (dt > 0.0f ? dt : 1e-6f);
    const float d_term = cfg_.kd * ((cfg_.derivative_lpf_tau > 0.0f) ? d_filter_.apply(derr, dt) : derr);

    // PID 输出
    float output = cfg_.kp * error + integral_ + d_term;
    const float out_lim = fabsf(cfg_.limit);
    output = clamp(output, -out_lim, out_lim);

    // 斜率限制
    if (cfg_.output_ramp > 0.0f && has_state_)
    {
        const float max_step = cfg_.output_ramp * dt;
        output = clamp(output, last_output_ - max_step, last_output_ + max_step);
    }

    last_output_ = output;
    prev_error_ = error;
    last_us_ = now_us;
    return output;
}
