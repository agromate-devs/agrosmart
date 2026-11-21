#ifndef _SMARTCONFIG_H
#define _SMARTCONFIG_H

#include <stdbool.h>

/**
 * @brief Starts SmartConfig provisioning flow.
 *
 * @param reset If true, deinitializes Wi-Fi before starting SmartConfig. Should be set to 1 if Wi-Fi is already enabled
 */
void initialise_smartconfig(bool reset);

#endif