//
// Created by Andrea Canale on 30/03/2026.
//

#ifndef PAIRING_HANDLER_H
#define PAIRING_HANDLER_H
#include <esp_event_base.h>

/**
 * @brief Handles MQTT client events.
 * It manages the plant details updates and communication with API
 *
 * @note The parameters are defined by esp_event
 */
void mqtt_connection_handler(
    void* handler_args, esp_event_base_t base,
    int32_t event_id, void* event_data);

#endif //PAIRING_HANDLER_H
