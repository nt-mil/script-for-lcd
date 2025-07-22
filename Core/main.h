/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_init.h"
#include "macro.h"

/// FreeRTOS related header files
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "event_groups.h"

// driver headers
#include "ili9341.h"

// middleware headers
#include "mid_display.h"
#include "databank.h"


void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
