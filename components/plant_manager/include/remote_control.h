/**
 * @file remote_control.h
 * @brief Task sizes and shared struct.
 *
 * This header provides the interfaces for controlling the humidifier and water pump
 *
 * @author Andrea Canale
 * @date 21/10/2026
 */

#ifndef AGROSMART_REMOTE_CONTROL_H
#define AGROSMART_REMOTE_CONTROL_H

#define REMOTE_CONTROL_TASK_STACK_SIZE 2048
// Remote watering sends telemetry over HTTP, so it needs a bigger stack
#define REMOTE_WATERING_TASK_STACK_SIZE 4096

typedef struct {
  int duration;
  bool release_sem;
} RemoteControlTaskParam;

#endif //AGROSMART_REMOTE_CONTROL_H
