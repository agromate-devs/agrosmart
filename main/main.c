#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <sys/time.h>

#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "humidifier_manager.h"
#include "light_manager.h"
#include "mqtt_helper.h"
#include "nvs_flash.h"
#include "ota_manager.h"
#include "sensors.h"
#include "smartconfig.h"
#include "telemetry.h"
#include "water_level_sensor.h"
#include "water_manager.h"
#include "wifi.h"

#define TAG "main"
uint8_t mac[6];

/**
 * @brief Initializes network interfaces and Wi-Fi driver.
 */
static void init_network(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  initialise_wifi_semaphore();
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

bool plant_assigned = false;

/**
 * @brief Initializes SNTP for system time synchronization via NTP.
 *
 * Configures SNTP to poll pool.ntp.org and sets CET/CEST timezone
 * so that POSIX functions like gettimeofday() return the correct local time.
 */
static void init_sntp(void) {
  ESP_LOGI(TAG, "Initializing SNTP");
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, CONFIG_NTP_SERVER);
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_init();

  // Set timezone to Europe/Rome (CET-1 / CEST-2 with DST)
  // TODO: Support custom timezone
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  ESP_LOGI(TAG, "SNTP initialized, waiting for time sync...");
}

/**
 * @brief FreeRTOS task that waits for Wi-Fi connection and then starts SNTP.
 *
 * Once Wi-Fi is connected (signaled via wifi_connected_semaphore),
 * SNTP is initialized and the task waits for a valid system time.
 */
static void sntp_task(void* pvParameters) {
  // Wait the Wi-Fi connection semaphore
  if (xSemaphoreTake(wifi_connected_semaphore, portMAX_DELAY) == pdTRUE) {
    init_sntp();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry_cnt = 0;

    while ((timeinfo.tm_year == 70 || now == 0) &&
           ++retry_cnt < CONFIG_NTP_RETRY_COUNT) {
      ESP_LOGI(TAG,
               "Waiting for system time to be set... (%d/%d). Current time: "
               "%s. Unix timestamp: %i",
               retry_cnt, CONFIG_NTP_RETRY_COUNT, ctime(&now), now);
      vTaskDelay(pdMS_TO_TICKS(CONFIG_NTP_TIMEOUT * 1000));
      if (time(&now) == -1) {
        ESP_LOGE(TAG, "Failed to get system time from unix timestamp");
        now = 0;
        break;
      };
      localtime_r(&now, &timeinfo);
      ESP_LOGI(TAG, "Current year: %d", timeinfo.tm_year + 1900);
    }

    if (now != 0 && retry_cnt <= CONFIG_NTP_RETRY_COUNT) {
      ESP_LOGI(TAG, "System time synchronized via SNTP. Current time: %s",
               ctime(&now));
    } else {
      ESP_LOGW(TAG, "SNTP time sync timed out, continuing anyway");
    }
  }

#if CONFIG_ENABLE_AUTO_OTA_UPDATE
  enable_auto_update();
#endif

  // Release semaphore for the MQTT task
  xSemaphoreGive(wifi_connected_semaphore);
  telemetry_send("SNTP connected");
  if (esp_reset_reason() == ESP_RST_PANIC) {
    ESP_LOGW(TAG, "Reboot caused by panic");
    telemetry_send("Reboot caused by panic");
  }
  vTaskDelete(NULL);
}

void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  init_network();
  xTaskCreate(sntp_task, "sntp_task", 4096, NULL, 3, NULL);
  init_humidifier();
  init_soil_moisture_sensor();
  init_water_level_sensor();
  init_water_pump();
  init_status_led();
  init_led_strip();

  xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 3, NULL);

  const WiFiCredentials* credentials = get_wifi_credentials();
  if (credentials == NULL) {
    ESP_LOGI(TAG, "Starting smartconfig");
    initialise_smartconfig(false);
  } else {
    ESP_LOGI(TAG,
             "WiFi credentials found in keystore. Starting WiFi connection.");
    wifi_init_sta(credentials->ssid, credentials->password);
    free(credentials->ssid);
    free(credentials->password);
    free((void *)credentials);
  }
}