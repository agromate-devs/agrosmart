//
// Created by Andrea Canale on 30/03/2026.
//

#include "mqtt_connection_handler.h"

#include <esp_log.h>
#include <esp_mac.h>
#include <mqtt_client.h>
#include "cJSON.h"
#include "keystore.h"

#include "../ota_manager/include/ota_manager.h"
#include "../telemetry/include/telemetry.h"
#include "light_manager.h"
#include "plant_manager.h"

#define TAG "mqtt_connection_handler"

static char setting_topic[37];  // Shared for MQTT_EVENT_DATA

/**
 * @brief Subscribes to the per-device settings topic.
 *
 * @param client MQTT client handle.
 * @param mac Device MAC address.
 */
static void listen_for_changes(esp_mqtt_client_handle_t client,
                               uint8_t mac[6]) {
  snprintf(setting_topic, sizeof(setting_topic),
           "agrosmart/%02X%02X%02X%02X%02X%02X/settings", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  if (esp_mqtt_client_subscribe(client, setting_topic, 0) == 1) {
    ESP_LOGE(TAG, "Failed to subscribe");
  };  // Listen for changes
}

static void send_agrosmart_info(void* pvParameters) {
  ESP_LOGI(TAG, "Sending agrosmart info");
  esp_mqtt_client_handle_t client = pvParameters;
  const char* payload = get_agrosmart_info();
  ESP_LOGI(TAG, "Payload: %s", payload);
  if (payload == NULL) {
    ESP_LOGE(TAG, "Invalid JSON payload");
    telemetry_send("Failed to get Agrosmart info");
    vTaskDelete(NULL);
  }

  extern uint8_t mac[6];
  char info_topic[28];
  snprintf(info_topic, sizeof(info_topic),
           "agrosmart/%02X%02X%02X%02X%02X%02X/info", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

  // Don't set retain to avoid server read old info
  if (esp_mqtt_client_publish(client, info_topic, payload, 0, 0, 0) < 0) {
    ESP_LOGE(TAG, "Failed to publish info");
    telemetry_send("Failed to publish info");
  }

  free((void*)payload);
  vTaskDelete(NULL);
}

/**
 * @brief Handles a settings payload received from MQTT.
 *
 * Stops the current plant manager if already running and starts it again
 * with the new JSON configuration.
 *
 * @param event MQTT event containing topic and payload data.
 */
static void handle_setting_update(esp_mqtt_event_handle_t event) {
  extern bool plant_assigned;

  size_t len = event->data_len;
  char data[len + 1];
  strncpy(data, event->data, event->data_len);
  data[event->data_len] = '\0';

  cJSON* payload = cJSON_Parse(data);

  if (payload == NULL) {
    ESP_LOGE(TAG, "Invalid JSON payload");
    return;
  }

  const cJSON* item = cJSON_GetObjectItemCaseSensitive(payload, "type");

  if (cJSON_IsString(item) && item->valuestring != NULL) {
    ESP_LOGI(TAG, "Received command of type: %s", item->valuestring);
  } else {
    ESP_LOGE(TAG,
             "Received command without 'type' field or 'type' is not a string. "
             "Invalid MQTT message");
    return;
  }

  if (strcmp(item->valuestring, "delete") == 0) {
    if (!plant_assigned) {
      ESP_LOGW(TAG, "No plant assigned, cannot delete");
      return;
    }
    delete_plant();
  } else if (strcmp(item->valuestring, "control") == 0) {
    if (!plant_assigned) {
      ESP_LOGW(TAG, "No plant assigned, cannot control");
      return;
    }
    parse_control_message(payload);
  } else if (strcmp(item->valuestring, "config") == 0) {
    if (!plant_assigned)
      plant_assigned = true;
    else
      stop_plant_manager(); // Remove old plant manager

    start_plant_manager(payload);
  } else if (strcmp(item->valuestring, "update") == 0) {
    ESP_LOGI(TAG, "Received update command");
    one_time_update();
  } else if (strcmp(item->valuestring, "info") == 0) {
    ESP_LOGI(TAG, "Received info command");
    // Avoid blocking MQTT handler
    xTaskCreate(send_agrosmart_info, "send_agrosmart_info", 4096, event->client,
                5, NULL);
  } else if (strcmp(item->valuestring, "reboot") == 0) {
    ESP_LOGI(TAG, "Received reboot command");
    esp_restart();
  } else {
    ESP_LOGW(TAG, "Received unknown command");
  }

  cJSON_Delete(payload);
}

void mqtt_connection_handler(void* handler_args, esp_event_base_t base,
                             int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  static int error_cnt = 0;

  switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
      error_cnt = 0;
      ESP_LOGI(TAG, "MQTT connected");
      set_status_led_color(ARGB_GREEN);
      extern uint8_t mac[6];
      listen_for_changes(client, mac);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      set_status_led_color(ARGB_RED);
      break;
    case MQTT_EVENT_SUBSCRIBED:
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      error_cnt = 0;
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG, "MQTT_EVENT_DATA");
      ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
      ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
      if (strncmp(event->topic, setting_topic, event->topic_len) == 0) {
        handle_setting_update(event);
      } else {
        ESP_LOGW(TAG, "Unknown topic");
      }
      error_cnt = 0;
      break;
    case MQTT_EVENT_ERROR:
      if (error_cnt++ > 10) {
        ESP_LOGE(TAG, "Too many MQTT errors, restarting...");
        esp_restart();
      }
      ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
      ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
      break;
    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}