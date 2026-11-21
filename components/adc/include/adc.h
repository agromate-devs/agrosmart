//
// Created by Andrea Canale on 21/04/2026.
//

// Library to handle ADC1 reads

#ifndef AGROSMART_ADC_H
#define AGROSMART_ADC_H

typedef struct adc_device adc_device_t;

adc_device_t* adc_init(int gpio);
int adc_read(const adc_device_t* device);
#endif //AGROSMART_ADC_H
