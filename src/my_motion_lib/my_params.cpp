#include "my_params.h"
#include "my_motion.h"
#include <Preferences.h>

// NVS namespace
static constexpr const char *PARAMS_NVS_NAMESPACE = "robot_params";

// NVS keys
static constexpr const char *KEY_PITCH_ZERO = "pitch_zero";
static constexpr const char *KEY_ANG_P = "ang_p";
static constexpr const char *KEY_ANG_I = "ang_i";
static constexpr const char *KEY_ANG_D = "ang_d";
static constexpr const char *KEY_SPD_P = "spd_p";
static constexpr const char *KEY_SPD_I = "spd_i";
static constexpr const char *KEY_SPD_D = "spd_d";
static constexpr const char *KEY_POS_P = "pos_p";
static constexpr const char *KEY_POS_I = "pos_i";
static constexpr const char *KEY_POS_D = "pos_d";
static constexpr const char *KEY_YAW_P = "yaw_p";
static constexpr const char *KEY_YAW_I = "yaw_i";
static constexpr const char *KEY_YAW_D = "yaw_d";

/**
 * 保存机器人参数到NVS
 * @return true 成功, false 失败
 */
bool my_params_save()
{
    Preferences pref;
    if (!pref.begin(PARAMS_NVS_NAMESPACE, false))
    {
        Serial.println("[PARAMS] Failed to open NVS for writing");
        return false;
    }

    // 保存pitch零点
    pref.putFloat(KEY_PITCH_ZERO, robot.pitch_zero);
    
    // 保存PID参数
    pref.putFloat(KEY_ANG_P, robot.ang_pid.p);
    pref.putFloat(KEY_ANG_I, robot.ang_pid.i);
    pref.putFloat(KEY_ANG_D, robot.ang_pid.d);
    
    pref.putFloat(KEY_SPD_P, robot.spd_pid.p);
    pref.putFloat(KEY_SPD_I, robot.spd_pid.i);
    pref.putFloat(KEY_SPD_D, robot.spd_pid.d);
    
    pref.putFloat(KEY_POS_P, robot.pos_pid.p);
    pref.putFloat(KEY_POS_I, robot.pos_pid.i);
    pref.putFloat(KEY_POS_D, robot.pos_pid.d);
    
    pref.putFloat(KEY_YAW_P, robot.yaw_pid.p);
    pref.putFloat(KEY_YAW_I, robot.yaw_pid.i);
    pref.putFloat(KEY_YAW_D, robot.yaw_pid.d);
    
    pref.end();
    
    Serial.println("[PARAMS] Parameters saved to NVS");
    Serial.printf("  pitch_zero: %.2f\n", robot.pitch_zero);
    Serial.printf("  ang_pid: P=%.3f I=%.3f D=%.5f\n", robot.ang_pid.p, robot.ang_pid.i, robot.ang_pid.d);
    Serial.printf("  spd_pid: P=%.5f I=%.5f D=%.5f\n", robot.spd_pid.p, robot.spd_pid.i, robot.spd_pid.d);
    Serial.printf("  pos_pid: P=%.5f I=%.5f D=%.5f\n", robot.pos_pid.p, robot.pos_pid.i, robot.pos_pid.d);
    Serial.printf("  yaw_pid: P=%.3f I=%.5f D=%.5f\n", robot.yaw_pid.p, robot.yaw_pid.i, robot.yaw_pid.d);
    
    return true;
}

/**
 * 从NVS加载机器人参数
 * @return true 成功加载, false 使用默认值
 */
bool my_params_load()
{
    Preferences pref;
    if (!pref.begin(PARAMS_NVS_NAMESPACE, true))
    {
        Serial.println("[PARAMS] No saved parameters found, using defaults");
        return false;
    }

    // 检查是否有保存的参数
    if (!pref.isKey(KEY_PITCH_ZERO))
    {
        pref.end();
        Serial.println("[PARAMS] No saved parameters found, using defaults");
        return false;
    }
    
    // 加载pitch零点
    robot.pitch_zero = pref.getFloat(KEY_PITCH_ZERO, robot.pitch_zero);
    
    // 加载PID参数
    robot.ang_pid.p = pref.getFloat(KEY_ANG_P, robot.ang_pid.p);
    robot.ang_pid.i = pref.getFloat(KEY_ANG_I, robot.ang_pid.i);
    robot.ang_pid.d = pref.getFloat(KEY_ANG_D, robot.ang_pid.d);
    
    robot.spd_pid.p = pref.getFloat(KEY_SPD_P, robot.spd_pid.p);
    robot.spd_pid.i = pref.getFloat(KEY_SPD_I, robot.spd_pid.i);
    robot.spd_pid.d = pref.getFloat(KEY_SPD_D, robot.spd_pid.d);
    
    robot.pos_pid.p = pref.getFloat(KEY_POS_P, robot.pos_pid.p);
    robot.pos_pid.i = pref.getFloat(KEY_POS_I, robot.pos_pid.i);
    robot.pos_pid.d = pref.getFloat(KEY_POS_D, robot.pos_pid.d);
    
    robot.yaw_pid.p = pref.getFloat(KEY_YAW_P, robot.yaw_pid.p);
    robot.yaw_pid.i = pref.getFloat(KEY_YAW_I, robot.yaw_pid.i);
    robot.yaw_pid.d = pref.getFloat(KEY_YAW_D, robot.yaw_pid.d);
    
    pref.end();
    
    Serial.println("[PARAMS] Parameters loaded from NVS");
    Serial.printf("  pitch_zero: %.2f\n", robot.pitch_zero);
    Serial.printf("  ang_pid: P=%.3f I=%.3f D=%.5f\n", robot.ang_pid.p, robot.ang_pid.i, robot.ang_pid.d);
    Serial.printf("  spd_pid: P=%.5f I=%.5f D=%.5f\n", robot.spd_pid.p, robot.spd_pid.i, robot.spd_pid.d);
    Serial.printf("  pos_pid: P=%.5f I=%.5f D=%.5f\n", robot.pos_pid.p, robot.pos_pid.i, robot.pos_pid.d);
    Serial.printf("  yaw_pid: P=%.3f I=%.5f D=%.5f\n", robot.yaw_pid.p, robot.yaw_pid.i, robot.yaw_pid.d);
    
    return true;
}

/**
 * 清除NVS中保存的参数（恢复默认值）
 * @return true 成功, false 失败
 */
bool my_params_clear()
{
    Preferences pref;
    if (!pref.begin(PARAMS_NVS_NAMESPACE, false))
    {
        Serial.println("[PARAMS] Failed to open NVS for clearing");
        return false;
    }
    
    pref.clear();
    pref.end();
    
    Serial.println("[PARAMS] All saved parameters cleared");
    return true;
}

