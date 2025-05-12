#include "macro.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_init.h"
#include "dev_lcd.h"
enum
{
    SEND_WITHOUT_DMA = 0,
    SEND_WITH_DMA
};

enum
{
    SENDING_COMMAND = 0,
    SENDING_DATA
};

extern SPI_HandleTypeDef hspi2;

typedef struct
{
    u16 current_row;
    u16 wait_count;
    u8 is_row_completed;
} DMA_LCD_CTRL;

static DMA_LCD_CTRL dma_lcd_ctrl;
static LCD_INFO lcd_info;

static void lcd_send(uint8_t type, uint8_t* data, uint16_t length, uint8_t dma_used)
{
    u32 start_time = HAL_GetTick();

    // Đợi SPI rảnh
    while (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY))
    {
        if ((HAL_GetTick() - start_time) > SPI_TIMEOUT_MS)
        {
            // Timeout: Không thể gửi
            return;
        }
    }

    // Chọn Command/Data
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, (type == SENDING_COMMAND) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET); // active CS

    HAL_StatusTypeDef result;
    if (dma_used == SEND_WITHOUT_DMA)
    {
        result = HAL_SPI_Transmit_IT(&hspi2, data, length);
    }
    else
    {
        result = HAL_SPI_Transmit_DMA(&hspi2, data, length);
    }

    if (result != HAL_OK)
    {
        HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET); // release CS nếu lỗi
    }
}

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
        HAL_SPI_Transmit_DMA(&hspi2, ptr, LCD_WIDTH * BIT_PER_PIXEL / 8);
        dma_lcd_ctrl.is_row_completed = FALSE;
        dma_lcd_ctrl.current_row++;
    }
}

static void dma_send_first_row(void)
{
    u8* ptr = &lcd_info.screen_buffer[0];
    HAL_SPI_Transmit_DMA(&hspi2, ptr, LCD_WIDTH * BIT_PER_PIXEL / 8);
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
            if (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY)
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
    if (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY)
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

