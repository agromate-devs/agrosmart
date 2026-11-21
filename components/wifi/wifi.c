#include <keystore.h>
#include <stdlib.h>
#include <string.h>
#include <wifi.h>

#include <lwip/ip4_addr.h>
#include <smartconfig.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "light_manager.h"

#define MAX_RETRY 10
#define SSID_LENGTH 32
#define PASSWORD_LENGTH 64

/* FreeRTOS event group to signal when we are connected */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define TAG "WiFi "

static int s_retry_num = 0;
SemaphoreHandle_t wifi_connected_semaphore = NULL;

void initialise_wifi_semaphore() {
  if (wifi_connected_semaphore == NULL) {
    wifi_connected_semaphore = xSemaphoreCreateBinary();
  }
}

/**
 * @brief Deletes stored WiFi credentials and starts the smartconfig procedure.
 *
 * This function deletes the "ssid" and "password" keys from the keystore,
 * prints a warning message, and calls the smartconfig initialization function
 * to allow WiFi reconfiguration.
 */
void reset_wifi_credentials() {
  clear_key("ssid");
  clear_key("password");
  ESP_LOGW(TAG, "Starting smartconfig");
  initialise_smartconfig(true);
}

/**
 * @brief WiFi and IP event handler.
 *
 * This function is registered as a callback for handling WiFi and IP events.
 * It manages connection start, reconnection attempts, exceeding the maximum
 * number of retries, and obtaining the IP address.
 *
 * @note Parameters from esp_event definition
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    set_status_led_color(ARGB_VIOLET);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < MAX_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP. Waiting 3 seconds before retrying");
      vTaskDelay(pdMS_TO_TICKS(3000));  // Wait 3 seconds before retrying
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      ESP_LOGW(
          TAG,
          "Cannot connect to the AP after %d retries. Starting smart config.",
          s_retry_num);
      set_status_led_color(ARGB_RED);
      vTaskDelay(pdMS_TO_TICKS(
          10000));  // Wait 10 seconds to allow user see the red LED before starting smartconfig
      reset_wifi_credentials();
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_sta(const char* ssid, const char* password) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = "",
              .password = "",
          },
  };

  memcpy(wifi_config.sta.ssid, ssid, SSID_LENGTH);
  memcpy(wifi_config.sta.password, password, PASSWORD_LENGTH);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  const EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", ssid);
    if (wifi_connected_semaphore != NULL) {
      xSemaphoreGive(wifi_connected_semaphore);
    }
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", ssid);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler));
  vEventGroupDelete(s_wifi_event_group);
}

WiFiCredentials* get_wifi_credentials() {
  char* ssid = malloc(32 * sizeof(char));
  if (ssid == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for SSID.");
    return NULL;
  }
  char* password = malloc(64 * sizeof(char));
  if (password == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for WiFi password");
    free(ssid);
    return NULL;
  }
  const bool ssid_read = read_key_str("ssid", 32, &ssid);
  const bool password_read = read_key_str("password", 64, &password);

  if (!ssid_read || !password_read) {
    free(ssid);
    free(password);
    return NULL;
  }

  WiFiCredentials* credentials = malloc(sizeof(WiFiCredentials));
  if (credentials == NULL) {
    free(ssid);
    free(password);
    return NULL;
  }
  credentials->ssid = ssid;
  credentials->password = password;

  return credentials;
}