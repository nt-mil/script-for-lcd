#ifndef __STM32F4xx_HAL_INIT_H
#define __STM32F4xx_HAL_INIT_H

/* Includes ------------------------------------------------------------------*/

#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define LCD_BACKLIGHT_PIN GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB

/* lcd pins */
#define LCD_DC_PIN GPIO_PIN_2
#define LCD_RESET_PIN GPIO_PIN_1
#define LCD_CS_PIN GPIO_PIN_15
#define LCD_GPIO_PORT GPIOB

void Init_Peripherals(void);

void GPIO_Init(void);
void DMA_Init(void);
void UART2_Init(void);
void SPI2_Init(void);

#endif /* __STM32F4xx_HAL_INIT_H */