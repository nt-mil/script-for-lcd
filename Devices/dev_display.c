// dev_display.c
#include "macro.h"
#include "dev_display.h"
// #include "spi_dma_driver.h"
#include "stm32f4xx_hal.h"

#define DMA_MAX_COUNTS (10)
extern SPI_HandleTypeDef hspi1;

typedef struct
{
    u16 current_row;
    u16 wait_count;
    u8 is_row_completed;
} DMA_LCD_CTRL;

static DMA_LCD_CTRL dma_lcd_ctrl;
static LCD_INFO lcd_info;

static void init_lcd(void)
{
    switch (lcd_info.initial_state)
    {
        case STATE_RESET_PORT:
            // toggle reset port
            lcd_info.initial_state = STATE_SENDING_COMMAND;
            break;

        case STATE_SENDING_COMMAND:
            // send init commands
            lcd_info.initial_state = STATE_COMPLETED;
            break;

        case STATE_COMPLETED:
            lcd_info.current_state = LCD_RUNNING;
            break;

        default:
            break;
    }
}

static void init_dma_state(void)
{
    dma_lcd_ctrl.current_row = 0;
    dma_lcd_ctrl.is_row_completed = FALSE;
    dma_lcd_ctrl.wait_count = 0;
}

static void dma_send_next_row(void)
{
    if (dma_lcd_ctrl.current_row >= LCD_HEIGHT)
    {
        init_dma_state();
    }
    else
    {
        u8* ptr = &lcd_info.screen_buffer[dma_lcd_ctrl.current_row * LCD_WIDTH * BIT_PER_PIXEL / 8];
        HAL_SPI_Transmit_DMA(&hspi1, ptr, LCD_WIDTH * BIT_PER_PIXEL / 8);
        dma_lcd_ctrl.is_row_completed = FALSE;
        dma_lcd_ctrl.current_row++;
    }
}

static void dma_send_first_row(void)
{
    u8* ptr = &lcd_info.screen_buffer[0];
    HAL_SPI_Transmit_DMA(&hspi1, ptr, LCD_WIDTH * BIT_PER_PIXEL / 8);
    dma_lcd_ctrl.is_row_completed = FALSE;
    dma_lcd_ctrl.current_row = 1;
}

static void wait_for_dma_completion(void)
{
    if (dma_lcd_ctrl.wait_count >= DMA_MAX_COUNTS)
    {
        // timeout case
        init_dma_state();
    }
    else
    {
        dma_lcd_ctrl.wait_count++;
    }
}

static void update_lcd_state(void)
{
    if ((lcd_info.current_state == LCD_IDLE) && (lcd_info.target_state == LCD_READY))
    {
        lcd_info.current_state = LCD_READY;
    }
    else if ((lcd_info.current_state == LCD_RUNNING) && (lcd_info.target_state == LCD_IDLE))
    {
        lcd_info.current_state = LCD_IDLE;
    }

    switch (lcd_info.current_state)
    {
        case LCD_READY:
            lcd_info.current_state = LCD_INITIALIZING;
            lcd_info.initial_state = STATE_RESET_PORT;
            break;

        case LCD_INITIALIZING:
            init_lcd();
            break;

        case LCD_RUNNING:
            if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
                break;
            else
                dma_send_first_row();
            break;

        default:
            break;
    }
}

void control_lcd(void)
{
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
    {
        if (dma_lcd_ctrl.is_row_completed == TRUE)
        {
            dma_send_next_row();
        }
        else
        {
            wait_for_dma_completion();
        }
    }
    else
    {
        update_lcd_state();
    }
}

// Call this from HAL_SPI_TxCpltCallback()
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        dma_lcd_ctrl.is_row_completed = TRUE;
    }
}
