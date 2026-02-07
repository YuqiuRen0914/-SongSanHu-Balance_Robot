#include <Arduino.h>
#include <math.h>
#include <cmath>
#include "my_config.h"
#include "my_tool.h"

float my_lim(float value, float min, float max)
{
    float new_value = value;
    if (value < min)
        new_value = min;
    else if (value > max)
        new_value = max;
    return new_value;
}

float my_lim(float value, float lim)
{
    float new_value = value;
    if (value < -lim)
        new_value = -lim;
    else if (value > lim)
        new_value = lim;
    return new_value;
}

float my_db(float value, float deadband)
{
    float new_value = value;
    if (fabsf(value) < deadband)
        new_value = 0.0f;
    return new_value;
}

static inline void rtrim_inplace(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        --e;
    *e = '\0';
}