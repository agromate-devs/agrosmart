#ifndef _WIFI_H
#define _WIFI_H

#include "esp_event.h"
#include "freertos/event_groups.h"

#include "freertos/semphr.h"

typedef struct wifi_credentials {
  char* ssid;
  char* password;
} WiFiCredentials;

extern SemaphoreHandle_t wifi_connected_semaphore;

/**
 * @brief Initializes Wi-Fi station mode and connects using provided credentials.
 *
 * @param ssid Wi-Fi SSID.
 * @param password Wi-Fi password.
 */
void wifi_init_sta(const char* ssid, const char* password);

/**
 * @brief Loads Wi-Fi credentials from persistent storage.
 *
 * @return Pointer to allocated credentials, or NULL if missing/invalid.
 */
WiFiCredentials* get_wifi_credentials();

/**
 * @brief Initializes the semaphore used to signal Wi-Fi connectivity.
 */
void initialise_wifi_semaphore();

#endif