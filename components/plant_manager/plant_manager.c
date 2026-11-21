//
// Created by Andrea Canale on 30/03/2026.
//

#include "plant_manager.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "humidifier_manager.h"
#include "keystore.h"
#include "light_manager.h"
#include "monitoring_tasks.h"
#include "remote_control.h"
#include "water_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_parser/utility.h"
#include "config_parser/config_parser.h"

#define TAG "PlantManager"

static plant_config_t s_plant_config;
static SemaphoreHandle_t s_remote_control_semaphore = NULL;
static TaskHandle_t remote_watering_task_handle = NULL;
static TaskHandle_t remote_humidifier_task_handle = NULL;

static TaskHandle_t humidity_checker_task_handle = NULL;
static TaskHandle_t watering_task_handle = NULL;
static TaskHandle_t sensor_checking_task_handle = NULL;
static TaskHandle_t light_task_handle = NULL;

static void start_plant_tasks(const plant_config_t* plant_config) {
  const uint32_t int_light_color =
      strtoul(plant_config->light_color, NULL, 10);
  if (int_light_color != 0 && plant_config->light_duration[0] != '\0') {
    //start_light_timer(plant_config->light_duration, int_light_color);
    light_task_arg_t* light_task_arg = malloc(sizeof(light_task_arg_t));
    if (light_task_arg == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for light_task_arg_t");
      abort();
    }
    light_task_arg->light_color = int_light_color;
    light_task_arg->time_range = malloc(sizeof(char) * 12);
    if (light_task_arg->time_range == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for light_task_arg_t");
      abort();
    }
    strcpy(light_task_arg->time_range, plant_config->light_duration);

    xTaskCreate(light_timer_task, "light_timer_task", 8192,
                (void*)light_task_arg, 5, &light_task_handle);
  }

  xTaskCreate(sensor_checking, "sensor_checking_task", 4096,
              (void*)plant_config, 5,
              &sensor_checking_task_handle);
  xTaskCreate(watering_task, "watering_task", 4096, (void*)plant_config, 5,
              &watering_task_handle);

  xTaskCreate(humidity_checker_task, "humidity_checker_task", 4096,
              (void*)plant_config, 5, &humidity_checker_task_handle);
}

void start_plant_manager(cJSON* payload) {
  if (!parse_plant_config_json(payload, &s_plant_config)) {
    ESP_LOGE(TAG, "Cannot start plant manager: invalid configuration");
    return;
  }

  // Create the semaphore if it doesn't exist
  if (s_remote_control_semaphore == NULL) {
    s_remote_control_semaphore = xSemaphoreCreateBinary();
    if (s_remote_control_semaphore == NULL) {
      ESP_LOGE(TAG, "Failed to create remote control semaphore");
      return;
    }
    // Initially not taken (remote control not active)
    xSemaphoreGive(s_remote_control_semaphore);
  }

  ESP_LOGI(TAG,
           "Config loaded. TimeBasedIrrigation=%d TimeBasedHumidification=%d",
           s_plant_config.is_time_based_irrigation_enabled,
           s_plant_config.is_time_based_humidification_enabled);

  // Debug: Print entire config struct
  ESP_LOGI(TAG, "=== Plant Config Debug ===");
  ESP_LOGI(TAG, "soil_moisture: %d", s_plant_config.soil_moisture);
  ESP_LOGI(TAG, "target_humidity: %d", s_plant_config.target_humidity);
  ESP_LOGI(TAG, "light_intensity: %d", s_plant_config.light_intensity);
  ESP_LOGI(TAG, "temperature_min: %d", s_plant_config.temperature_min);
  ESP_LOGI(TAG, "temperature_max: %d", s_plant_config.temperature_max);
  ESP_LOGI(TAG, "light_duration: %s", s_plant_config.light_duration);
  ESP_LOGI(TAG, "light_color: %s", s_plant_config.light_color);
  ESP_LOGI(TAG, "is_time_based_irrigation_enabled: %d",
           s_plant_config.is_time_based_irrigation_enabled);
  ESP_LOGI(TAG, "watering_frequency: %d", s_plant_config.watering_frequency);
  ESP_LOGI(TAG, "water_quantity: %d ml", s_plant_config.water_quantity);
  ESP_LOGI(TAG, "is_time_based_humidification_enabled: %d",
           s_plant_config.is_time_based_humidification_enabled);
  ESP_LOGI(TAG, "humidification_frequency: %d",
           s_plant_config.humidification_frequency);
  ESP_LOGI(TAG, "humidification_duration: %d min",
           s_plant_config.humidification_duration);
  ESP_LOGI(TAG, "=========================");

  start_plant_tasks(&s_plant_config);
}

static void delete_and_clean_task(TaskHandle_t* task) {
  if (task != NULL && *task != NULL) {
    vTaskDelete(*task);
    *task = NULL;
  }
}

void stop_plant_manager() {
  delete_and_clean_task(&sensor_checking_task_handle);
  // GPIOs are stopped after the tasks are deleted, so the tasks can't turn
  // them on again
  delete_and_clean_task(&humidity_checker_task_handle);
  stop_humidifier();
  delete_and_clean_task(&watering_task_handle);
  stop_water_pump();
  delete_and_clean_task(&light_task_handle);

  memset(&s_plant_config, 0, sizeof(s_plant_config));

  if (s_remote_control_semaphore != NULL) {
    vSemaphoreDelete(s_remote_control_semaphore);
    s_remote_control_semaphore = NULL;
  }
}

void delete_plant() {
  stop_plant_manager();
  ESP_LOGI(TAG, "Received delete command. Nothing to do");
  // The deletion is managed from MQTT broker side
  // The client are deleted from the broker. Agrosmart needs to be paired again
  clear_key("mqtt_url");
  clear_key("ssid");
  clear_key("password");
  esp_restart(); // Reboot the ESP to restart the pairing process
}

SemaphoreHandle_t get_remote_control_semaphore() {
  return s_remote_control_semaphore;
}

void parse_control_message(const cJSON* payload) {
  int hydrate;
  int humidify;
  if (!parse_int_field(payload, "hydrate", &hydrate) ||
      !parse_int_field(payload, "humidify", &humidify)) {
    ESP_LOGE(TAG, "Invalid control message");
    return;
  }
  if (hydrate < 0 || humidify < 0) {
    ESP_LOGE(TAG, "Invalid control message: negative values");
    return;
  }
  if (hydrate == 0 && humidify == 0) {
    ESP_LOGI(TAG, "Control message without commands. Nothing to do");
    return;
  }

  // Allocate everything before taking the semaphore, so a failure can't leave it taken
  RemoteControlTaskParam* hydrate_param = NULL;
  RemoteControlTaskParam* humidify_param = NULL;
  if (hydrate != 0) {
    hydrate_param = malloc(sizeof(RemoteControlTaskParam));
  }
  if (humidify != 0) {
    humidify_param = malloc(sizeof(RemoteControlTaskParam));
  }
  if ((hydrate != 0 && hydrate_param == NULL) ||
      (humidify != 0 && humidify_param == NULL)) {
    ESP_LOGE(TAG, "Failed to allocate memory for RemoteControlTaskParam");
    free(hydrate_param);
    free(humidify_param);
    return;
  }

  // hydrate is in ml and humidify in minutes: compare the running times in seconds
  const int hydrate_seconds = hydrate != 0 ? compute_running_time(hydrate) : 0;
  const int humidify_seconds = humidify * 60;
  // Both tasks start together, so the longest one releases the semaphore
  const bool hydrate_releases = hydrate_seconds >= humidify_seconds;

  // Take the semaphore to indicate remote control is active
  if (s_remote_control_semaphore != NULL) {
    xSemaphoreTake(s_remote_control_semaphore, portMAX_DELAY);
  }

  if (hydrate_param != NULL) {
    hydrate_param->duration = hydrate;
    hydrate_param->release_sem = hydrate_releases;

    ESP_LOGI(TAG, "Received control command: Hydrate plant");
    if (xTaskCreate(&remote_watering_task, "remote_watering_task",
                REMOTE_WATERING_TASK_STACK_SIZE,
                (void*)hydrate_param, 5, &remote_watering_task_handle) == pdFALSE) {
      free(hydrate_param);
      if (hydrate_releases && s_remote_control_semaphore != NULL) {
          xSemaphoreGive(s_remote_control_semaphore);
      }
    }
  }

  if (humidify_param != NULL) {
    humidify_param->duration = humidify;
    humidify_param->release_sem = !hydrate_releases;

    ESP_LOGI(TAG, "Received control command: Humidify plant");
    if (xTaskCreate(&remote_humidifier_task, "remote_humidifier_task",
                REMOTE_CONTROL_TASK_STACK_SIZE,
                (void*)humidify_param, 5, &remote_humidifier_task_handle) == pdFALSE) {
      free(humidify_param);
      if (!hydrate_releases && s_remote_control_semaphore != NULL) {
          xSemaphoreGive(s_remote_control_semaphore);
      }
    }
  }
}

void clean_humidifier_task() {
  remote_humidifier_task_handle = NULL;
}

void clean_watering_task() {
  watering_task_handle = NULL;
}