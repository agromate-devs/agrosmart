#include <esp_mac.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "keystore.h"
#include "mqtt_client.h"
#include "mqtt_connection_handler.h"
#include "wifi.h"

#define TAG "MQTT Helper"
#define METRICS_TOPIC "agrosmart/metrics"

static esp_mqtt_client_handle_t client;

/**
 * @brief Configures and starts the MQTT client.
 *
 * Loads broker credentials from NVS, builds client identity from the device
 * MAC address, registers the MQTT event handler and starts the client.
 */
void mqtt_app_start(void) {
  char* mqtt_url = malloc(50 * sizeof(char));
  if (mqtt_url == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for mqtt_url");
    return;
  }
  if (!read_key_str("mqtt_url", 50, &mqtt_url)) {
    free(mqtt_url);
    ESP_LOGE(TAG, "Failed to read mqtt_url from NVS");
    return;
  }
  const size_t real_len = strlen(mqtt_url);
  mqtt_url[real_len] = '\0';

  char* esp_password = malloc(17 * sizeof(char));
  if (esp_password == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for esp_password");
    free(mqtt_url);
    return;
  }

  if (!read_key_str("esp_password", 17, &esp_password)) {
    ESP_LOGE(TAG, "Failed to read esp_password from NVS");
    free(mqtt_url);
    free(esp_password);
    return;
  }
  char mqtt_username[28];
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  sprintf(mqtt_username, "agrosmart_%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
          mac[2], mac[3], mac[4], mac[5]);

  ESP_LOGI(TAG, "MQTT URL: %s", mqtt_url);
  ESP_LOGI(TAG, "MQTT Username: %s", mqtt_username);
  ESP_LOGI(TAG, "MQTT Password: %s", esp_password);

  const esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = mqtt_url,
      .credentials.client_id = mqtt_username,
      .credentials.username = mqtt_username,
      .credentials.authentication.password = esp_password,
#ifdef CONFIG_MQTT_INSECURE
      .broker.verification = {
          .skip_cert_common_name_check = true,
      }
#endif
  };

  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                 mqtt_connection_handler, NULL);
  esp_mqtt_client_start(client);

  free(mqtt_url);
  free(esp_password);
}

/**
 * @brief Task entry point that starts MQTT after Wi-Fi is connected.
 *
 * @param arg (unused).
 */
void mqtt_task(void* arg) {
  ESP_LOGI(TAG, "Waiting for wifi connection...");
  if (xSemaphoreTake(wifi_connected_semaphore, portMAX_DELAY) == pdTRUE) {
    mqtt_app_start();
  }
  vTaskDelete(NULL);
}

/**
 * @brief Publishes an MQTT message
 *
 * @param topic Destination topic.
 * @param payload UTF-8 payload string.
 * @param retain MQTT retain flag.
 */
void publish_mqtt_message(char* topic, char* payload, int retain) {
  if (client == NULL) {
    ESP_LOGE(TAG, "MQTT client is not initialized");
    return;
  }

  if (topic == NULL || payload == NULL) {
    ESP_LOGE(TAG, "Topic or payload is NULL");
    return;
  }

  int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, retain);
  if (msg_id == -1) {
    ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic);
    return;
  }

  ESP_LOGI(TAG, "Message published to topic: %s (msg_id: %d)", topic, msg_id);
}

void publish_action_event(const char* type) {
  extern uint8_t mac[6];

  if (type == NULL) {
    ESP_LOGE(TAG, "Event type is NULL");
    return;
  }

  cJSON* json = cJSON_CreateObject();
  if (json == NULL) {
    ESP_LOGE(TAG, "Failed to create event JSON");
    return;
  }

  char mac_str[13];
  snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);

  cJSON_AddStringToObject(json, "id", mac_str);  // Necessary for Telegraf
  cJSON_AddStringToObject(json, "type", type);
  cJSON_AddBoolToObject(json, "success", true);

  char* json_string = cJSON_PrintUnformatted(json);
  cJSON_Delete(json);
  if (json_string == NULL) {
    ESP_LOGE(TAG, "Failed to print event JSON");
    return;
  }

  publish_mqtt_message(METRICS_TOPIC, json_string, 0);
  free(json_string);
}
