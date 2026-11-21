//
// Created by Andrea Canale on 20/10/2026.
//

// This module manages the humidifier according to the humidity threshold or
// to the time-based humidification

#include "humidifier_manager.h"

#include <stdlib.h>
#include <time.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "keystore.h"
#include "mqtt_helper.h"
#include "plant_manager.h"
#include "remote_control.h"
#include "telemetry.h"
#include "temp_hum_monitor.h"

#define TAG "humidifier_manager"
#define NVS_LAST_HUMIDIFICATION_KEY "last_hum_run"

// Active-low relays close when the control pin is LOW
#ifdef CONFIG_HUMIDIFIER_GPIO_INVERTED
#define HUMIDIFIER_ON_LEVEL 0
#else
#define HUMIDIFIER_ON_LEVEL 1
#endif
#define HUMIDIFIER_OFF_LEVEL (!HUMIDIFIER_ON_LEVEL)

static time_t hum_last_run = 0;
// True while humidity_checker_task is running the humidifier (remote control excluded)
static bool s_humidifier_task_on = false;

void init_humidifier(void) {
  const gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << CONFIG_HUMIDIFIER_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = 0,
      .pull_down_en = 0,
      .intr_type = GPIO_INTR_DISABLE};
  // Set the off level before enabling the output to avoid a glitch at boot
  gpio_set_level(CONFIG_HUMIDIFIER_GPIO, HUMIDIFIER_OFF_LEVEL);
  gpio_config(&io_conf);
}

static void humidifier_on(void) {
  ESP_LOGI(TAG, "Activating humidifier");
  telemetry_send("Humidifier activated");
  gpio_set_level(CONFIG_HUMIDIFIER_GPIO, HUMIDIFIER_ON_LEVEL);
}

static void humidifier_off(void) {
  gpio_set_level(CONFIG_HUMIDIFIER_GPIO, HUMIDIFIER_OFF_LEVEL);
  telemetry_send("Humidifier deactivated");
}

static void run_humidifier(const int running_time) {
  humidifier_on();
  vTaskDelay(pdMS_TO_TICKS(running_time * 60 * 1000));
  humidifier_off();
}

static void run_humidifier_cycle(const int running_time) {
  run_humidifier(running_time);
  hum_last_run = time(NULL);
  write_key_int64(NVS_LAST_HUMIDIFICATION_KEY, hum_last_run);
}

static void delay_hours(const int hours) {
  for (int hour = 0; hour < hours; ++hour) {
    vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
  }
}

// Split the delay in hours to avoid tick overflow with long intervals
static void delay_ms(const int64_t ms) {
  delay_hours((int)(ms / (60 * 60 * 1000)));
  vTaskDelay(pdMS_TO_TICKS(ms % (60 * 60 * 1000)));
}

// --- Humidity threshold code ---

static bool is_remote_control_active(void) {
  // Semaphore is taken (held), meaning remote control is active
  const SemaphoreHandle_t sem = get_remote_control_semaphore();
  return sem != NULL && uxSemaphoreGetCount(sem) == 0;
}

static bool is_humidity_below_threshold(const float humidity,
                                        const int target_humidity) {
  return (int)humidity < target_humidity - CONFIG_HUMIDITY_THRESHOLD_RANGE;
}

/*
 * @brief Run the humidifier until the humidity is above the humidity limit(considering the threshold)
 * then wait for the next check
 */
static void run_threshold_humidification(const plant_config_t* plant_config) {
  // When remote control is active ignore threshold
  if (!is_remote_control_active()) {
    float hum = read_temperature_and_humidity().humidity;

    if (is_humidity_below_threshold(hum, plant_config->target_humidity)) {
      ESP_LOGI(TAG, "Humidity below threshold. Current hum: %f", hum);
      s_humidifier_task_on = true;
      humidifier_on();
      while (is_humidity_below_threshold(hum, plant_config->target_humidity)) {
        delay_ms(CONFIG_HUMIDIFIER_CHECK_INTERVAL_MS);
        hum = read_temperature_and_humidity().humidity;
      }
      humidifier_off();
      s_humidifier_task_on = false;
      publish_action_event(EVENT_TYPE_HUMIDIFICATION);
      ESP_LOGI(TAG, "Humidity above threshold. Current hum: %f", hum);
    }
  }

  delay_ms(CONFIG_HUMIDIFIER_CHECK_INTERVAL_MS);
}

// --- Time-based humidification code ---

/*
 * @brief Run the humidifier for the configured duration if the configured
 * frequency is elapsed since the last run, then wait for the next run
 */
static void run_time_based_humidification(const plant_config_t* plant_config) {
  const int running_time = plant_config->humidification_duration;
  const time_t next_run =
      hum_last_run + ((int64_t)plant_config->humidification_frequency * 60 * 60);

  if (time(NULL) < next_run) {
    ESP_LOGI(TAG,
             "Skipping humidification(probably ESP restart or modify). Last "
             "run was at %s",
             ctime(&hum_last_run));
    const int64_t remaining_seconds = (int64_t)difftime(next_run, time(NULL));
    delay_hours((int)(remaining_seconds / 3600));
    vTaskDelay(pdMS_TO_TICKS((remaining_seconds % 3600) * 1000));
    return;
  }

  ESP_LOGI(TAG, "Start time based humidification. Running time: %d minutes",
           running_time);
  s_humidifier_task_on = true;
  run_humidifier_cycle(running_time);
  s_humidifier_task_on = false;
  publish_action_event(EVENT_TYPE_HUMIDIFICATION);
  ESP_LOGI(TAG, "Humidification completed. Next humidification in %d hours",
           plant_config->humidification_frequency);
  delay_hours(plant_config->humidification_frequency);
}

void humidity_checker_task(void* pvParameters) {
  const plant_config_t* plant_config = (plant_config_t*)pvParameters;

  read_key_int64(NVS_LAST_HUMIDIFICATION_KEY, &hum_last_run);

  // ReSharper disable once CppDFAEndlessLoop
  // FreeRTOS task
  while (1) {
    if (plant_config->is_time_based_humidification_enabled) {
      run_time_based_humidification(plant_config);
    } else {
      run_threshold_humidification(plant_config);
    }
  }
}

void remote_humidifier_task(void* param) {
  const RemoteControlTaskParam* task_param = (RemoteControlTaskParam*)param;
  const int duration = task_param->duration;

  ESP_LOGD(TAG, "remote_humidifier_task start for %d minutes", duration);

  run_humidifier_cycle(duration);

  if (task_param->release_sem) {
    // Release the remote control semaphore when done
    const SemaphoreHandle_t sem = get_remote_control_semaphore();
    if (sem != NULL) {
      xSemaphoreGive(sem);
    }
  }

  free((void*)task_param);

  clean_humidifier_task();
  vTaskDelete(NULL);
}

void stop_humidifier(void) {
  if (!s_humidifier_task_on) {
    return;
  }

  gpio_set_level(CONFIG_HUMIDIFIER_GPIO, HUMIDIFIER_OFF_LEVEL);
  s_humidifier_task_on = false;
}
