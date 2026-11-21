//
// Created by Andrea Canale on 30/05/2026.
//

#include <stdbool.h>
#include "cJSON.h"
#include "utility.h"

#include "esp_log.h"
#include "plant_manager.h"

#define TAG "config_parser"

bool parse_plant_config_json(cJSON* root, plant_config_t* out_config) {
  if (root == NULL || out_config == NULL) {
    ESP_LOGE(TAG, "Invalid parse input");
    return false;
  }

  plant_config_t config = {0};
  const cJSON* light_color_item =
      cJSON_GetObjectItemCaseSensitive(root, "lightColor");
  if (!parse_required_bool_field(root, "isTimeBasedIrrigationEnabled",
                                 &config.is_time_based_irrigation_enabled)) {
    ESP_LOGE(TAG, "Field 'isTimeBasedIrrigationEnabled' parsing failed");
    goto error;
  }

  if (!parse_int_field(root, "soilMoisture", &config.soil_moisture) &&
      !config.is_time_based_irrigation_enabled) {
    ESP_LOGW(TAG, "Field 'soilMoisture' parsing failed");
    goto error;
  }

  if (!parse_int_field(root, "targetHumidity", &config.target_humidity)) {
    ESP_LOGE(TAG, "Field 'targetHumidity' parsing failed");
    goto error;
  }

  if (!parse_int_field(root, "lightIntensity", &config.light_intensity)) {
    ESP_LOGE(TAG, "Field 'lightIntensity' parsing failed");
    goto error;
  }

  if (!parse_int_field(root, "temperatureMin", &config.temperature_min)) {
    ESP_LOGE(TAG, "Field 'temperatureMin' parsing failed");
    goto error;
  }

  if (!parse_int_field(root, "temperatureMax", &config.temperature_max)) {
    ESP_LOGE(TAG, "Field 'temperatureMax' parsing failed");
    goto error;
  }

  const cJSON* light_duration_item =
      cJSON_GetObjectItemCaseSensitive(root, "lightDuration");
  if (!cJSON_IsString(light_duration_item) ||
      light_duration_item->valuestring == NULL) {
    ESP_LOGE(TAG, "Field 'lightDuration' missing or not string");
    goto error;
  }
  if (strlen(light_duration_item->valuestring) >=
      sizeof(config.light_duration)) {
    ESP_LOGE(TAG, "Field 'lightDuration' too long");
    goto error;
  }
  strncpy(config.light_duration, light_duration_item->valuestring,
          sizeof(config.light_duration) - 1U);
  config.light_duration[sizeof(config.light_duration) - 1U] = '\0';

  if (!cJSON_IsString(light_color_item) ||
      light_color_item->valuestring == NULL) {
    ESP_LOGE(TAG, "Field 'lightColor' missing or not string");
    goto error;
  }

  if (strlen(light_color_item->valuestring) >= sizeof(config.light_color)) {
    ESP_LOGE(TAG, "Field 'lightColor' too long");
    goto error;
  }

  strncpy(config.light_color, light_color_item->valuestring,
          sizeof(config.light_color) - 1U);
  config.light_color[sizeof(config.light_color) - 1U] = '\0';

  if (!parse_int_field(root, "wateringFrequency", &config.watering_frequency) &&
      config.is_time_based_irrigation_enabled) {
    ESP_LOGE(
        TAG,
        "Field 'wateringFrequency' parsing failed and missing soil_moisture");
    goto error;
  }

  if (!parse_int_field(root, "waterQuantity", &config.water_quantity) &&
      config.is_time_based_irrigation_enabled) {
    ESP_LOGE(TAG,
             "Field 'waterQuantity' parsing failed and missing soil_moisture");
    goto error;
  }

  if (!parse_optional_bool_field(
          root, "isTimeBasedHumidificationEnabled", false,
          &config.is_time_based_humidification_enabled)) {
    ESP_LOGW(TAG, "Field 'isTimeBasedHumidificationEnabled' parsing failed. Using default value.");
    config.is_time_based_humidification_enabled = false;
  } else {
    if (config.is_time_based_humidification_enabled) {
      if (!parse_int_field(root, "humidificationFrequency",
                           &config.humidification_frequency)) {
        ESP_LOGE(TAG, "Field 'humidificationFrequency' parsing failed");
        goto error;
                           }

      if (!parse_int_field(root, "humidificationDuration",
                           &config.humidification_duration) ) {
        ESP_LOGE(TAG, "Field 'humidificationDuration' parsing failed");
        goto error;
      }
    }
  }

  *out_config = config;
  return true;

error:
  return false;
}