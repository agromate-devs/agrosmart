
#ifndef KEYSTORE_H
#define KEYSTORE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Stores a null-terminated string value in NVS.
 *
 * @param key Key associated to value.
 * @param value String value to store.
 */
void write_key_str(char *key, char *value);

/**
 * @brief Stores an integer value in NVS.
 *
 * @param key Key associated to value.
 * @param value Integer value to store.
 */
void write_key_int64(char* key, int64_t value);

/**
 * @brief Reads a string value from NVS.
 *
 * @param key Key associated to value.
 * @param length Buffer length to allocate for the returned string.
 * @param[out] out Receives the allocated string (caller must free it).
 * @return true if the value was read successfully, false otherwise.
 */
bool read_key_str(const char *key, size_t length, char **out);

/**
 * @brief Reads an integer value from NVS.
 *
 * @param key Key associated to value.
 * @param[out] out Receives the integer value.
 * @return true if the value was read successfully, false otherwise.
 */
bool read_key_int64(char* key, int64_t *out);

/**
 * @brief Removes a key from NVS.
 *
 * @param key Key associated to value to erase.
 */
void clear_key(char *key);


#endif