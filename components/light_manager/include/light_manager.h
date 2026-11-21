//
// Created by Andrea Canale on 30/04/2026.
//

#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H
#include <stdint.h>

#define ARGB_RED 0xFFFF0000
#define ARGB_GREEN 0xFF00FF00
#define ARGB_BLUE 0xFF0000FF
#define ARGB_YELLOW 0xFFFFFF00
#define ARGB_VIOLET 0xFF8A2BE2

typedef struct {
  char* time_range;
  uint32_t light_color;
} light_task_arg_t;

void init_led_strip(void);
void init_status_led(void);
void set_status_led_color(uint32_t argb32);
void set_led_strip_light_color(uint32_t argb32);

void light_timer_task(void* arg);

#endif  //LIGHT_MANAGER_H
