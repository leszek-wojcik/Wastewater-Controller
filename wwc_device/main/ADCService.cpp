#include "ADCService.h"
#include "esp32/rom/ets_sys.h"

ADCService::ADCService()
{
    adc_chars1_0 = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));
    adc_chars1_3 = (esp_adc_cal_characteristics_t*) calloc(1, sizeof(esp_adc_cal_characteristics_t));

    esp_adc_cal_characterize( 
        (adc_unit_t) 1, 
        ADC_ATTEN_DB_11, 
        ADC_WIDTH_BIT_12, 
        1100, 
        adc_chars1_0);
    
    esp_adc_cal_characterize( 
        (adc_unit_t) 1, 
        ADC_ATTEN_DB_11, 
        ADC_WIDTH_BIT_12, 
        1100, 
        adc_chars1_3);
            
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);    
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);    

}

ADCService::~ADCService()
{
    free(adc_chars1_0);
    free(adc_chars1_3);
}

void ADCService::readAdc1(int *val)
{
    uint32_t voltage[20];
    uint32_t maxVoltage;
    uint32_t minVoltage;

    esp_adc_cal_get_voltage(ADC_CHANNEL_0, adc_chars1_0, &voltage[0]);
    maxVoltage = voltage[0];
    minVoltage = voltage[0];

    ets_delay_us(1000);
    for (int i=1; i<20; i++)
    {
        esp_adc_cal_get_voltage(ADC_CHANNEL_0, adc_chars1_0, &voltage[i]);
        ets_delay_us(1000);
        if (voltage[i] > maxVoltage)
            maxVoltage = voltage[i];
        
        if (voltage[i] < minVoltage)
            minVoltage = voltage[i];

    }

    *val =  maxVoltage - minVoltage;
}

void ADCService::readAdc2(int *val)
{
    uint32_t voltage[20];
    uint32_t maxVoltage;
    uint32_t minVoltage;

    esp_adc_cal_get_voltage(ADC_CHANNEL_3, adc_chars1_3, &voltage[0]);
    maxVoltage = voltage[0];
    minVoltage = voltage[0];

    ets_delay_us(1000);
    for (int i=1; i<20; i++)
    {
        esp_adc_cal_get_voltage(ADC_CHANNEL_3, adc_chars1_3, &voltage[i]);
        ets_delay_us(1000);
        if (voltage[i] > maxVoltage)
            maxVoltage = voltage[i];
        
        if (voltage[i] < minVoltage)
            minVoltage = voltage[i];

    }

    *val = maxVoltage - minVoltage;
}