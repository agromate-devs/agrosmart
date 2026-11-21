#include <inttypes.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs.h"

#define STORAGE "storage"
#define TAG "keystore"

static esp_err_t open_storage(nvs_open_mode_t mode, nvs_handle_t *handle) {
  const esp_err_t res = nvs_open(STORAGE, mode, handle);
  if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(res));
  }
  return res;
}

bool read_key_str(const char *key, size_t length, char **out) {
  if (key == NULL) {
    return NULL;
  }

  nvs_handle_t my_handle;
  const esp_err_t res = open_storage(NVS_READONLY, &my_handle);
  if (res != ESP_OK) {
    return NULL;
  }

  const uint16_t err = nvs_get_str(my_handle, key, *out, &length);
  nvs_close(my_handle);  // The value is already read here so we can close NVS

  switch (err) {
    case ESP_OK:
      ESP_LOGI(TAG, "Value %s", *out);
      return true;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGW(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGI(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
  }

  return false;
}

/**
 * @brief Reads a string value from NVS storage by key.
 *
 * Opens the NVS storage in read/write mode and attempts to read the string value
 * associated with the given key. If the key is found, returns a pointer to the allocated buffer containing
 * the value (caller must free). If the key is not found or an error occurs, returns NULL.
 *
 * @param key     The key to look up in NVS.
 * @param out     Pointer to an int64_t variable where the read value will be stored.
 * @return        True if read operation finished successfully, false otherwise
 */
bool read_key_int64(char* key, int64_t *out) {
  if (key == NULL || out == NULL) {
    return false;
  }

  nvs_handle_t my_handle;
  const esp_err_t res = open_storage(NVS_READONLY, &my_handle);
  if (res != ESP_OK) {
    return false;
  }

  const uint16_t err = nvs_get_i64(my_handle, key, out);
  nvs_close(my_handle);  // The value is already read here so we can close NVS

  switch (err) {
    case ESP_OK:
      ESP_LOGI(TAG, "Value %d", *out);
      return true;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGW(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGI(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
  }

  return false;
}

/**
 * @brief Writes a string value to NVS storage under the given key.
 *
 * Opens the NVS storage in read/write mode and writes the provided string value
 * under the specified key. Commits the change to ensure it is saved.
 *
 * @param key     The key under which to store the value.
 * @param value   The string value to store.
 */
void write_key_str(char* key, char* value) {
  nvs_handle_t my_handle;
  ESP_ERROR_CHECK(open_storage(NVS_READWRITE, &my_handle));
  ESP_LOGD(TAG, "Write %s = %s to NVS", key, value);
  ESP_ERROR_CHECK(nvs_set_str(my_handle, key, value));

  ESP_ERROR_CHECK(nvs_commit(my_handle));
  nvs_close(my_handle);
}

/**
 * @brief Writes an integer value to NVS storage under the given key.
 *
 * Opens the NVS storage in read/write mode and writes the provided integer value
 * under the specified key. Commits the change to ensure it is saved.
 *
 * @param key     The key under which to store the value.
 * @param value   The integer value to store.
 */
void write_key_int64(char* key, const int64_t value) {
  nvs_handle_t my_handle;
  ESP_ERROR_CHECK(open_storage(NVS_READWRITE, &my_handle));
  ESP_LOGD(TAG, "Write %s = %d to NVS", key, value);
  ESP_ERROR_CHECK(nvs_set_i64(my_handle, key, value));

  ESP_ERROR_CHECK(nvs_commit(my_handle));
  nvs_close(my_handle);
}

/**
 * @brief Removes a key and its value from NVS storage.
 *
 * Opens the NVS storage in read/write mode and erases the entry associated with
 * the given key.
 *
 * @param key     The key to erase from NVS.
 */
void clear_key(char* key) {
  nvs_handle_t my_handle;
  ESP_ERROR_CHECK(open_storage(NVS_READWRITE, &my_handle));
  nvs_erase_key(my_handle, key);
  nvs_close(my_handle);
}