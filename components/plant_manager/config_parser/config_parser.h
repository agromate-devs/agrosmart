//
// Created by Andrea Canale on 30/05/2026.
//

#ifndef AGROSMART_CONFIG_PARSER_H
#define AGROSMART_CONFIG_PARSER_H

/**
 * @brief Parses the plant configuration JSON into a typed structure.
 *
 * @param plant_json JSON payload.
 * @param len Payload length in bytes.
 * @param out_config Output configuration.
 * @return true on success, false on parsing/validation errors.
 */
bool parse_plant_config_json(cJSON* root,
                             plant_config_t* out_config);

#endif //AGROSMART_CONFIG_PARSER_H
