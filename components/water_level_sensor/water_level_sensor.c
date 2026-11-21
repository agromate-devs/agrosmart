//
// Created by Andrea Canale on 29/10/2026.
//

// This module reads the XKC-Y23A-NPN capacitive water level sensor

#include "water_level_sensor.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define TAG "water_level_sensor"
#define REFILL_TASK_STACK_SIZE 4096
#define REFILL_TASK_PRIORITY 5

static TaskHandle_t s_refill_task = NULL;
// Shared between water_refill_task and the tasks arming the interrupt without
// a lock: each one is a single 32 bit word, so reads and writes are atomic
static volatile water_refill_callback_t s_refill_callback = NULL;
static volatile int s_refill_callback_arg = 0;
// Taken when the interrupt is armed and given by water_refill_task once the
// callback has completed and its parameters are reset. Binary semaphore since
// it's taken and given by different tasks, which a mutex doesn't allow
static SemaphoreHandle_t s_refill_pending_sem = NULL;

static void IRAM_ATTR water_refill_isr(void* arg) {
  // One shot interrupt: disable it immediately, so water ripples around the
  // sensor don't trigger it again
  gpio_intr_disable(CONFIG_WATER_LEVEL_SENSOR_GPIO);

  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(s_refill_task, &higher_priority_task_woken); // Notify to water_refill_task the ISR
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

// The callback can block (e.g. run the pump), so it can't run in the ISR
static void water_refill_task(void* pvParameters) {
  // ReSharper disable once CppDFAEndlessLoop
  // FreeRTOS task
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Water tank refilled");
    if (s_refill_callback != NULL) {
      s_refill_callback(s_refill_callback_arg);
    }
    s_refill_callback = NULL;
    s_refill_callback_arg = 0;
    xSemaphoreGive(s_refill_pending_sem);
    // The interrupt was disabled by the ISR and stays disabled
  }
}

void init_water_level_sensor(void) {
  const gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << CONFIG_WATER_LEVEL_SENSOR_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = 1,
      .pull_down_en = 0,
      .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);

  s_refill_pending_sem = xSemaphoreCreateBinary();
  // A binary semaphore is created empty: give it since no refill is pending
  xSemaphoreGive(s_refill_pending_sem);
  xTaskCreate(water_refill_task, "water_refill_task", REFILL_TASK_STACK_SIZE,
              NULL, REFILL_TASK_PRIORITY, &s_refill_task);

  const esp_err_t err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s",
             esp_err_to_name(err));
    return;
  }
  // Interrupt type stays GPIO_INTR_DISABLE until the tank is in reserve
  gpio_isr_handler_add(CONFIG_WATER_LEVEL_SENSOR_GPIO, water_refill_isr, NULL);
}

bool is_water_in_reserve(void) {
  // The NPN output pulls the line LOW when water is detected, otherwise the
  // pull-up keeps it HIGH. So return directly the status of GPIO
  const bool in_reserve =
      gpio_get_level(CONFIG_WATER_LEVEL_SENSOR_GPIO);
  ESP_LOGI(TAG, "Water reserve: %s", in_reserve ? "yes" : "no");

  return in_reserve;
}

bool wait_water_refill_completion(void) {
  if (xSemaphoreTake(s_refill_pending_sem, 0) == pdTRUE) {
    xSemaphoreGive(s_refill_pending_sem);
    return false;
  }

  ESP_LOGI(TAG, "Waiting for water refill and its callback to complete");
  xSemaphoreTake(s_refill_pending_sem, portMAX_DELAY);
  xSemaphoreGive(s_refill_pending_sem);
  return true;
}

void arm_water_refill_interrupt(const water_refill_callback_t callback,
                                const int arg) {
  if (s_refill_callback != NULL) {
    ESP_LOGW(TAG, "Water refill interrupt already armed");
    return;
  }

  // Released by water_refill_task when the callback has completed. Can only
  // wait for the short time between water_refill_task resetting the callback
  // and giving the semaphore
  xSemaphoreTake(s_refill_pending_sem, portMAX_DELAY);
  s_refill_callback = callback;
  s_refill_callback_arg = arg;

  // The NPN output goes from HIGH to LOW when water is detected again
  gpio_set_intr_type(CONFIG_WATER_LEVEL_SENSOR_GPIO, GPIO_INTR_NEGEDGE);
  gpio_intr_enable(CONFIG_WATER_LEVEL_SENSOR_GPIO);
  ESP_LOGI(TAG, "Water refill interrupt armed");
}
