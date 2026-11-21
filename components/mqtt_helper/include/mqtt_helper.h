#ifndef _MQTT_HELPER_H
#define _MQTT_HELPER_H

#define EVENT_TYPE_METRIC "metric"
#define EVENT_TYPE_IRRIGATION "irrigation"
#define EVENT_TYPE_HUMIDIFICATION "humidification"
#define METRICS_TOPIC "agrosmart/metrics"

/**
 * @brief FreeRTOS task that waits for Wi-Fi and starts MQTT.
 *
 * @param arg (unused).
 */
void mqtt_task(void* arg);

/**
 * @brief Publishes a payload to an MQTT topic.
 *
 * @param topic Destination topic.
 * @param payload Message payload.
 * @param retain MQTT retain flag (0 or 1).
 */
void publish_mqtt_message(char* topic, char* payload, int retain);

/**
 * @brief Publishes an action event to METRICS_TOPIC.
 *
 * Payload: {"id": "<MAC>", "type": "<type>", "success": true}
 *
 * @param type Event type (e.g. EVENT_TYPE_IRRIGATION).
 */
void publish_action_event(const char* type);

#endif
