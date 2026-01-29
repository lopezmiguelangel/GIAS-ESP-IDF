#ifndef GIAS_H
#define GIAS_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_mmc.h"

/**
 * @brief Main GIAS function to initialize and run the system.
 */
void gias(void);

#endif // GIAS_H
