//
// Created by Andrea Canale on 30/03/2026.
//

#ifndef PLANT_MANAGER_H
#define PLANT_MANAGER_H
#include <stdbool.h>

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define PLANT_LIGHT_COLOR_MAX_LEN 11
#define LIGHT_TIME_RANGE_MAX_LEN 12

typedef struct {
  int soil_moisture;
  int target_humidity;
  int light_intensity;
  int temperature_min;
  int temperature_max;
  char light_duration[LIGHT_TIME_RANGE_MAX_LEN];
  char light_color[PLANT_LIGHT_COLOR_MAX_LEN];
  bool is_time_based_irrigation_enabled;
  int watering_frequency;
  int water_quantity;
  bool is_time_based_humidification_enabled;
  int humidification_frequency;
  int humidification_duration;
} plant_config_t;

/**
 * @brief Parses, stores and starts plant monitoring tasks.
 *
 * @param payload The cJSON parsed payload from MQTT
 */
void start_plant_manager(cJSON* payload);

/**
 * @brief Stops plant management and clears in-memory configuration.
 */
void stop_plant_manager();

/** @brief Parse a control message received from MQTT and perform the corresponding action.
 *
 * @param payload The cJSON parsed payload from MQTT
 */
void parse_control_message(const cJSON* payload);

/**
 * @brief Delete stored plant in NVS
 */
void delete_plant();

/**
 * @brief Get the semaphore handle for remote control state
 * @return SemaphoreHandle_t semaphore handle
 */
SemaphoreHandle_t get_remote_control_semaphore();

/**
 * @brief Set remote humidifier TaskHandle to NULL
 */
void clean_humidifier_task();

/**
 * @brief Set watering TaskHandle to NULL
 */
void clean_watering_task();

#endif //PLANT_MANAGER_H