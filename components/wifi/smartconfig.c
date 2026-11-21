/*
 * This file is part of the Agromate project (https://github.com/xxxx or http://xxx.github.io).
 * Copyright (c) 2026 Andrea Canale.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "smartconfig.h"
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "keystore.h"
#include "light_manager.h"
#include "wifi.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
#define CONNECTED_BIT BIT0
#define ESPTOUCH_DONE_BIT BIT1

#define TAG "smartconfig"
#define SSID_LENGTH 32
#define PASSWORD_LENGTH 64

static char SSID[SSID_LENGTH];
static char PASSWORD[PASSWORD_LENGTH];

/**
 * @brief SmartConfig worker task.
 *
 * Starts SmartConfig and waits for connection/provisioning completion events.
 * On completion, persists Wi-Fi credentials and terminates itself.
 *
 * @param param Task argument (unused).
 */
static void smartconfig_task(void* param);

/**
 * @brief Handles Wi-Fi/IP/SmartConfig events during provisioning.
 *
 * @note Parameters from esp_event definition
 */
static void smart_config_event_handler(void* arg, esp_event_base_t event_base,
                                       int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
    ESP_LOGI(TAG, "Scan done");
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    ESP_LOGI(TAG, "Found channel");
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
    ESP_LOGI(TAG, "Got SSID and password");
    const smartconfig_event_got_ssid_pswd_t* evt = event_data;
    wifi_config_t wifi_config;

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password,
           sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = evt->bssid_set;

    if (wifi_config.sta.bssid_set == true) {
      memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
    }

    memcpy(SSID, evt->ssid, SSID_LENGTH);
    memcpy(PASSWORD, evt->password, PASSWORD_LENGTH);
    ESP_LOGI(TAG, "SSID:%s", SSID);
    ESP_LOGI(TAG, "PASSWORD:%s", PASSWORD);

    if (evt->type == SC_TYPE_ESPTOUCH_V2) {
      uint8_t rvd_data[128] = {0};
      ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
      ESP_LOGI(TAG, "RVD DATA:%s", rvd_data);

      // ReSharper disable once CppRedundantCastExpression
      // Project fails to build without casting on ESP-IDF 5.5.3
      char* mqtt_url = strtok((char*)rvd_data, ";");
      char* esp_password = strtok(NULL, ";");

      if (mqtt_url != NULL) {
        ESP_LOGI(TAG, "MQTT URL:%s", mqtt_url);
        write_key_str("mqtt_url", mqtt_url);
      }

      if (esp_password != NULL) {
        ESP_LOGI(TAG, "MQTT Password:%s", esp_password);
        write_key_str("esp_password", esp_password);
      }
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
    xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
  }
}

/**
 * @brief SmartConfig task implementation.
 *
 * @param param (unused).
 */
static void smartconfig_task(void* param) {
  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2));
  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

  while (1) {
    EventBits_t uxBits = xEventGroupWaitBits(s_wifi_event_group,
                                             CONNECTED_BIT | ESPTOUCH_DONE_BIT,
                                             true, false, portMAX_DELAY);

    if (uxBits & CONNECTED_BIT) {
      // wifi_connected = 1;// TODO: Save it
      ESP_LOGI(TAG, "WiFi Connected to ap");
      if (wifi_connected_semaphore != NULL) {
        xSemaphoreGive(wifi_connected_semaphore);
      }
    }

    if (uxBits & ESPTOUCH_DONE_BIT) {
      ESP_LOGI(TAG, "smartconfig over");
      ESP_LOGI(TAG, "Writing keys to storage");

      write_key_str("ssid", SSID);
      write_key_str("password", PASSWORD);

      esp_smartconfig_stop();
      vTaskDelete(NULL);
    }
  }
}

void initialise_smartconfig(bool reset) {
  set_status_led_color(ARGB_YELLOW);

  if (reset) {
    ESP_LOGI(TAG, "Stop wifi");
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
  }

  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &smart_config_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &smart_config_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
      SC_EVENT, ESP_EVENT_ANY_ID, &smart_config_event_handler, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
}