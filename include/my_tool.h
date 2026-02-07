#pragma once

#include "my_config.h"

float my_lim(float value, float min, float max);
float my_lim(float value, float lim);
float my_db(float value, float deadband);
static inline void rtrim_inplace(char *s);