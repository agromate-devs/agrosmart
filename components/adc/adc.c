//
// Created by Andrea Canale on 21/04/2026.
//

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#define TAG "adc"
#define ADC_ATTENUATION ADC_ATTEN_DB_12

typedef struct adc_device {
  adc_oneshot_unit_handle_t oneshot_handle;
  adc_channel_t channel;
} adc_device_t;

/**
 * @brief Converts a GPIO pin number to the corresponding ADC1 channel.
 *
 * This function maps GPIO pin numbers to their respective ADC1 channel identifiers
 * based on the target ESP32 variant (ESP32, ESP32S2/S3, or others).
 *
 * Supported GPIO mappings vary by target:
 * - ESP32: GPIO 32-39 map to ADC channels 4, 5, 6, 7, 0, 1, 2, 3
 * - ESP32S2/S3: GPIO 1-10 map to ADC channels 0-9
 * - Others: GPIO 1-5 map to ADC channels 0-4
 *
 * @param[in] gpio The GPIO pin number to convert.
 *
 * @return The corresponding adc_channel_t value for the given GPIO pin.
 *
 * @note This function will abort() the program if the GPIO pin is not valid
 *       for the target device, as invalid GPIO values indicate configuration errors.
 *
 * @see adc_init()
 */
static adc_channel_t gpio_to_adc1_channel(int gpio) {
  switch (gpio) {
#ifdef CONFIG_IDF_TARGET_ESP32
    case 32:
      return ADC_CHANNEL_4;
    case 33:
      return ADC_CHANNEL_5;
    case 34:
      return ADC_CHANNEL_6;
    case 35:
      return ADC_CHANNEL_7;
    case 36:
      return ADC_CHANNEL_0;
    case 37:
      return ADC_CHANNEL_1;
    case 38:
      return ADC_CHANNEL_2;
    case 39:
      return ADC_CHANNEL_3;
#elif  defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    case 1:
      return ADC_CHANNEL_0;
    case 2:
      return ADC_CHANNEL_1;
    case 3:
      return ADC_CHANNEL_2;
    case 4:
      return ADC_CHANNEL_3;
    case 5:
      return ADC_CHANNEL_4;
    case 6:
      return ADC_CHANNEL_5;
    case 7:
      return ADC_CHANNEL_6;
    case 8:
      return ADC_CHANNEL_7;
    case 9:
      return ADC_CHANNEL_8;
    case 10:
      return ADC_CHANNEL_9;
#else
    case 1:
      return ADC_CHANNEL_0;
    case 2:
      return ADC_CHANNEL_1;
    case 3:
      return ADC_CHANNEL_2;
    case 4:
      return ADC_CHANNEL_3;
    case 5:
      return ADC_CHANNEL_4;
#endif
    default:
      ESP_LOGE(TAG, "Invalid GPIO %d for ADC1", gpio);
      abort();
  }
}

adc_device_t* adc_init(int gpio) {
  adc_device_t* device = malloc(sizeof(adc_device_t));

  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &device->oneshot_handle));

  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTENUATION,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  device->channel = gpio_to_adc1_channel(gpio);
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(device->oneshot_handle, device->channel, &
        config));
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(device->oneshot_handle, device->channel, &
        config));

  return device;
}

int adc_read(const adc_device_t* device) {
  int adc_value;

  ESP_ERROR_CHECK(
      adc_oneshot_read(device->oneshot_handle, device->channel, &adc_value));
  ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1,
           device->channel, adc_value);

  return adc_value;
}