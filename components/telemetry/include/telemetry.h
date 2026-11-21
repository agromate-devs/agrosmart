#ifndef _TELEMETRY_H
#define _TELEMETRY_H

/**
 * @brief Sends a telemetry POST request to the server.
 *
 * Extracts the broker IP from the MQTT URL stored in NVS,
 * then sends an HTTP POST to http://<broker_ip>:<CONFIG_TELEMETRY_PORT>/telemetry.
 *
 * Headers:
 *   - Content-Type: application/x-www-form-urlencoded
 *   - macaddress: <device MAC address>
 *
 * Body (x-www-form-urlencoded):
 *   message=<message>
 *
 * @param message Null-terminated string to send as the "message" field.
 */
void telemetry_send(const char* message);

/**
 * @brief Builds and returns a JSON string with Agrosmart device info.
 *
 * The returned JSON contains:
 *   - compile_time, compile_date, firmware_hash (SHA256), idf_version
 *   - free_heap_bytes, free_internal_heap_bytes
 *   - cpu_freq_mhz
 *   - wifi_ssid, wifi_rssi, wifi_ip
 *   - flash_encryption (bool), secure_boot (bool)
 *
 * @return A dynamically allocated JSON string (caller must free it),
 *         or NULL on error.
 */
const char* get_agrosmart_info(void);

#endif
