//
// Created by Andrea Canale on 02/04/2026.
//

// This module will read DHT values

#include "temp_hum_monitor.h"

#include <dht.h>

#include "esp_log.h"

#if defined(CONFIG_DHT_CHIP_TYPE_DHT11)
#define SENSOR_TYPE DHT_TYPE_DHT11
#endif
#if defined(CONFIG_DHT_CHIP_TYPE_AM2301)
#define SENSOR_TYPE DHT_TYPE_AM2301
#endif
#if defined(CONFIG_DHT_CHIP_SI7021)
#define SENSOR_TYPE DHT_TYPE_SI7021
#endif

#define TAG "temp_hum_monitoring"

temp_hum_data_t read_temperature_and_humidity() {
  temp_hum_data_t result;
  result.valid = false;
  result.temperature = -1.0f;
  result.humidity = -1.0f;

  float temperature;
  float humidity;

  if (dht_read_float_data(SENSOR_TYPE, CONFIG_DHT_GPIO, &humidity,
                          &temperature) == ESP_OK) {
    ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC\n", humidity, temperature);

    result.temperature = temperature;
    result.humidity = humidity;
    result.valid = true;
  } else {
    ESP_LOGE(TAG, "Could not read data from sensor\n");
  }

  return result;
}