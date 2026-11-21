//
// Created by Andrea Canale on 02/04/2026.
//

#ifndef TEMP_HUM_MONITORING_H
#define TEMP_HUM_MONITORING_H
#include <stdbool.h>

typedef struct {
  float temperature;
  float humidity;
  bool valid;
} temp_hum_data_t;

/**
 * @brief Reads DHT data
 */
temp_hum_data_t read_temperature_and_humidity();

#endif //TEMP_HUM_MONITORING_H
