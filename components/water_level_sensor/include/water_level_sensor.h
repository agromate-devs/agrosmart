//
// Created by Andrea Canale on 29/10/2026.
//

#ifndef AGROSMART_WATER_LEVEL_SENSOR_H
#define AGROSMART_WATER_LEVEL_SENSOR_H
#include <stdbool.h>

/**
 * @brief Initialize the XKC-Y23A-NPN water level sensor GPIO.
 *
 * The sensor has an NPN open collector output, so the GPIO is configured as
 * input with the internal pull-up enabled.
 * It must be called before performing any readings from the sensor.
 */
void init_water_level_sensor(void);

/**
 * @brief Check if the water tank is in reserve.
 *
 * @return true if the water is below the sensor level, false otherwise.
 */
bool is_water_in_reserve(void);

/**
 * @brief Function run when the water tank exits the reserve.
 *
 * @param arg Argument given to arm_water_refill_interrupt()
 * @return Ignored
 */
typedef bool (*water_refill_callback_t)(int arg);

/**
 * @brief Arm a one shot interrupt that triggers when the sensor detects water
 *        again, i.e. the tank exits the reserve.
 *
 * Must be called only when the tank is in reserve. When the interrupt
 * triggers, it's disabled and the callback runs once in a dedicated task, so
 * it can block. Arming it again before it triggers replaces the callback.
 *
 * @warning Not protected by a lock: if it's called concurrently by different
 *          tasks, the callback of one call can run with the arg of the other.
 *          It's safe as long as every caller passes the same callback.
 *
 * @param callback Function to run when the tank is refilled
 * @param arg Argument passed to the callback
 */
void arm_water_refill_interrupt(water_refill_callback_t callback, int arg);

/**
 * @brief Block while the water refill interrupt is armed, until the tank is
 *        refilled and the callback has completed.
 *
 * Returns immediately if the interrupt isn't armed.
 *
 * @return true if the callback was running and the caller had to wait,
 *         false otherwise
 *
 * @warning Must not be called by the callback, otherwise it blocks forever.
 */
bool wait_water_refill_completion(void);

#endif  //AGROSMART_WATER_LEVEL_SENSOR_H
