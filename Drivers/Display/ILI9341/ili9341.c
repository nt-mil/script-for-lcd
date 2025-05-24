#include "main.h"

#define CMD 0
#define DATA 1

#define DMA_MAX_COUNTS (10)

#define RESET_PORT_DELAY (5)
#define SLEEP_OUT_DELAY (120)
#define BACKLIGTH_DELAY (13)

extern SPI_HandleTypeDef hspi2;
extern EventGroupHandle_t display_event;

enum
{
    DMA_WRITE_TYPE_NONE,
    DMA_WRITE_TYPE_INIT_SCREEN,
    DMA_WRITE_TYPE_ZERO_SCREEN,
    DMA_WRITE_TYPE_NON_ZERO_SCREEN,
};

typedef struct
{
    uint8_t current_row;
    uint16_t wait_count;
    uint8_t write_type;
    bool is_row_completed;
    bool is_writting;
} dma_ctrl_t;

static dma_ctrl_t dma_ctrl;

static initial_state_t init_state;
static operation_state_t current_state = STATE_READY;
static operation_state_t target_state = STATE_READY;
static uint16_t init_timeout = 0;
static uint8_t init_seq_index = 0;
static uint8_t frame_buffer[FB_SIZE];
static uint16_t line_buffer[ACTUAL_BYTE_PER_ROW/2];

static uint8_t init_cmds[] = {
    0x01, 0, // sw reset
    0xCF, 3, 0x00, 0xD9, 0x30, // power control B
    0xED, 4, 0x64, 0x03, 0x12, 0x81, // power on sequence
    0xE8, 3, 0x85, 0x10, 0x7A, // driver timing control A
    0xCB, 5, 0x29, 0x2C, 0x00, 0x34, 0x02, // power control A
    0xF7, 1, 0x20,// pump ratio control
    0xEA, 2, 0x00, 0x00, // driver timing control B
    0xC0, 1, 0x1B, // Power Control 1
    0xC1, 1, 0x12, // power control 2
    0xC5, 2, 0x08, 0x26, // VCM control
    0xC7, 1, 0xB7, // VCM control 2
    0x36, 1, 0x48, // memory access control
    0x3A, 1, 0x55, // Pixel Format
    0xB1, 2, 0x00, 0x18, // Frame Rate Control (In Normal Mode)
    0xB6, 3, 0x08, 0x82, 0x27, // display function control
    0xF2, 1, 0x02, // disable 3Gamma]
    0x26, 1, 0x01, // Gamma set
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // positive gamma correction
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // negative gamma correction
};

static uint16_t gray4_to_rgb565(uint8_t gray4)
{
    uint8_t r = (gray4 * 31) / 15;
    uint8_t g = (gray4 * 61) / 15;
    uint8_t b = (gray4 * 31) / 15;

    uint16_t rgb565 = (r << 11) | (g << 5) | b;

    return rgb565;
}

static void convert_gray4bit_to_565rgb_line(void)
{
    uint16_t index, rgb1, rgb2;
    uint8_t byte, gray1, gray2;
    for (int i = 0; i < BYTE_PER_ROW; i++)
    {
        index = dma_ctrl.current_row * BYTE_PER_ROW;
        byte = frame_buffer[index];

        gray1 = byte >> 4;
        gray2 = byte & 0x0F;

        rgb1 = gray4_to_rgb565(gray1);
        rgb2 = gray4_to_rgb565(gray2);

        line_buffer[(i << 1)] = rgb1;
        line_buffer[(i << 1) + 1] = rgb2;
    }
}

static void send(data_type_t type, uint8_t *data, uint16_t len, bool use_dma)
{
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, (type == SEND_COMMAND) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

    if (use_dma)
    {
        HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)data, len);
    }
    else
    {
        HAL_SPI_Transmit_IT(&hspi2, (uint8_t *)data, len);
    }
}

static void ili9341_send_cmd(uint8_t cmd)
{
    send(SEND_COMMAND, &cmd, 1, false);
}

static void ili9341_send_data(uint8_t *data, uint16_t len, bool use_dma)
{
    send(SEND_DATA, data, len, use_dma);
}

static void set_ram_offset(void)
{
    uint8_t x[4] = {0x00, 0x00, 0x00, 0x14};
    uint8_t y[4] = {0x00, 0x00, 0x00, 0xF0};

    ili9341_send_cmd(0x2A); // Column Address
    ili9341_send_data(&x[0], 4, false);

    vTaskDelay(1);

    ili9341_send_cmd(0x2B); // Row Address
    ili9341_send_data(&y[0], 4, false);

    vTaskDelay(1);

    ili9341_send_cmd(0x2C); // Write Memory
}

static bool check_init_timeout(uint16_t timeout)
{
    if (init_timeout >= timeout)
    {
        return true;
    }
    return false;
}

static void hw_reset(void)
{
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_RESET);
    vTaskDelay(1);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
}

void reset_sequence(void)
{
    init_seq_index = 0;

    while (init_seq_index < sizeof(init_cmds))
    {
        uint8_t cmd = init_cmds[init_seq_index++];
        uint8_t len = init_cmds[init_seq_index++];

        send(SEND_COMMAND, &cmd, 1, true);

        if (len)
        {
            send(SEND_DATA, &init_cmds[init_seq_index], len, true);
            init_seq_index += len;
        }
        vTaskDelay(1);
    }
}

static void init_lcd_screen(void)
{
    current_state = STATE_READY;
    init_state = STATE_NONE;
    init_timeout = 0;
    target_state = STATE_READY;
}

static void init_dma_state(void)
{
    dma_ctrl.current_row = 0;
    dma_ctrl.wait_count = 0;
    dma_ctrl.is_writting = false;
    // dma_ctrl.is_row_completed = false;
    dma_ctrl.write_type = DMA_WRITE_TYPE_NONE;
}

static void dma_start_drawing_screen(void)
{
    // define area of frame
    set_ram_offset();

    if (dma_ctrl.write_type == DMA_WRITE_TYPE_NON_ZERO_SCREEN)
    {
        convert_gray4bit_to_565rgb_line();
    }
    else
    {
        memset(((uint8_t*)(&line_buffer[0])), 0, sizeof(line_buffer));
    }

    ili9341_send_data(((uint8_t*)(&line_buffer[0])), sizeof(line_buffer), true);
    dma_ctrl.is_writting = true;
    dma_ctrl.current_row = 1;
}

static void dma_draw_next_row(void)
{
    if (dma_ctrl.current_row >= HEIGHT)
    {
        if (dma_ctrl.write_type == DMA_WRITE_TYPE_INIT_SCREEN)
        {
            init_state = STATE_BACKLIGHT;
        }
        else if (dma_ctrl.write_type == DMA_WRITE_TYPE_ZERO_SCREEN)
        {
            current_state = STATE_STOP;
        }
        init_dma_state();
    }
    else
    {
        if (dma_ctrl.write_type == DMA_WRITE_TYPE_NON_ZERO_SCREEN)
        {
            convert_gray4bit_to_565rgb_line();
        }
        else
        {
            memset(line_buffer, 0, sizeof(line_buffer));
        }

        ili9341_send_data((uint8_t*)(&line_buffer[0]), sizeof(line_buffer), true);

        dma_ctrl.is_row_completed = false;
        dma_ctrl.current_row++;
    }
}


void check_wait_init_timeout(TimerHandle_t timer)
{
    printf("timer triggered!\n");
    switch (init_state)
    {
    case STATE_HW_RESET:
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;

    case STATE_INITAL_CMD:
        if (check_init_timeout(RESET_PORT_DELAY))
        {
            xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        }
        break;

    case STATE_SLEEP_OUT:
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;
    
    case STATE_INIT_SCREEN:
        if (check_init_timeout(SLEEP_OUT_DELAY))
        {
            xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        }
        break; 

    case STATE_BACKLIGHT:
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;

    case STATE_COMPLETED:
    case STATE_NONE:
        break;
    }
    init_timeout++;
}

static void process_lcd_init(void)
{
    initial_state_t pre_init_state = init_state;

    switch (init_state)
    {
    case STATE_HW_RESET:
        hw_reset();
        init_state = STATE_INITAL_CMD;
        break;

    case STATE_INITAL_CMD:
        if (check_init_timeout(RESET_PORT_DELAY))
        {
            reset_sequence();
            init_state = STATE_SLEEP_OUT;
        }
        break;

    case STATE_SLEEP_OUT:
        ili9341_send_cmd(0x11); // sleep out
        init_state = STATE_INIT_SCREEN;
        break;

    case STATE_INIT_SCREEN:
        if (check_init_timeout(SLEEP_OUT_DELAY))
        {
            dma_ctrl.write_type = DMA_WRITE_TYPE_INIT_SCREEN;
            dma_start_drawing_screen();
        }
        break;

    case STATE_BACKLIGHT:
        ili9341_send_cmd(0x29); // set backlight
        current_state = STATE_RUNNING;
        init_state = STATE_COMPLETED;
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;

    case STATE_COMPLETED:
    case STATE_NONE:
        break;
    }

    if (pre_init_state != init_state)
    {
        init_timeout = 0;
    }
}

static void wait_for_dma_writting_row_complete(void)
{
    if (dma_ctrl.wait_count >= DMA_MAX_COUNTS)
    {
        // timeout case
        init_dma_state();
    }
    else
    {
        dma_ctrl.wait_count++;
    }
}

static void ili9341_init(void)
{
    static bool init = false;
    TimerHandle_t init_timer;

    if (!init)
    {
        init_seq_index = 0;

        init_dma_state();
        init_lcd_screen();

        init_timer = xTimerCreate("CheckWaitInitTime", pdMS_TO_TICKS(1), pdTRUE, (void *)0, check_wait_init_timeout);
        if (init_timer != NULL)
        {
            xTimerStart(init_timer, 0);
        }

        init = true;
    }
}

static void update_lcd_state(void)
{
    if ((current_state == STATE_STOP) && (target_state == STATE_READY))
    {
        current_state = STATE_READY;
    }
    else if ((current_state == STATE_READY) && (target_state == STATE_RUNNING))
    {
        current_state = STATE_RUNNING;
    }
    else if ((current_state == STATE_RUNNING) && (target_state == STATE_STOP))
    {
        current_state = STATE_STOP;
    }
}

void ili9341_control(void)
{
    if (dma_ctrl.is_writting == true)
    {
        if (dma_ctrl.is_row_completed == true)
        {
            // send row: zero or data
            dma_draw_next_row();
        }
        else
        {
            wait_for_dma_writting_row_complete();
        }
    }
    else
    {
        switch (current_state)
        {
        case STATE_READY:
            current_state = STATE_INITIALIZING;
            init_state = STATE_HW_RESET;
            break;

        case STATE_INITIALIZING:
            process_lcd_init();
            break;

        case STATE_RUNNING:
            if (dma_ctrl.is_writting == true)
            {
                dma_ctrl.write_type = DMA_WRITE_TYPE_NON_ZERO_SCREEN;
                dma_start_drawing_screen();
            }
            break;

        case STATE_STOP:
            dma_ctrl.write_type = DMA_WRITE_TYPE_ZERO_SCREEN;
            dma_start_drawing_screen();
        break;

        case STATE_STOP_COMPLETED:
            break;
        }
    }
}

static uint8_t *ili9341_get_framebuffer(void)
{
    return &frame_buffer[0];
}

void dma_callback(void)
{
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    dma_ctrl.is_row_completed = true;
    // post event
}

static const display_driver_t ili9341_display_driver =
{
    .init = ili9341_init,
    .get_framebuffer = ili9341_get_framebuffer
};

const display_driver_t *get_ili9341_display_driver(void)
{
    return &ili9341_display_driver;
}