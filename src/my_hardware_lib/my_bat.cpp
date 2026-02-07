#include <Arduino.h>
#include "esp_adc_cal.h"
#include "my_bat.h"
#include "my_config.h"

// 电压检测相关变量定义
static esp_adc_cal_characteristics_t adc_chars; 
static const adc1_channel_t channel = ADC1_CHANNEL_9;  
static const adc_bits_width_t width = ADC_WIDTH_BIT_12; 
static const adc_atten_t atten = ADC_ATTEN_DB_12;       
static const adc_unit_t unit = ADC_UNIT_1;              
float battery_voltage = 12.0; 

void my_bat_init() 
{
    // 检查eFuse中是否支持两点校准电压
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
        Serial.println("eFuse Two Point: Supported\n");
    else
        Serial.println("eFuse Two Point: NOT supported\n");
    
    // 检查eFuse中是否烧录了Vref参考电压
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
        Serial.println("eFuse Vref: Supported\n");
    else
        Serial.println("eFuse Vref: NOT supported\n");
    
    // 配置ADC1的采样宽度为12位
    adc1_config_width(width);
    // 配置ADC1通道的衰减级别为12dB
    adc1_config_channel_atten(channel, atten);
    // 根据硬件参数对ADC进行校准，存储校准参数到adc_chars
    esp_adc_cal_characterize(unit, atten, width, 0, &adc_chars);
}

void my_bat_update() 
{
    uint32_t sum = 0;
    sum = analogRead(BAT_PIN);   // 从BAT_PIN(GPIO3)读取ADC原始值
    uint32_t voltage = esp_adc_cal_raw_to_voltage(sum, &adc_chars); // 将ADC原始值转换为毫伏(mV)电压值
    battery_voltage = voltage * 4 / 1000.0;
}


