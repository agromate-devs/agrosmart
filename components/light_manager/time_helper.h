#ifndef _TIME_HELPER_H
#define _TIME_HELPER_H
#include <stdbool.h>
#include "time.h"

typedef struct {
  // time_t start;
  // time_t end;
  // TODO: Use uint8_t
  int start_h;
  int start_m;
  int end_h;
  int end_m;
} light_timer_t;

light_timer_t* parse_mqtt_time(const char* input_time);

/**
 * @brief Calculates the difference between two times expressed as hour/minute pairs.
 *
 * This function compute the number
 * of minutes between two time(given in h:m format).
 *
 * @param h1 First hour value.
 * @param h2 Second hour value.
 * @param m1 First minute value.
 * @param m2 Second minute value.
 * @param normalize_24 Normalize the value in a 24h format
 * @return The computed time difference between h2:m2 and h1:m1 in minutes.
 * @note The difference is computed based on 24h time order in case of normalize_24
 * (for example 07:00 - 23:00 gives a positive value
 * because 07:00 is after 23:00 in a 24h range)
 */
int timediff(int h1, int h2, int m1, int m2, bool normalize_24);

/**
 * @brief Updates the current time into the provided time_t and struct tm pointers.
 *
 * @param[out] now Pointer to a time_t to receive the current calendar time.
 * @param[out] timeinfo Pointer to a struct tm to receive the broken-down local time.
 * @return true on success, false if time(2) fails.
 */
bool update_time(time_t* now, struct tm* timeinfo);

/**
 * @brief Returns the number of minutes elapsed since midnight for a given broken-down time.
 *
 * @param tm_info Pointer to a struct tm representing a local time.
 * @return Minutes since midnight (0–1439).
 */
int minutes_since_midnight(const struct tm* tm_info);

/**
 * @brief Normalizes a minute difference within a 24-hour range.
 *
 * Computes (to_min - from_min) and wraps the result into [0, 24*60 - 1].
 * If the difference is negative, 24 hours are added.
 *
 * @param from_min Starting minute-of-day value.
 * @param to_min Ending minute-of-day value.
 * @return The normalized positive difference in minutes.
 */
int normalize_diff_minutes(const int from_min, const int to_min);

/**
 * @brief Checks whether a given minute-of-day falls within a [start_min, end_min) window.
 *
 * Supports overnight windows where start_min > end_min (e.g. 22:00–06:00).
 * If start_min equals end_min, the window is considered empty and the function
 * returns false.
 *
 * @param now_min The current minute-of-day to test.
 * @param start_min The inclusive start minute-of-day of the window.
 * @param end_min The exclusive end minute-of-day of the window.
 * @return true if now_min is inside the window, false otherwise.
 */
bool is_now_in_window(const int now_min, const int start_min,
                      const int end_min);

/**
 * @brief Returns the number of minutes remaining until the end of the current time window.
 *
 * The caller must ensure that now_min lies within [start_min, end_min) (as
 * determined by is_now_in_window()). If it does not, the function returns 0.
 *
 * @param now_min The current minute-of-day.
 * @param start_min The inclusive start minute-of-day of the window.
 * @param end_min The exclusive end minute-of-day of the window.
 * @return Minutes until the window ends, or 0 if not currently in the window.
 */
int minutes_until_end(const int now_min, const int start_min,
                      const int end_min);

#endif
