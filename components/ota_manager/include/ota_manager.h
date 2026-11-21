//
// Created by Andrea Canale on 28/06/2026.
//

#ifndef AGROSMART_OTA_MANAGER_H
#define AGROSMART_OTA_MANAGER_H

/**
 * @brief Enable automatic over-the-air (OTA) firmware updates.
 *
 * Creates a dedicated FreeRTOS task
 * that periodically checks for new firmware versions.
 * If there is a new firmware, it will be downloaded, installed
 * and the ESP will be rebooted in order to complete the update
 *
 * @note  The update check interval is controlled by the Kconfig option
 *        CONFIG_OTA_UPDATE_CHECK_INTERVAL (in seconds).
 */
void enable_auto_update();

/**
 * @brief Perform a single one-shot OTA firmware update check.
 *
 * Checks for a new firmware version and, if an update is available, downloads and
 * flashes it immediately. This function does **not** spawn a persistent
 * background task — it runs once and returns in case of failure otherwise
 * the ESP will be rebooted
 *
 */
void one_time_update();

#endif //AGROSMART_OTA_MANAGER_H