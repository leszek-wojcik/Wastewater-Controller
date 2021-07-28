#include <driver/adc.h>
#include "esp_adc_cal.h"

class ADCService
{
    private:
        esp_adc_cal_characteristics_t *adc_chars1_0;        
        esp_adc_cal_characteristics_t *adc_chars1_3;
        
    public:        
        ADCService();
        ~ADCService();

        void readAdc1(int *);
        void readAdc2(int *);
};