#include "time_helper.h"

#include <stdio.h>

#include <time.h>
#include "esp_log.h"

#define TAG "time_helper"

light_timer_t* parse_mqtt_time(const char* input_time) {
  int start_h = 0, start_m = 0, end_h = 0, end_m = 0;
  const int parsed =
      sscanf(input_time, "%d:%d-%d:%d", &start_h, &start_m, &end_h, &end_m);
  if (parsed != 4) {
    ESP_LOGE(TAG, "Invalid time range format: %s (expected HH:MM-HH:MM)",
             input_time);
    return NULL;
  }

  if (start_h < 0 || start_h > 23 || start_m < 0 || start_m > 59 || end_h < 0 ||
      end_h > 23 || end_m < 0 || end_m > 59) {
    ESP_LOGE(TAG, "Invalid time values in range: %s", input_time);
    return NULL;
  }

  light_timer_t* light_timer = malloc(sizeof(light_timer_t));
  if (light_timer == NULL) {
    ESP_LOGE(TAG, "Unable to allocate memory for light_timer_t");
    return NULL;
  }
  light_timer->start_h = start_h;
  light_timer->start_m = start_m;
  light_timer->end_h = end_h;
  light_timer->end_m = end_m;

  return light_timer;
}

int timediff(int h1, int h2, int m1, int m2, bool normalize_24) {
  const int min1 = h1 * 60 + m1;
  int min2 = h2 * 60 + m2;

  if (min2 < min1 && normalize_24)
    min2 += 24 * 60;

  return min2 - min1;
}

// Update current time
bool update_time(time_t* now, struct tm* timeinfo) {
  if (time(now) == -1) {
    ESP_LOGE(TAG, "Failed to get current time");
    return false;
  }
  localtime_r(now, timeinfo);
  return true;
}

int minutes_since_midnight(const struct tm* tm_info) {
  return tm_info->tm_hour * 60 + tm_info->tm_min;
}

int normalize_diff_minutes(const int from_min, const int to_min) {
  int diff = to_min - from_min;
  if (diff < 0) {
    diff += 24 * 60;
  }
  return diff;
}

bool is_now_in_window(const int now_min, const int start_min,
                      const int end_min) {
  if (start_min == end_min) {
    return false;
  }

  if (start_min < end_min) {
    return now_min >= start_min && now_min < end_min;
  }

  // Overnight window (e.g. 22:00-06:00)
  return now_min >= start_min || now_min < end_min;
}

int minutes_until_end(const int now_min, const int start_min,
                      const int end_min) {
  if (!is_now_in_window(now_min, start_min, end_min)) {
    return 0;
  }

  if (start_min < end_min) {
    return end_min - now_min;
  }

  if (now_min >= start_min) {
    return (24 * 60 - now_min) + end_min;
  }

  return end_min - now_min;
}
