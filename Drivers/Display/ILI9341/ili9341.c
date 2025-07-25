#include "main.h"

// SPI and GPIO configuration
extern SPI_HandleTypeDef hspi2;
extern EventGroupHandle_t display_event;

// Timing constants (in milliseconds)
#define ILI9341_DMA_TIMEOUT_COUNT   (100)
#define ILI9341_RESET_DELAY_MS      (1)
#define ILI9341_SLEEP_OUT_DELAY_MS  (120)
#define ILI9341_BACKLIGHT_DELAY_MS  (13)

#define CURRENT_RENDER_BUFFER  (line_buffer[active_buf_idx])
#define CURRENT_DMA_BUFFER     (line_buffer[1 - active_buf_idx])

#define SWAP_BUFFERS() do { active_buf_idx = 1 - active_buf_idx; } while (0)
#define DRAW_CLEAR_SCREEN()     {draw_screen(CURRENT_RENDER_BUFFER, DMA_WRITE_CLEAR_SCREEN);}
#define DRAW_FRAME_BUFFER()     {draw_screen(CURRENT_RENDER_BUFFER, DMA_WRITE_FRAMEBUFFER);}

// DMA write operation types
typedef enum
{
    DMA_WRITE_NONE,
    DMA_WRITE_INIT_SCREEN,
    DMA_WRITE_CLEAR_SCREEN,
    DMA_WRITE_FRAMEBUFFER,
} dma_write_type_t;

// DMA control structure
typedef struct
{
    uint8_t current_row; // Current row being processed
    uint16_t wait_count; // DMA timeout counter
    dma_write_type_t write_type; // Type of DMA operation
    bool is_row_completed; // Flag for row completion
    bool is_writing; // Flag for active DMA transfer
    bool multiple_byte;
} dma_control_t;

// Static variables

static SemaphoreHandle_t dma_semaphore = NULL;

static dma_control_t dma_control = {0};
static ili9341_init_state_t init_state = ILI9341_INIT_NONE;
static ili9341_operation_state_t current_state = ILI9341_STATE_READY;
static ili9341_operation_state_t target_state = ILI9341_STATE_READY;
static uint16_t init_timeout_ms = 0;
static uint8_t init_sequence_index = 0;
static uint8_t framebuffer[ILI9341_FRAMEBUFFER_SIZE];
static uint16_t line_buffer[2][ILI9341_WIDTH];
static uint8_t active_buf_idx = 0; // 0 or 1
static display_info_t display_info;

// Color info
static uint16_t fg = 0xd68a;
static uint16_t bg = 0x25ae;

// Initialization command sequence
static uint8_t init_commands[] = {
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
    0x36, 1, 0x28, // memory access control
    0x3A, 1, 0x55, // Pixel Format
    0xB1, 2, 0x00, 0x18, // Frame Rate Control (In Normal Mode)
    0xB6, 3, 0x08, 0x82, 0x27, // display function control
    0xF2, 1, 0x02, // disable 3Gamma]
    0x26, 1, 0x01, // Gamma set
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // positive gamma correction
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // negative gamma correction
};

// Initialize DMA control structure
static void reset_dma_control(void) {
    dma_control.current_row = 0;
    dma_control.wait_count = 0;
    dma_control.write_type = DMA_WRITE_NONE;
    dma_control.is_writing = false;
    dma_control.is_row_completed = false;
}

// Initialize LCD controller state
static void reset_lcd_controller(void) {
    // Reset operational and initialization states to their initial values
    current_state = ILI9341_STATE_READY;
    init_state = ILI9341_INIT_NONE;
    target_state = ILI9341_STATE_READY;
    init_timeout_ms = 0;
    init_sequence_index = 0;
    reset_dma_control();
    // Clear framebuffer to prevent stale data
    memset(framebuffer, 0, ILI9341_FRAMEBUFFER_SIZE);
}

// Convert 16-bit value to RGB565
static uint16_t swap_byte(uint16_t value) {
    return (value >> 8) | (value << 8);
}

static inline uint16_t get_pixel_color(uint8_t byte, uint8_t bit_pos) {
    return (byte & (1 << bit_pos)) ? swap_byte(fg) : swap_byte(bg);
}

// Draw zero screen
static void draw_screen(uint16_t* buffer, dma_write_type_t write_type) {
    uint16_t row_offset = dma_control.current_row * ILI9341_BYTES_PER_ROW;

    if (row_offset >= ILI9341_FRAMEBUFFER_SIZE - ILI9341_BYTES_PER_ROW) {
        return; // Prevent buffer overflow
    }

    uint16_t send_index = 0;
    for (uint16_t byte_idx = 0; byte_idx < ILI9341_BYTES_PER_ROW; ++byte_idx) {
        if (write_type == DMA_WRITE_CLEAR_SCREEN) {
            buffer[send_index++] = 0x0000;
        } else {
            uint8_t byte = framebuffer[row_offset + byte_idx];

            for (int bit = 7; bit >= 0; --bit) {
                buffer[send_index++] = get_pixel_color(byte, bit);
            }
        }
    }
}

#if 0
// Convert a row of 1-bit framebuffer data to RGB565 color format
static void convert_row_to_rgb565(uint16_t* buffer) {
    uint16_t row_offset = dma_control.current_row * ILI9341_BYTES_PER_ROW;

    if (row_offset >= ILI9341_FRAMEBUFFER_SIZE - ILI9341_BYTES_PER_ROW) {
        return; // Prevent buffer overflow
    }

    uint16_t send_index = 0;
#if 1
    uint16_t _fg = swap_byte(fg);
    uint16_t _bg = swap_byte(bg);
#endif

    for (uint16_t byte_idx = 0; byte_idx < ILI9341_BYTES_PER_ROW; ++byte_idx) {
        uint8_t byte = framebuffer[row_offset + byte_idx];
#if 1
        uint16_t color = fg; /// default
        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            color = ((byte << bit_idx)  & 0x80)? _fg : _bg;
            buffer[send_index++] = color;
        }
#else
        // buffer[send_index++] = (byte & 0x80) ? swap_byte(fg) : swap_byte(bg); // Bit 7
        // buffer[send_index++] = (byte & 0x40) ? swap_byte(fg) : swap_byte(bg); // Bit 6
        // buffer[send_index++] = (byte & 0x20) ? swap_byte(fg) : swap_byte(bg); // Bit 5
        // buffer[send_index++] = (byte & 0x10) ? swap_byte(fg) : swap_byte(bg); // Bit 4
        // buffer[send_index++] = (byte & 0x08) ? swap_byte(fg) : swap_byte(bg); // Bit 3
        // buffer[send_index++] = (byte & 0x04) ? swap_byte(fg) : swap_byte(bg); // Bit 2
        // buffer[send_index++] = (byte & 0x02) ? swap_byte(fg) : swap_byte(bg); // Bit 1
        // buffer[send_index++] = (byte & 0x01) ? swap_byte(fg) : swap_byte(bg); // Bit 0
#endif
    }
}
#endif

// Send data over SPI
static HAL_StatusTypeDef send_data(ili9341_data_type_t type, uint8_t *data, uint16_t len, bool use_dma) {
    if (!data || len == 0) return HAL_ERROR;
    
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, 
                     (type == ILI9341_DATA_COMMAND) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(&hspi2, data, len);

    dma_control.multiple_byte = use_dma;

    if (status != HAL_OK) {
        // Log error (assuming a logging mechanism)
    }
    return status;

}

void ILI9341_Write_Command(uint8_t cmd)
{
    send_data(ILI9341_DATA_COMMAND, &cmd, 1, false);
}

void ILI9341_Write_Data(uint8_t data)
{
    send_data(ILI9341_DATA_PAYLOAD, &data, 1, false);
}

// Send a single command
static HAL_StatusTypeDef send_command(uint8_t cmd) {
    return send_data(ILI9341_DATA_COMMAND, &cmd, 1, false);
}

// Send data payload
static HAL_StatusTypeDef send_payload(uint8_t *data, uint16_t len, bool use_dma) {
    HAL_StatusTypeDef status = send_data(ILI9341_DATA_PAYLOAD, data, len, use_dma);

    if (status != HAL_OK) {
        reset_dma_control();
        xSemaphoreGive(dma_semaphore);
    }

    return status;
}

// Set display RAM address window
static void set_memory_window(void) {
    uint16_t x0 = 0, x1 = 319;
    uint16_t y0 = 0, y1 = 239;

    uint8_t column_addr[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF}; // 0 to 319
    uint8_t row_addr[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};    // 0 to 239
    
    send_command(0x2A); // Column Address Set
    send_payload(column_addr, 4, false);
    
    send_command(0x2B); // Page Address Set
    send_payload(row_addr, 4, false);
    
    send_command(0x2C); // Memory Write
}

// Perform hardware reset
static void hw_reset(void) {
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_RESET);
    for(int i = 0; i < 10000; i++)
    {

    }
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
}

// Execute initialization command sequence
static void execute_init_sequence(void) {
#if 1
    init_sequence_index = 0;
    while (init_sequence_index < sizeof(init_commands)) {
        uint8_t cmd = init_commands[init_sequence_index++];
        uint8_t len = init_commands[init_sequence_index++];

        send_command(cmd);
        if (len > 0) {
            send_payload(&init_commands[init_sequence_index], len, false);
            init_sequence_index += len;
        }
    }
#else
    ILI9341_Write_Command(0x01);
    HAL_Delay(100);

    //POWER CONTROL A
    ILI9341_Write_Command(0xCB);
    ILI9341_Write_Data(0x39);
    ILI9341_Write_Data(0x2C);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x34);
    ILI9341_Write_Data(0x02);

    //POWER CONTROL B
    ILI9341_Write_Command(0xCF);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0xC1);
    ILI9341_Write_Data(0x30);

    //DRIVER TIMING CONTROL A
    ILI9341_Write_Command(0xE8);
    ILI9341_Write_Data(0x85);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x78);

    //DRIVER TIMING CONTROL B
    ILI9341_Write_Command(0xEA);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x00);

    //POWER ON SEQUENCE CONTROL
    ILI9341_Write_Command(0xED);
    ILI9341_Write_Data(0x64);
    ILI9341_Write_Data(0x03);
    ILI9341_Write_Data(0x12);
    ILI9341_Write_Data(0x81);

    //PUMP RATIO CONTROL
    ILI9341_Write_Command(0xF7);
    ILI9341_Write_Data(0x20);

    //POWER CONTROL,VRH[5:0]
    ILI9341_Write_Command(0xC0);
    ILI9341_Write_Data(0x23);

    //POWER CONTROL,SAP[2:0];BT[3:0]
    ILI9341_Write_Command(0xC1);
    ILI9341_Write_Data(0x10);

    //VCM CONTROL
    ILI9341_Write_Command(0xC5);
    ILI9341_Write_Data(0x3E);
    ILI9341_Write_Data(0x28);

    //VCM CONTROL 2
    ILI9341_Write_Command(0xC7);
    ILI9341_Write_Data(0x86);

    //MEMORY ACCESS CONTROL
    ILI9341_Write_Command(0x36);
    ILI9341_Write_Data(0x28);

    //PIXEL FORMAT
    ILI9341_Write_Command(0x3A);
    ILI9341_Write_Data(0x55);

    //FRAME RATIO CONTROL, STANDARD RGB COLOR
    ILI9341_Write_Command(0xB1);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x18);

    //DISPLAY FUNCTION CONTROL
    ILI9341_Write_Command(0xB6);
    ILI9341_Write_Data(0x08);
    ILI9341_Write_Data(0x82);
    ILI9341_Write_Data(0x27);

    //3GAMMA FUNCTION DISABLE
    ILI9341_Write_Command(0xF2);
    ILI9341_Write_Data(0x00);

    //GAMMA CURVE SELECTED
    ILI9341_Write_Command(0x26);
    ILI9341_Write_Data(0x01);

    //POSITIVE GAMMA CORRECTION
    ILI9341_Write_Command(0xE0);
    ILI9341_Write_Data(0x0F);
    ILI9341_Write_Data(0x31);
    ILI9341_Write_Data(0x2B);
    ILI9341_Write_Data(0x0C);
    ILI9341_Write_Data(0x0E);
    ILI9341_Write_Data(0x08);
    ILI9341_Write_Data(0x4E);
    ILI9341_Write_Data(0xF1);
    ILI9341_Write_Data(0x37);
    ILI9341_Write_Data(0x07);
    ILI9341_Write_Data(0x10);
    ILI9341_Write_Data(0x03);
    ILI9341_Write_Data(0x0E);
    ILI9341_Write_Data(0x09);
    ILI9341_Write_Data(0x00);

    //NEGATIVE GAMMA CORRECTION
    ILI9341_Write_Command(0xE1);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x0E);
    ILI9341_Write_Data(0x14);
    ILI9341_Write_Data(0x03);
    ILI9341_Write_Data(0x11);
    ILI9341_Write_Data(0x07);
    ILI9341_Write_Data(0x31);
    ILI9341_Write_Data(0xC1);
    ILI9341_Write_Data(0x48);
    ILI9341_Write_Data(0x08);
    ILI9341_Write_Data(0x0F);
    ILI9341_Write_Data(0x0C);
    ILI9341_Write_Data(0x31);
    ILI9341_Write_Data(0x36);
    ILI9341_Write_Data(0x0F);

    //EXIT SLEEP
    ILI9341_Write_Command(0x11);
    HAL_Delay(120);

    //TURN ON DISPLAY
    ILI9341_Write_Command(0x29);
#endif
}

// Start drawing a screen (initial, clear, or framebuffer)
static void start_screen_draw(void) {
    if (xSemaphoreTake(dma_semaphore, portMAX_DELAY) == pdTRUE) {
        set_memory_window();
        
        if (dma_control.write_type == DMA_WRITE_FRAMEBUFFER) {
            // convert_row_to_rgb565(CURRENT_RENDER_BUFFER);
            DRAW_FRAME_BUFFER();
        } else {
            uint16_t* buf = (uint16_t*)line_buffer;
            for (int i = 0; i < ILI9341_WIDTH; i++) { // 320 pixels
                buf[i] = 0x0000; // Fill with 0x00 as 16-bit words
            }
        }

        SWAP_BUFFERS();
        send_payload((uint8_t*)CURRENT_DMA_BUFFER, ILI9341_LINE_BUFFER_SIZE, true);
        dma_control.is_writing = true;
        dma_control.current_row = 1;
    } else {
        printf("Semaphore timeout in start_screen_draw\n");
    }
}

// Draw the next row
static void draw_next_row(void) {
    if (dma_control.current_row >= ILI9341_HEIGHT) {
        xSemaphoreGive(dma_semaphore);
        if (dma_control.write_type == DMA_WRITE_INIT_SCREEN) {
            init_state = ILI9341_INIT_BACKLIGHT;
        } else if (dma_control.write_type == DMA_WRITE_CLEAR_SCREEN) {
            current_state = ILI9341_STATE_STOPPING;
        }
        reset_dma_control();
        return;
    }
    
    if (dma_control.write_type == DMA_WRITE_FRAMEBUFFER) {
        // convert_row_to_rgb565(CURRENT_RENDER_BUFFER);
        DRAW_FRAME_BUFFER();
    } else {
        uint16_t* buf = (uint16_t*)line_buffer;
        for (int i = 0; i < ILI9341_WIDTH; i++) { // 320 pixels
            buf[i] = 0x0000; // Fill with 0x00 as 16-bit words
        }
    }

    SWAP_BUFFERS();
    send_payload((uint8_t*)CURRENT_DMA_BUFFER, ILI9341_LINE_BUFFER_SIZE, true);
    dma_control.is_row_completed = false;
    dma_control.current_row++;
}

// Check initialization timeout
static bool has_timeout_expired(uint16_t timeout_ms) {
    return init_timeout_ms >= timeout_ms;
}

// Timer callback for initialization timeouts
void init_timeout_callback(TimerHandle_t timer) {
    switch (init_state) {
    case ILI9341_INIT_HW_RESET:
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;

    case ILI9341_INIT_COMMANDS:
        if (has_timeout_expired(ILI9341_RESET_DELAY_MS))
        {
            xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        }
        break;

    case ILI9341_INIT_SLEEP_OUT:
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;
    
    case ILI9341_INIT_SCREEN:
        if (has_timeout_expired(ILI9341_SLEEP_OUT_DELAY_MS))
        {
            xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        }
        break; 

    case ILI9341_INIT_BACKLIGHT:
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;

    case ILI9341_INIT_COMPLETED:
    case ILI9341_INIT_NONE:
        break;
    }
    init_timeout_ms++;
}

// Process initialization state machine
static void process_initialization(void) {
    ili9341_init_state_t prev_state = init_state;

    switch (init_state) {
    case ILI9341_INIT_HW_RESET:
        hw_reset();
        init_state = ILI9341_INIT_COMMANDS;
        break;

    case ILI9341_INIT_COMMANDS:
        if (has_timeout_expired(ILI9341_RESET_DELAY_MS))
        {
            execute_init_sequence();
            init_state = ILI9341_INIT_SLEEP_OUT;
        }
        break;

    case ILI9341_INIT_SLEEP_OUT:
        send_command(0x11); // sleep out
        init_state = ILI9341_INIT_SCREEN;
        break;

    case ILI9341_INIT_SCREEN:
        if (has_timeout_expired(ILI9341_SLEEP_OUT_DELAY_MS))
        {
            dma_control.write_type = DMA_WRITE_INIT_SCREEN;
            start_screen_draw();
        }
        break;

    case ILI9341_INIT_BACKLIGHT:
        HAL_GPIO_WritePin(LD2_GPIO_Port, LCD_BACKLIGHT_PIN, GPIO_PIN_SET);
        send_command(0x29); // set backlight
        current_state = ILI9341_STATE_RUNNING;
        init_state = ILI9341_INIT_COMPLETED;
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
        break;

    case ILI9341_INIT_COMPLETED:
    case ILI9341_INIT_NONE:
        break;
    }

    if (prev_state != init_state)
    {
        init_timeout_ms = 0;
    }
}

// Handle DMA timeout
static void handle_dma_timeout(void) {
    if (dma_control.wait_count >= ILI9341_DMA_TIMEOUT_COUNT) {
        reset_dma_control();
        if (dma_semaphore) {
            xSemaphoreGive(dma_semaphore); // Release on timeout
        }
        printf("DMA timeout for row %d, count: %d, resetting dma_control\n", 
               dma_control.current_row, dma_control.wait_count);
    } else {
        dma_control.wait_count++;
    }
}

// Initialize the ILI9341 driver
static void ili9341_driver_init(void) {
    static bool is_initialized = false;
    if (is_initialized) return;
    
    reset_lcd_controller();
    
    TimerHandle_t init_timer = xTimerCreate(
        "ILI9341InitTimer",
        pdMS_TO_TICKS(1),
        pdTRUE,
        NULL,
        init_timeout_callback
    );
    
    if (init_timer) {
        xTimerStart(init_timer, 0);
    } else {
        // Log timer creation failure
    }

    dma_semaphore = xSemaphoreCreateBinary();
    if (dma_semaphore) {
        xSemaphoreGive(dma_semaphore);
    }
    
    is_initialized = true;
}

// Update operational state
// static void update_operation_state(void) {
//     if (current_state == ILI9341_STATE_STOPPING && target_state == ILI9341_STATE_READY) {
//         current_state = ILI9341_STATE_READY;
//     } else if (current_state == ILI9341_STATE_READY && target_state == ILI9341_STATE_RUNNING) {
//         current_state = ILI9341_STATE_RUNNING;
//     } else if (current_state == ILI9341_STATE_RUNNING && target_state == ILI9341_STATE_STOPPING) {
//         current_state = ILI9341_STATE_STOPPING;
//     }
// }

// Main controller task
void ili9341_controller_task(void) {
    if (dma_control.is_writing) {
        if (dma_control.is_row_completed) {
            draw_next_row();
        } else {
            handle_dma_timeout();
        }
        return;
    }

    switch (current_state) {
    case ILI9341_STATE_READY:
        current_state = ILI9341_STATE_INITIALIZING;
        init_state = ILI9341_INIT_HW_RESET;
        break;

    case ILI9341_STATE_INITIALIZING:
        process_initialization();
        break;

    case ILI9341_STATE_RUNNING:
        if (dma_control.is_writing != true)
        {
            dma_control.write_type = DMA_WRITE_FRAMEBUFFER;
            start_screen_draw();
        }
        break;

    case ILI9341_STATE_STOPPING:
        dma_control.write_type = DMA_WRITE_CLEAR_SCREEN;
        start_screen_draw();
    break;

    case ILI9341_STATE_STOPPED:
        break;
    }
}

// Get framebuffer pointer
static display_info_t* get_framebuffer(void) {
    display_info.data = &framebuffer[0];
    display_info.size = ILI9341_FRAMEBUFFER_SIZE;
    display_info.fg_color = fg;
    display_info.bg_color = bg;

    return &display_info;
}

// DMA completion callback
void ili9341_dma_complete(void) {
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    dma_control.is_row_completed = true;
    if (dma_control.multiple_byte)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xEventGroupSetBitsFromISR(display_event, DISPLAY_EVENT_UPDATE, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// static void test_display(void)
// {
//     hw_reset();
//     execute_init_sequence();
// }

// Display driver structure
static const display_driver_t ili9341_driver = {
    .init = ili9341_driver_init,
    // .update = test_display,
    .get_framebuffer = get_framebuffer
};

// Get driver instance
const display_driver_t* ili9341_get_driver(void) {
    return &ili9341_driver;
}