//
// Created by Andrea Canale on 21/04/2026.
//

#include "../../include/soil_moisture_monitor.h"

#include <freertos/FreeRTOS.h>

#include "sdkconfig.h"
#include "adc.h"
#include "esp_log.h"
#include "driver/gpio.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#if CONFIG_SOIL_MOISTURE_GPIO < 32
#error "Soil moisture GPIO must be in ADC1(GPIO 32-39)"
#endif
#elif  defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#if CONFIG_SOIL_MOISTURE_GPIO > 10 || CONFIG_SOIL_MOISTURE_GPIO == 0
#error "Soil moisture GPIO must be in ADC1(GPIO 1-10)"
#endif
#elifdef CONFIG_IDF_TARGET_ESP32C5
#if CONFIG_SOIL_MOISTURE_GPIO > 5 || CONFIG_SOIL_MOISTURE_GPIO == 0
#error "Soil moisture GPIO must be in ADC1(GPIO 1-5)"
#endif
#endif
static adc_device_t* adc_device = NULL;
#define TAG "soil_moisture_monitor"

void init_soil_moisture_sensor() {
#if CONFIG_SOIL_MOISTURE_RESISTIVE_PROBE
  const gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << CONFIG_SOIL_MOISTURE_POWER_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = 0,
      .pull_down_en = 0,
      .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io_conf);
#endif
  adc_device = adc_init(CONFIG_SOIL_MOISTURE_GPIO);
}

// From Arduino: https://docs.arduino.cc/language-reference/en/functions/math/map/
long map(const long x, const long in_min, const long in_max, const long out_min,
         const long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int get_soil_moisture(void) {
  gpio_set_level(CONFIG_SOIL_MOISTURE_GPIO, 1);
  const int adc_value = adc_read(adc_device);
  gpio_set_level(CONFIG_SOIL_MOISTURE_GPIO, 0);
  const int moisture_percent = map(adc_value, 4095, 0, 0, 100);
  ESP_LOGI(TAG, "Soil moisture: %d%% (ADC: %d)", moisture_percent, adc_value);

  return moisture_percent;
}
