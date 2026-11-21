//
// Created by Andrea Canale on 02/04/2026.
//

#include "monitoring_tasks.h"

#include <stdint.h>
#include <stdio.h>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/soil_moisture_monitor.h"
#include "mqtt_helper.h"
#include "plant_manager.h"
#include "temp_hum_monitor.h"
#include "water_level_sensor.h"
#include "water_manager.h"

#define TAG "plant_manager_tasks"

typedef struct {
  float temperature;
  float humidity;
  int soil_moisture;
  time_t last_irrigation;
  bool water_reserve;
} metric_message;

/**
 * @brief Converts a metric_message structure to a JSON object.
 *
 * @param metric Input metric values.
 * @return Newly allocated cJSON object, or NULL on allocation failure.
 * @note cJSON object must be freed after usage
 */
static cJSON* metric_to_json(metric_message* metric) {
  extern uint8_t mac[6];
  cJSON* json = cJSON_CreateObject();
  if (json == NULL) {
    return NULL;
  }

  char mac_str[13];
  snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);

  cJSON_AddStringToObject(json, "id", mac_str);  // Necessary for Telegraf
  cJSON_AddStringToObject(json, "type", EVENT_TYPE_METRIC);
  cJSON_AddNumberToObject(json, "temperature", metric->temperature);
  cJSON_AddNumberToObject(json, "humidity", metric->humidity);
  cJSON_AddNumberToObject(json, "soil_moisture", metric->soil_moisture);
  cJSON_AddNumberToObject(json, "last_irrigation",
                          (double)metric->last_irrigation);
  cJSON_AddBoolToObject(json, "water_reserve", metric->water_reserve);
  return json;
}

void sensor_checking(void* param) {
  const plant_config_t* s_plant_config = (plant_config_t*)param;

  // ReSharper disable once CppDFAEndlessLoop
  // FreeRTOS task
  while (1) {
    ESP_LOGI(TAG, "Temp min-max: %d %d", s_plant_config->temperature_min,
             s_plant_config->temperature_max);
    ESP_LOGI(TAG, "Humidity: %d", s_plant_config->target_humidity);

    const temp_hum_data_t data = read_temperature_and_humidity();

    metric_message metrics;
    metrics.temperature = data.temperature;
    metrics.humidity = data.humidity;
    metrics.soil_moisture = get_soil_moisture();
    metrics.last_irrigation = get_last_irrigation_time();
    metrics.water_reserve = is_water_in_reserve();

    cJSON* json = metric_to_json(&metrics);
    char* json_string = cJSON_PrintUnformatted(json);
    ESP_LOGI(TAG, "JSON payload: %s", json_string);
    publish_mqtt_message(METRICS_TOPIC, json_string, 0);

    free(json_string);
    cJSON_Delete(json);

    vTaskDelay(pdMS_TO_TICKS(CONFIG_METRIC_PUBLISH_INTERVAL * 1000));
  }
}