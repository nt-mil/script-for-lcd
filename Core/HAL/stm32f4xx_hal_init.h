#ifndef __STM32F4xx_HAL_INIT_H
#define __STM32F4xx_HAL_INIT_H

/* Includes ------------------------------------------------------------------*/
#include "main.h"

void HAL_Initialization(void);

void GPIO_Init(void);
void USART2_UART_Init(void);
void SPI2_UART_Init(void);

#endif /* __STM32F4xx_HAL_INIT_H */