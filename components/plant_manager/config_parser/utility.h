//
// Created by Andrea Canale on 30/05/2026.
//

#ifndef AGROSMART_UTILITY_H
#define AGROSMART_UTILITY_H

#include <stdbool.h>

bool parse_int_field(const cJSON* root, const char* key, int* out_value);
bool parse_required_bool_field(const cJSON* root, const char* key,
                               bool* out_value);
bool parse_optional_bool_field(const cJSON* root, const char* key,
                               bool default_value, bool* out_value);

#endif //AGROSMART_UTILITY_H
