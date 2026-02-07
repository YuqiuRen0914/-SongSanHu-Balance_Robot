#pragma once

/**
 * 机器人参数持久化保存/加载模块
 * 使用ESP32的NVS（Non-Volatile Storage）保存参数
 */

/**
 * 保存机器人参数到NVS
 * @return true 成功, false 失败
 */
bool my_params_save();

/**
 * 从NVS加载机器人参数
 * @return true 成功加载, false 使用默认值
 */
bool my_params_load();

/**
 * 清除NVS中保存的参数（恢复默认值）
 * @return true 成功, false 失败
 */
bool my_params_clear();

