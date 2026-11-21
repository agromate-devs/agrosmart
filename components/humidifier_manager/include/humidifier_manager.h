//
// Created by Andrea Canale on 20/10/2026.
//

#ifndef AGROSMART_HUMIDIFIER_MANAGER_H
#define AGROSMART_HUMIDIFIER_MANAGER_H

/**
 * @brief Initialize the humidifier control GPIO.
 *
 * It must be called before starting humidity_checker_task.
 */
void init_humidifier();

/*
 * @brief Run the humidifier for a specified duration in minutes.
 * @param param is a void pointer to the duration
 * @note This function is intended to be run as a FreeRTOS task.
 */
void remote_humidifier_task(void* param);

/**
 * @brief FreeRTOS task that manages the humidifier.
 *
 * If time-based humidification is enabled, the humidifier runs for the
 * configured humidification duration, then the task suspends for the
 * configured humidification frequency interval before repeating. The last run
 * is stored in NVS, so a restart doesn't trigger an early humidification.
 *
 * Otherwise the humidity is checked every CONFIG_HUMIDIFIER_CHECK_INTERVAL_MS
 * and the humidifier runs until the humidity is above the target humidity
 * (considering CONFIG_HUMIDITY_THRESHOLD_RANGE). The threshold is ignored
 * while remote control is active.
 *
 * @param[in] pvParameters Pointer to a @c plant_config_t structure containing
 *                         the target humidity and the time-based
 *                         humidification settings.
 */
void humidity_checker_task(void* pvParameters);

/**
 * @brief Turns off the humidifier if humidity_checker_task was running it.
 *
 * Must be called after deleting humidity_checker_task, so the humidifier isn't
 * left on when the task is deleted while running it. A remote humidification
 * isn't stopped.
 */
void stop_humidifier(void);

#endif  //AGROSMART_HUMIDIFIER_MANAGER_H
