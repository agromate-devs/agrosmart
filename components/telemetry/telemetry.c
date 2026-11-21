#include "telemetry.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_http_client.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
// #include "esp_clk.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "driver/temperature_sensor.h"
#include "keystore.h"
#include "esp_private/esp_clk.h"

#define TAG "telemetry"

#define TELEMETRY_PATH "/telemetry"

/**
 * @brief Extracts the broker IP from the MQTT URL stored in NVS.
 *
 * The MQTT URL is expected to be in the format "mqtt://<ip>" or "mqtt://<ip>:<port>".
 *
 * @param[out] ip_buf Buffer to hold the extracted IP string.
 * @param[in]  buf_size Size of ip_buf.
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t get_broker_ip(char* ip_buf, size_t buf_size) {
  char* mqtt_url = malloc(128 * sizeof(char));
  if (!read_key_str("mqtt_url", 128, &mqtt_url)) {
    ESP_LOGE(TAG, "Failed to read mqtt_url from NVS");
    return ESP_ERR_NOT_FOUND;
  }

  // Expect format: mqtt://<ip> or mqtt://<ip>:<port>
  const char* prefix = "mqtt://";
  char* start = strstr(mqtt_url, prefix);
  if (start == NULL) {
    ESP_LOGE(TAG, "Invalid mqtt_url format: %s", mqtt_url);
    free(mqtt_url);
    return ESP_ERR_INVALID_ARG;
  }
  start += strlen(prefix);

  // Find end of IP (either ':' for port or end of string)
  char* end = strchr(start, ':');
  size_t ip_len;
  if (end != NULL) {
    ip_len = end - start;
  } else {
    ip_len = strlen(start);
  }

  if (ip_len >= buf_size) {
    ESP_LOGE(TAG, "IP address too long for buffer");
    free(mqtt_url);
    return ESP_ERR_INVALID_SIZE;
  }

  memcpy(ip_buf, start, ip_len);
  ip_buf[ip_len] = '\0';

  free(mqtt_url);
  return ESP_OK;
}

/**
 * @brief Builds the full telemetry URL: http://<ip>:<port>/telemetry
 *
 * @param[out] url_buf Buffer to hold the resulting URL.
 * @param[in]  buf_size Size of url_buf.
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t build_telemetry_url(char* url_buf, size_t buf_size) {
  char ip_buf[64];
  esp_err_t err = get_broker_ip(ip_buf, sizeof(ip_buf));
  if (err != ESP_OK) {
    return err;
  }

  int written = snprintf(url_buf, buf_size, "http://%s:%d" TELEMETRY_PATH,
                         ip_buf, CONFIG_TELEMETRY_PORT);
  if (written < 0 || (size_t)written >= buf_size) {
    ESP_LOGE(TAG, "URL buffer too small");
    return ESP_ERR_INVALID_SIZE;
  }

  return ESP_OK;
}

/**
 * @brief Builds the x-www-form-urlencoded body: message=<message>
 *
 * @param[out] body_buf Buffer to hold the resulting body.
 * @param[in]  buf_size Size of body_buf.
 * @param[in]  message  The message to URL-encode and include.
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t build_body(char* body_buf, size_t buf_size,
                            const char* message) {
  if (message == NULL) {
    ESP_LOGE(TAG, "message is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  // Simple approach: prepend "message=" to the message.
  // For production, consider URL-encoding special characters.
  int written = snprintf(body_buf, buf_size, "message=%s", message);
  if (written < 0 || (size_t)written >= buf_size) {
    ESP_LOGE(TAG, "Body buffer too small");
    return ESP_ERR_INVALID_SIZE;
  }

  return ESP_OK;
}

/**
 * @brief Builds the MAC address string from the Wi-Fi STA MAC.
 *
 * @param[out] mac_buf Buffer to hold the MAC address string (e.g. "AABBCCDDEEFF").
 * @param[in]  buf_size Size of mac_buf (should be at least 13).
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t get_mac_address_string(char* mac_buf, size_t buf_size) {
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read MAC address");
    return err;
  }

  int written = snprintf(mac_buf, buf_size,
                         "%02X%02X%02X%02X%02X%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  if (written < 0 || (size_t)written >= buf_size) {
    ESP_LOGE(TAG, "MAC string buffer too small");
    return ESP_ERR_INVALID_SIZE;
  }

  return ESP_OK;
}

/**
 * @brief HTTP event handler for the telemetry request.
 *
 * Logs the response status and cleans up the HTTP client on completion.
 */
static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
               evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (evt->data_len > 0) {
        ESP_LOGI(TAG, "Response: %.*s", evt->data_len, (char*)evt->data);
      }
      break;
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
    case HTTP_EVENT_REDIRECT:
      ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
      break;
    default:
      break;
  }
  return ESP_OK;
}

void telemetry_send(const char* message) {
  char url[256];
  esp_err_t err = build_telemetry_url(url, sizeof(url));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to build telemetry URL");
    return;
  }

  char body[512];
  err = build_body(body, sizeof(body), message);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to build request body");
    return;
  }

  char mac_str[13];
  err = get_mac_address_string(mac_str, sizeof(mac_str));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get MAC address");
    return;
  }

  ESP_LOGI(TAG, "Sending telemetry to: %s", url);
  ESP_LOGI(TAG, "MAC address: %s", mac_str);
  ESP_LOGI(TAG, "Body: %s", body);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_POST,
      .event_handler = http_event_handler,
      .timeout_ms = 10000,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);

  // Set Content-Type header
  esp_http_client_set_header(client, "Content-Type",
                             "application/x-www-form-urlencoded");

  // Set macaddress header
  esp_http_client_set_header(client, "macaddress", mac_str);

  // Set the POST body
  esp_http_client_set_post_field(client, body, (int)strlen(body));

  esp_err_t perform_err = esp_http_client_perform(client);
  if (perform_err == ESP_OK) {
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP POST completed, status: %d", status_code);
  } else {
    ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(perform_err));
  }

  esp_http_client_cleanup(client);
}

const char* get_agrosmart_info() {
  const esp_app_desc_t* app_desc = esp_app_get_description();
  if (app_desc == NULL) {
    ESP_LOGE(TAG, "Failed to get app description");
    return NULL;
  }

  cJSON* root = cJSON_CreateObject();
  if (root == NULL) {
    ESP_LOGE(TAG, "Failed to create cJSON root");
    return NULL;
  }

  // --- Compile time and date ---
  cJSON_AddStringToObject(root, "compile_time", app_desc->time);
  cJSON_AddStringToObject(root, "compile_date", app_desc->date);

  // --- Firmware hash (SHA256 of ELF) ---
  char sha256_str[65];
  esp_app_get_elf_sha256(sha256_str, sizeof(sha256_str));
  cJSON_AddStringToObject(root, "firmware_hash", sha256_str);

  // --- IDF version ---
  cJSON_AddStringToObject(root, "idf_version", app_desc->idf_ver);

  // --- RAM: free heap ---
  const uint32_t free_heap = esp_get_free_heap_size();
  cJSON_AddNumberToObject(root, "free_heap_bytes", free_heap);

  // --- RAM: free internal (DRAM) heap ---
  const uint32_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  cJSON_AddNumberToObject(root, "free_internal_heap_bytes", free_internal);

  // --- CPU frequency ---
  const uint32_t cpu_freq_mhz = esp_clk_cpu_freq() / 1000000;
  cJSON_AddNumberToObject(root, "cpu_freq_mhz", cpu_freq_mhz);

  // --- CPU temperature ---
  // temperature_sensor_handle_t temp_sensor = NULL;
  // float cpu_temp_c = -127.0f;
  // temperature_sensor_config_t temp_config =
  //     TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  // esp_err_t temp_err = temperature_sensor_install(&temp_config, &temp_sensor);
  // if (temp_err == ESP_OK) {
  //   temp_err = temperature_sensor_enable(temp_sensor);
  //   if (temp_err == ESP_OK) {
  //     temp_err = temperature_sensor_get_celsius(temp_sensor, &cpu_temp_c);
  //     if (temp_err != ESP_OK) {
  //       ESP_LOGW(TAG, "Failed to read CPU temperature: %s",
  //                esp_err_to_name(temp_err));
  //     }
  //     temperature_sensor_disable(temp_sensor);
  //   } else {
  //     ESP_LOGW(TAG, "Failed to enable temperature sensor: %s",
  //              esp_err_to_name(temp_err));
  //   }
  //   temperature_sensor_uninstall(temp_sensor);
  // } else {
  //   ESP_LOGW(TAG, "Failed to install temperature sensor: %s",
  //            esp_err_to_name(temp_err));
  // }
  // cJSON_AddNumberToObject(root, "cpu_temp_c", cpu_temp_c);

  // --- WiFi info: SSID, RSSI, IP ---
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    cJSON_AddStringToObject(root, "wifi_ssid", (const char*)ap_info.ssid);
    cJSON_AddNumberToObject(root, "wifi_rssi", ap_info.rssi);
  } else {
    cJSON_AddStringToObject(root, "wifi_ssid", "N/A");
    cJSON_AddNumberToObject(root, "wifi_rssi", 0);
  }

  esp_netif_ip_info_t ip_info;
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    cJSON_AddStringToObject(root, "wifi_ip", ip_str);
  } else {
    cJSON_AddStringToObject(root, "wifi_ip", "N/A");
  }

  // --- Flash encryption status ---
  bool flash_enc_enabled = false;
#if CONFIG_SECURE_FLASH_ENC_ENABLED
  flash_enc_enabled = esp_flash_encrypt_state();
#endif
  cJSON_AddBoolToObject(root, "flash_encryption", flash_enc_enabled);

  // --- Secure boot status ---
  const bool secure_boot_enabled = esp_secure_boot_enabled();
  cJSON_AddBoolToObject(root, "secure_boot", secure_boot_enabled);

  char* json_string = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  if (json_string == NULL) {
    ESP_LOGE(TAG, "Failed to print cJSON to string");
    return NULL;
  }

  return json_string;
}