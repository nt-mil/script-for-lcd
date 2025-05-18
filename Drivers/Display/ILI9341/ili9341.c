#include "main.h"
#include "ili9341.h"

#define CMD  0
#define DATA 1

#define DMA_MAX_COUNTS (10)

#define RESET_PORT_DELAY    (5)
#define SLEEP_OUT_DELAY     (120)

extern SPI_HandleTypeDef hspi2;
extern EventGroupHandle_t display_event;

typedef struct {
    uint8_t current_row;
    uint16_t wait_count;
    bool is_row_completed;
    bool is_writting_completed;
} dma_ctrl_t;

static dma_ctrl_t dma_ctrl;

static initial_state_t init_state;
static operation_state_t current_state = STATE_READY;
static operation_state_t target_state = STATE_READY;
static uint16_t init_timeout = 0;
static uint8_t init_seq_index = 0;
static uint8_t frame_buffer[FB_SIZE];

static const uint8_t init_cmds[] = {
    0xEF, 3, 0x03, 0x80, 0x02,
    0xCF, 3, 0x00, 0xC1, 0x30,
    0xED, 4, 0x64, 0x03, 0x12, 0x81,
    0xE8, 3, 0x85, 0x00, 0x78,
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    0xF7, 1, 0x20,
    0xEA, 2, 0x00, 0x00,
    0xC0, 1, 0x23, // Power control
    0xC1, 1, 0x10, // Power control
    0xC5, 2, 0x3e, 0x28, // VCM control
    0xC7, 1, 0x86, // VCM control2
    0x36, 1, 0x48, // Memory Access Control
    0x3A, 1, 0x55, // Pixel Format
    0xB1, 2, 0x00, 0x18, // Frame Rate
    0xB6, 3, 0x08, 0x82, 0x27, // Display Function Control
    0xF2, 1, 0x00, // Enable 3G
    0x26, 1, 0x01, // Gamma Set
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    0x11, 0, // Exit Sleep
    0x29, 0  // Display ON
};

static void send(data_type_t type, const uint8_t* data, uint16_t len, bool use_dma)
{
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, (type == SEND_COMMAND) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    if (use_dma) 
    {
        HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)data, len);
    }
    else
    {
        HAL_SPI_Transmit_IT(&hspi2, (uint8_t*)data, len);
    }
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
    // HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
    // HAL_Delay(1);
    // HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_RESET);
}

void reset_sequence(void)
{
    init_seq_index = 0;

    while (init_seq_index < sizeof(init_cmds))
    {
        uint8_t cmd = init_cmds[init_seq_index++];
        uint8_t len = init_cmds[init_seq_index++];
        
        send(SEND_COMMAND, &cmd, 1, false);

        if (len) {
            send(SEND_DATA, &init_cmds[init_seq_index], len, false);
            init_seq_index += len;
        }
    }
}

void check_wait_init_timeout(TimerHandle_t timer)
{
    switch (init_state)
    {
        case STATE_HW_RESET:
            xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
            break;

        case STATE_SLEEP_OUT:
            if (check_init_timeout(RESET_PORT_DELAY))
            {
                xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
            }
            break;

        case STATE_INITAL_CMD:
            if (check_init_timeout(SLEEP_OUT_DELAY))
            {
                xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
            }
            break;
        
        case STATE_COMPLETED:
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
            init_state = STATE_SLEEP_OUT;
            break;

        case STATE_SLEEP_OUT:
            if (check_init_timeout(RESET_PORT_DELAY))
            {
                // send command sleep out
                init_state = STATE_INITAL_CMD;
            }
            break;

        case STATE_INITAL_CMD:
            if (check_init_timeout(SLEEP_OUT_DELAY))
            {
                reset_sequence();
                // write zero screen
                init_state = STATE_COMPLETED;
            }
            break;
        
        case STATE_COMPLETED:
            current_state = STATE_RUNNING;
            break;
    }

    if (pre_init_state != init_state)
    {
        init_timeout = 0;
    }
}

static void init_dma_state(void)
{
    dma_ctrl.current_row = 0;
    dma_ctrl.is_row_completed = false;
    dma_ctrl.wait_count = 0;
    dma_ctrl.is_writting_completed = false;
}

static void dma_send_row(uint8_t row_index)
{
    if (dma_ctrl.current_row >= HEIGHT)
    {
        init_dma_state();
    }
    else
    {
        send(SEND_DATA, &frame_buffer[row_index], BYTE_PER_ROW, true);
        dma_ctrl.is_row_completed = FALSE;

        if (row_index)
        {
            dma_ctrl.current_row++;
        }
        else
        {
            dma_ctrl.current_row = 1;
        }
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
        current_state = STATE_READY;
        target_state = STATE_READY;
        init_seq_index = 0;

        init_dma_state();
        dma_ctrl.is_writting_completed = true;

        init_timer = xTimerCreate("CheckWaitInitTime", pdMS_TO_TICKS(100), pdTRUE, (void *)0, check_wait_init_timeout);
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
    if (dma_ctrl.is_writting_completed == false)
    {
        if (dma_ctrl.is_row_completed == true)
        {
            uint8_t row_index = dma_ctrl.current_row * BYTE_PER_ROW;
            dma_send_row(row_index);
        }
        else
        {
            wait_for_dma_writting_row_complete();
        }
    }
    else
    {
        // update_lcd_state();

        switch (current_state)
        {
            case STATE_READY:
                current_state = STATE_INITIALIZING;
                init_state = STATE_HW_RESET;
                xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
                break;

            case STATE_INITIALIZING:
                process_lcd_init();
                xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
                break;

            case STATE_RUNNING:
                if (dma_ctrl.is_writting_completed == true)
                {
                    dma_send_row(0);
                }
                break;

            default:
                break;
        }
    }
}

static uint8_t* ili9341_get_framebuffer(void)
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

const display_driver_t* get_ili9341_display_driver(void)
{
    return &ili9341_display_driver;
}