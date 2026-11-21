//
// Created by Andrea Canale on 30/05/2026.
//

#include "cJSON.h"
#include <stdbool.h>

#include "esp_log.h"

#define TAG "json_utility"

/**
 * @brief Parses a required integer field from a cJSON object.
 *
 * @param root Root JSON object.
 * @param key Field name.
 * @param out_value Output integer value.
 * @return true if the field exists and is numeric, false otherwise.
 */
bool parse_int_field(const cJSON* root, const char* key, int* out_value) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);

  // Required int field
  if (!cJSON_IsNumber(item)) {
    ESP_LOGE(TAG, "Field '%s' missing or not numeric", key);
    return false;
  }

  *out_value = item->valueint;

  return true;
}

/**
 * @brief Parses a required boolean field from a cJSON object.
 *
 * @param root Root JSON object.
 * @param key Field name.
 * @param out_value Output boolean value.
 * @return true if the field exists and is boolean, false otherwise.
 */
bool parse_required_bool_field(const cJSON* root, const char* key,
                               bool* out_value) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsBool(item)) {
    ESP_LOGE(TAG, "Field '%s' missing or not boolean", key);
    return false;
  }

  *out_value = cJSON_IsTrue(item);
  return true;
}

/**
 * @brief Parses an optional boolean field from a cJSON object.
 *
 * @param root Root JSON object.
 * @param key Field name.
 * @param default_value Value used when the field is missing or null.
 * @param out_value Output boolean value.
 * @return true if the field is missing, null or boolean, false otherwise.
 */
bool parse_optional_bool_field(const cJSON* root, const char* key,
                               const bool default_value, bool* out_value) {
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (item == NULL || cJSON_IsNull(item)) {
    ESP_LOGW(TAG, "Field '%s' missing. Using default value: %d", key,
             default_value);
    *out_value = default_value;
    return true;
  }

  if (!cJSON_IsBool(item)) {
    ESP_LOGE(TAG, "Field '%s' not boolean", key);
    return false;
  }

  *out_value = cJSON_IsTrue(item);
  return true;
}