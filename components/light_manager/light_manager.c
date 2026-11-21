//
// Created by Andrea Canale on 30/04/2026.
//

#include "include/light_manager.h"

#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "led_strip.h"
#include "sys/time.h"
#include "telemetry.h"
#include "time_helper.h"

#define TAG "light_manager"

#if defined(CONFIG_LED_STRIP_TYPE_W2812)
#define LED_STRIP_TYPE LED_MODEL_WS2812
#endif

#if defined(CONFIG_LED_STRIP_TYPE_W2811)
#define LED_STRIP_TYPE LED_MODEL_WS2811
#endif

#if defined(CONFIG_LED_STRIP_TYPE_W2816)
#define LED_STRIP_TYPE LED_MODEL_WS2816
#endif

#if defined(CONFIG_LED_STRIP_TYPE_SK6812)
#define LED_STRIP_TYPE LED_MODEL_SK6812
#endif

static led_strip_handle_t status_led;
static led_strip_handle_t led_strip;

static void init_rgb_led(int gpio_num, uint32_t led_count,
                         led_model_t model_type, led_strip_handle_t* handle) {
  // LED strip general initialization, according to your led board design
  led_strip_config_t strip_config = {
      .strip_gpio_num = gpio_num,
      // The GPIO that connected to the LED strip's data line
      .max_leds = led_count,
      // The number of LEDs in the strip,
      .led_model = model_type,  // LED strip model
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      // The color order of the strip: GRB
      .flags = {
          .invert_out = false,  // don't invert the output signal
      }};

  // LED strip backend configuration: RMT
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 0,      // 10mhz by default if set to 0
      .mem_block_symbols = 0,  // Default size
      .flags = {
          .with_dma = 0,
      }};

  // LED Strip object handle
  if (led_strip_new_rmt_device(&strip_config, &rmt_config, handle) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create LED strip object");
    return;
  }

  ESP_LOGI(TAG, "Created LED object with RMT backend");
  if (led_strip_clear(*handle) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to clear LED strip");
    return;
  }  // Set all LED off to clear any previous state
}

void init_led_strip(void) {
  init_rgb_led(CONFIG_LED_STRIP_GPIO, CONFIG_LED_STRIP_LED_COUNT,
               LED_STRIP_TYPE, &led_strip);
  ESP_LOGI(TAG, "LED strip initialized and cleared");
}

void init_status_led(void) {
#if CONFIG_ENABLE_STATUS_LED
  init_rgb_led(CONFIG_STATUS_LED_GPIO, 1, LED_MODEL_WS2812, &status_led);
  ESP_LOGI(TAG, "Status LED initialized and cleared");
#endif
}

typedef struct {
  // uint8_t alpha;
  // Bits 24-31 // Not implemented(it should modulate the output to provide lower voltage)
  uint8_t red;    // Bits 16-23
  uint8_t green;  // Bits 8-15
  uint8_t blue;   // Bits 0-7
} argb32_color_t;

/**
 * @brief Parse an ARGB32 integer values into its individual color components (alpha, red, green, blue).
 *
 * @param argb32_value ARGB32 value
 * @return argb32_color_t ARGB32 parsed for each channel
 */
static argb32_color_t parse_argb32(uint32_t argb32_value) {
  const argb32_color_t color = {
      //  .alpha = (uint8_t)((argb32_value >> 24) & 0xFF),
      .red = (uint8_t)((argb32_value >> 16) & 0xFF),
      .green = (uint8_t)((argb32_value >> 8) & 0xFF),
      .blue = (uint8_t)(argb32_value & 0xFF)};

  ESP_LOGI(TAG, "Parsing argb32 color");
  ESP_LOGI(TAG, "ARGB32 value: 0x%08X", argb32_value);
  ESP_LOGI(TAG, "Parsed color - Red: %d, Green: %d, Blue: %d", color.red,
           color.green, color.blue);
  return color;
}

static void set_light_color(uint32_t argb32, int led_count,
                            led_strip_handle_t led_handle) {
  const argb32_color_t color = parse_argb32(argb32);

  for (int i = 0; i < led_count; i++) {
    ESP_ERROR_CHECK(
        led_strip_set_pixel(led_handle, i, color.red, color.green, color.blue));
  }
  ESP_ERROR_CHECK(led_strip_refresh(led_handle));
}

void set_led_strip_light_color(uint32_t argb32) {
  set_light_color(argb32, CONFIG_LED_STRIP_LED_COUNT, led_strip);
}

void set_status_led_color(uint32_t argb32) {
#if CONFIG_ENABLE_STATUS_LED
  set_light_color(argb32, 1, status_led);
#endif
}
static void light_timer(const uint32_t duration) {
  telemetry_send("Light timer started");
  ESP_LOGI(TAG, "Light timer task started for %li minutes", duration);

  vTaskDelay(pdMS_TO_TICKS(duration * 60 * 1000UL));

  ESP_LOGI(TAG, "Light timer expired after %li minutes", duration);

  telemetry_send("Light timer expired, turning off lights");
  led_strip_clear(led_strip);  // Set all LEDs off
}

// const char* time_range, const uint32_t light_color
void light_timer_task(void* arg) {
  light_task_arg_t* args = (light_task_arg_t*)arg;
  char* time_range = args->time_range;
  const uint32_t light_color = args->light_color;

  if (time_range == NULL) {
    ESP_LOGE(TAG, "Time range is NULL, light timer not started");
    return;
  }

  time_t now = 0;
  struct tm now_tm = {0};

  const light_timer_t* light_duration_raw = parse_mqtt_time(time_range);
  if (light_duration_raw == NULL) {
    ESP_LOGE(TAG, "Invalid time range '%s', light timer not started",
             time_range);
    telemetry_send("Invalid time range for light timer");
    return;
  }

  const int start_min =
      light_duration_raw->start_h * 60 + light_duration_raw->start_m;
  const int end_min =
      light_duration_raw->end_h * 60 + light_duration_raw->end_m;

  // ReSharper disable once CppDFAEndlessLoop
  // FreeRTOS task
  while (1) {
    if (!update_time(&now, &now_tm)) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    ESP_LOGI(TAG, "Current time: %s", ctime(&now));

    const int now_min = minutes_since_midnight(&now_tm);
    const int computed_time_diff = normalize_diff_minutes(now_min, start_min);
    const int light_duration = minutes_until_end(now_min, start_min, end_min);

    ESP_LOGI(TAG, "Computed time diff: %d", computed_time_diff);
    ESP_LOGI(TAG, "Light duration: %d", light_duration);

    char payload1[27] = {0};
    char payload2[27] = {0};
    sprintf(payload1, "Computed time diff: %d", computed_time_diff);
    sprintf(payload2, "Light duration: %d", light_duration);
    telemetry_send(payload1);
    telemetry_send(payload2);

    if (!is_now_in_window(now_min, start_min, end_min)) {
      telemetry_send("waiting time");
      ESP_LOGI(
          TAG,
          "Current time is before the start time. Light timer not started.");
      vTaskDelay(pdMS_TO_TICKS(computed_time_diff * 60 * 1000UL));
      continue;
    }

    if (light_duration <= 0) {
      telemetry_send("Computed non-positive light duration, skipping cycle");
      ESP_LOGW(TAG, "Computed non-positive light duration, skipping cycle");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    set_led_strip_light_color(light_color);
    light_timer((uint32_t)light_duration);
  }
}
