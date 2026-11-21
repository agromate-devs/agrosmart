//
// Created by Andrea Canale on 28/06/2026.
//

#include "include/ota_manager.h"

#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"

#define TAG "ota_manager"

typedef struct {
  esp_http_client_config_t* version_http_config;
  esp_http_client_config_t* agrosmart_http_config;
} ota_update_param_t;

static void free_ota_params(ota_update_param_t* params) {
  if (params == NULL) {
    return;
  }
  if (params->agrosmart_http_config != NULL) {
    free((void*)params->agrosmart_http_config->url);
    free(params->agrosmart_http_config);
  }
  if (params->version_http_config != NULL) {
    free((void*)params->version_http_config->url);
    free(params->version_http_config);
  }
  free(params);
}

static bool check_update(const esp_http_client_config_t* version_http_config) {
  bool update_available = false;  // If something happens don't update
  esp_http_client_handle_t version_client =
      esp_http_client_init(version_http_config);
  if (version_client == NULL) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client for version check.");
    return false;
  }
  const esp_err_t err = esp_http_client_open(version_client, 0);

  if (err == ESP_OK) {
    const int64_t content_length =
        esp_http_client_fetch_headers(version_client);
    // content_length may be -1 for chunked encoding; try to read anyway
    if (content_length > 0 || content_length == -1) {
      char remote_sha256[65] = {0};
      const int read_len = esp_http_client_read(version_client, remote_sha256,
                                                sizeof(remote_sha256) - 1);
      if (read_len > 0) {
        remote_sha256[read_len] = '\0';
        // Trim trailing whitespace and newlines
        char* end = remote_sha256 + strlen(remote_sha256) - 1;
        while (end >= remote_sha256 &&
               (*end == '\n' || *end == '\r' || *end == ' ')) {
          *end = '\0';
          end--;
        }

        // Get the SHA256 of the currently running ELF
        char* local_sha256 = esp_app_get_elf_sha256_str();

        ESP_LOGI(TAG, "Remote SHA256: %s, Local SHA256: %s", remote_sha256,
                 local_sha256);

        if (strcmp(remote_sha256, local_sha256) == 0) {
          ESP_LOGI(TAG, "Firmware is already up to date. Skipping OTA.");
          return false;
        } else {
          update_available = true;
        }
      }
    }
  }

  esp_http_client_close(version_client);
  esp_http_client_cleanup(version_client);

  return update_available;
}

static bool ota_update(ota_update_param_t* param) {
  if (!check_update(param->version_http_config)) {
    return false;
  }

  ESP_LOGI(TAG, "New firmware available. Proceeding with OTA...");

  const esp_https_ota_config_t ota_config = {
      .http_config = param->agrosmart_http_config,
  };

  const esp_err_t ret = esp_https_ota(&ota_config);
  if (ret == ESP_OK) {
    return true;
  }

  ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
  return false;
}

static ota_update_param_t* prepare_ota_params() {
  ESP_LOGI(TAG, "Starting OTA");
  // + agrosmart.bin length
  char* agrosmart_url = malloc(strlen(CONFIG_OTA_UPDATE_URL) + 14);
  // + version.txt length
  char* version_txt_url = malloc(strlen(CONFIG_OTA_UPDATE_URL) + 12);
  if (agrosmart_url == NULL || version_txt_url == NULL) {
    ESP_LOGE(TAG,
             "Failed to allocate memory for OTA URL. Aborting OTA update.");
    free(agrosmart_url);
    free(version_txt_url);
    return NULL;
  }

  strcpy(agrosmart_url, CONFIG_OTA_UPDATE_URL);
  strcpy(version_txt_url, CONFIG_OTA_UPDATE_URL);
  strcat(agrosmart_url, "agrosmart.bin");
  strcat(version_txt_url, "version.txt");

  esp_http_client_config_t* agrosmart_http_config =
      calloc(1, sizeof(esp_http_client_config_t));
  if (agrosmart_http_config == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for OTA HTTP config.");
    free(agrosmart_url);
    free(version_txt_url);
    return NULL;
  }
  agrosmart_http_config->url = agrosmart_url;

#if CONFIG_UNSAFE_OTA_UPDATE
  ESP_LOGW(TAG,
           "OTA over HTTP is enabled. This is insecure and should only be used "
           "for testing.");
  agrosmart_http_config->cert_pem = NULL;
  agrosmart_http_config->skip_cert_common_name_check = true;
#else
  agrosmart_http_config->crt_bundle_attach = esp_crt_bundle_attach;
#endif

  // Fetch version.txt to get the remote ELF SHA256 hash
  esp_http_client_config_t* version_http_config =
      calloc(1, sizeof(esp_http_client_config_t));
  if (version_http_config == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for version HTTP config.");
    free(agrosmart_http_config);
    free(agrosmart_url);
    free(version_txt_url);
    return NULL;
  }

  version_http_config->url = version_txt_url;

#if CONFIG_UNSAFE_OTA_UPDATE
  version_http_config->skip_cert_common_name_check = true;
  version_http_config->cert_pem = NULL;
#else
  version_http_config->crt_bundle_attach = esp_crt_bundle_attach;
#endif

  ota_update_param_t* params = malloc(sizeof(ota_update_param_t));
  if (params == NULL) {
    free(agrosmart_http_config);
    free(agrosmart_url);
    free(version_http_config);
    free(version_txt_url);
    ESP_LOGE(TAG, "Failed to allocate memory for OTA update params.");
    return NULL;
  }

  params->agrosmart_http_config = agrosmart_http_config;
  params->version_http_config = version_http_config;

  return params;
}

static void task_ota_update([[maybe_unused]] void* pvParameter) {
  ota_update_param_t* params = prepare_ota_params();
  if (params == NULL) {
    ESP_LOGE(TAG, "OTA update params is NULL. Aborting OTA update.");
    vTaskDelete(NULL);
  }

  while (1) {
    ESP_LOGI(TAG, "Checking for OTA update...");
    ESP_LOGI(TAG, "Available heap: %d", esp_get_free_heap_size());
    if (!ota_update(params)) {
      vTaskDelay(pdMS_TO_TICKS(CONFIG_OTA_UPDATE_CHECK_INTERVAL * 1000));
    } else {
      ESP_LOGI(TAG, "OTA update successful. Rebooting...");
      esp_restart();
    }
  }
}

void enable_auto_update() {
  ESP_LOGI(TAG, "Enabling OTA automatic update");
  xTaskCreate(task_ota_update, "ota_task", 8192, NULL, 5, NULL);
}

void one_time_update() {
  ota_update_param_t* params = prepare_ota_params();
  if (params == NULL) {
    ESP_LOGE(TAG, "OTA update params is NULL. Aborting OTA update.");
    return;
  }

  ota_update(params);
  free_ota_params(params);
}