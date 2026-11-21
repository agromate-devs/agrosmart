//
// Created by Andrea Canale on 29/06/2026.
//

#ifndef AGROSMART_WATER_MANAGER_H
#define AGROSMART_WATER_MANAGER_H

#include <time.h>

void init_water_pump(void);

/**
 * @brief FreeRTOS task that manages automated plant watering.
 *
 * This task implements two irrigation strategies based on the plant configuration
 * passed via parameters:
 * - **Time-based irrigation**: The pump runs for a duration proportional to the
 *   required water quantity, then the task suspends for the configured watering
 *   frequency interval before repeating.
 * - **Soil moisture-based irrigation**: Periodically reads the soil moisture sensor.
 *   When the measured moisture drops below the configured threshold, the pump is
 *   activated for the calculated running time. The check interval is defined by
 *   CONFIG_SOIL_MOISTURE_CHECK_INTERVAL_MS.
 *
 *
 * @param[in] pvParameters Pointer to a @c plant_config_t structure containing
 *                         the plant's watering parameters (water quantity,
 *                         soil moisture threshold, and
 *                         irrigation mode selection).
 *
 * @note In case of time-based irrigation, watering frequency must be defined
 */
void watering_task(void* pvParameters);

void remote_watering_task(void *pvParameters);

/**
 * @brief Computes how long the pump must run to deliver a water quantity.
 *
 * @param water_quantity Water quantity in ml
 * @return Running time in seconds, based on CONFIG_WATER_PUMP_FLOW_RATE
 */
int compute_running_time(int water_quantity);

/**
 * @brief Returns the timestamp of the last irrigation.
 *
 * @return Epoch time (seconds) of the last pump run, 0 if never run
 * @note Value is kept in RAM, NVS is not read
 */
time_t get_last_irrigation_time(void);

/**
 * @brief Turns off the pump and resets the status LED if watering_task was
 *        running the pump.
 *
 * Must be called after deleting watering_task, so the pump isn't left on when
 * the task is deleted while running it. A remote irrigation isn't stopped.
 */
void stop_water_pump(void);

#endif //AGROSMART_WATER_MANAGER_H
