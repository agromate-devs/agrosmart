//
// Created by Andrea Canale on 29/06/2026.
//

#include "water_manager.h"
#include <stdlib.h>
#include <time.h>

#include "esp_log.h"

#include "driver/gpio.h"
#include "light_manager.h"
#include "plant_manager.h"
#include "remote_control.h"
#include "soil_moisture_monitor.h"
#include "telemetry.h"
#include "keystore.h"
#include "mqtt_helper.h"
#include "water_level_sensor.h"

#define TAG "water_manager"
#define NVS_LAST_RUN_KEY "last_run"

// Active-low relays close when the control pin is LOW
#ifdef CONFIG_WATER_PUMP_GPIO_INVERTED
#define WATER_PUMP_ON_LEVEL 0
#else
#define WATER_PUMP_ON_LEVEL 1
#endif
#define WATER_PUMP_OFF_LEVEL (!WATER_PUMP_ON_LEVEL)

static time_t last_run = 0;
// True while watering_task is running the pump (remote irrigation excluded)
// Use a simple boolean variable instead of a semaphore since if not set, the task hasn't run the pump
static bool s_watering_task_pump_on = false;

void init_water_pump(void) {
  const gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << CONFIG_WATER_PUMP_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = 0,
      .pull_down_en = 0,
      .intr_type = GPIO_INTR_DISABLE};
  // Set the off level before enabling the output to avoid a glitch at boot
  gpio_set_level(CONFIG_WATER_PUMP_GPIO, WATER_PUMP_OFF_LEVEL);
  gpio_config(&io_conf);
}

static bool run_water_cycle_after_refill(int running_time);

/*
 * @brief Run the pump for running_time seconds. If check_tank is true and the
 * water tank is in reserve, the pump isn't run, to avoid running it dry, and
 * the irrigation is postponed until the tank is refilled
 *
 * @param check_tank false to skip the water tank reserve check
 * @return true if the pump has been run, false if irrigation was skipped
 */
static bool run_pump(const int running_time, const bool check_tank) {
  if (check_tank && is_water_in_reserve()) {
    ESP_LOGW(TAG, "Water tank is in reserve, irrigation postponed to refill");
    telemetry_send("Irrigation skipped: water tank in reserve");
    arm_water_refill_interrupt(run_water_cycle_after_refill, running_time);
    return false;
  }

  set_status_led_color(ARGB_BLUE);
  telemetry_send("Water pump activated");
  gpio_set_level(CONFIG_WATER_PUMP_GPIO, WATER_PUMP_ON_LEVEL);
  vTaskDelay(pdMS_TO_TICKS(running_time * 1000));
  gpio_set_level(CONFIG_WATER_PUMP_GPIO, WATER_PUMP_OFF_LEVEL);
  telemetry_send("Water pump deactivated");
  set_status_led_color(ARGB_GREEN);
  return true;
}

// Automatic irrigation only, remote irrigation calls run_pump directly
// Returns true if the pump has been run, false if the tank is in reserve
static bool run_water_cycle(const int running_time, const bool check_tank) {
  if (!run_pump(running_time, check_tank)) {
    return false;
  }
  last_run = time(NULL);
  write_key_int64(NVS_LAST_RUN_KEY, last_run);
  publish_action_event(EVENT_TYPE_IRRIGATION);
  return true;
}

// Run by the water refill interrupt: the tank has just exited the reserve, so
// skip the check to avoid arming the interrupt again. last_run is updated, so
// time based irrigation skips the irrigation already done
static bool run_water_cycle_after_refill(const int running_time) {
  return run_water_cycle(running_time, false);
}

static void delay_hours(const int hours) {
  for (int hour = 0; hour < hours; ++hour) {
    vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
  }
}

void stop_water_pump(void) {
  if (!s_watering_task_pump_on) {
    return;
  }

  gpio_set_level(CONFIG_WATER_PUMP_GPIO, WATER_PUMP_OFF_LEVEL);
  set_status_led_color(ARGB_GREEN);
  s_watering_task_pump_on = false;
}

time_t get_last_irrigation_time(void) {
  return last_run;
}

int compute_running_time(const int water_quantity) {
  return (60 * water_quantity) / CONFIG_WATER_PUMP_FLOW_RATE;
}

void remote_watering_task(void *pvParameters) {
  const RemoteControlTaskParam* task_param =
      (RemoteControlTaskParam*)pvParameters;

  // Calculate running time in seconds based on capacity of the water pump
  const int running_time =
      compute_running_time(task_param->duration);

  ESP_LOGI(TAG, "Start remote irrigation of %d ml. Running time: %d",
           task_param->duration, running_time);
  // Remote irrigation is refused if the tank is in reserve, it isn't postponed
  // to the refill like automatic irrigation
  if (is_water_in_reserve()) {
    ESP_LOGW(TAG, "Water tank is in reserve, remote irrigation refused");
    telemetry_send("Remote irrigation refused: water tank in reserve");
  } else {
    run_pump(running_time, false);
    ESP_LOGI(TAG, "Remote irrigation completed");
  }

  if (task_param->release_sem) {
    // Release the remote control semaphore when done
    const SemaphoreHandle_t sem = get_remote_control_semaphore();
    if (sem != NULL) {
      xSemaphoreGive(sem);
    }
  }

  free(pvParameters);
  vTaskDelete(NULL);
}

/*
 * @brief Run the pump until the soil moisture is above the threshold then wait
 * for the next check
 */
static void run_threshold_irrigation(const plant_config_t* plant_config,
                                     const int running_time) {
  // If the tank was in reserve, wait for the refill and the irrigation run
  // after it
  if (wait_water_refill_completion()) {
    // The soil has just been irrigated, so the probe could still read a low
    // moisture: skip the check until the next interval
    ESP_LOGI(TAG, "Irrigation done after water refill, skipping check");
  } else if (get_soil_moisture() < plant_config->soil_moisture) {
    ESP_LOGI(TAG, "Start irrigation based on soil moisture. Running time: %d",
             running_time);
    s_watering_task_pump_on = true;
    run_water_cycle(running_time, true);
    s_watering_task_pump_on = false;
  } else {
    ESP_LOGI(TAG, "Soil moisture is above threshold. No irrigation needed");
  }
  vTaskDelay(pdMS_TO_TICKS(CONFIG_SOIL_MOISTURE_CHECK_INTERVAL_MS));
}

/*
 * @brief Run the pump if the configured frequency is elapsed since the last
 * run, then wait for the next run
 */
static void run_time_based_irrigation(const plant_config_t* plant_config,
                                      const int running_time) {
  // If the tank was in reserve, wait for the refill and the irrigation run
  // after it, which updates last_run, before computing the next run
  if (!wait_water_refill_completion()) {
    const time_t next_run =
        last_run + ((int64_t)plant_config->watering_frequency * 60 * 60);

    if (time(NULL) < next_run) {
      ESP_LOGI(TAG,
               "Skipping irrigation(probably ESP restart or modify). Last "
               "run was at %s",
               ctime(&last_run));
      const int64_t remaining_seconds = (int64_t)difftime(next_run, time(NULL));
      delay_hours((int)(remaining_seconds / 3600));
      vTaskDelay(pdMS_TO_TICKS((remaining_seconds % 3600) * 1000));
      return;
    }

    ESP_LOGI(TAG, "Start time based irrigation. Running time: %d", running_time);
    s_watering_task_pump_on = true;
    run_water_cycle(running_time, true);
    s_watering_task_pump_on = false;
  }

  // Either semaphore got released(just finished to irrigate) or
  // wait_water_refill_completion returned true
  // In both case delay for the watering_frequency
  ESP_LOGI(TAG, "Irrigation completed. Next irrigation in %d hours",
           plant_config->watering_frequency);
  delay_hours(plant_config->watering_frequency);
}

void watering_task(void* pvParameters) {
  const plant_config_t* plant_config = (plant_config_t*)pvParameters;

  // Calculate running time in seconds based on capacity of the water pump
  const int running_time =
      compute_running_time(plant_config->water_quantity);

  read_key_int64(NVS_LAST_RUN_KEY, &last_run);

  // ReSharper disable once CppDFAEndlessLoop
  // FreeRTOS task
  while (1) {
    if (plant_config->is_time_based_irrigation_enabled) {
      run_time_based_irrigation(plant_config, running_time);
    } else {
      run_threshold_irrigation(plant_config, running_time);
    }
  }
}
