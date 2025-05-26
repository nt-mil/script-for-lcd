#include "main.h"

// SPI and GPIO configuration
extern SPI_HandleTypeDef hspi2;
extern EventGroupHandle_t display_event;

// Timing constants (in milliseconds)
#define ILI9341_DMA_TIMEOUT_COUNT   (10)
#define ILI9341_RESET_DELAY_MS      (5)
#define ILI9341_SLEEP_OUT_DELAY_MS  (120)
#define ILI9341_BACKLIGHT_DELAY_MS  (13)

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
} dma_control_t;

// Static variables

static dma_control_t dma_control = {0};
static ili9341_init_state_t init_state = ILI9341_INIT_NONE;
static ili9341_operation_state_t current_state = ILI9341_STATE_READY;
static ili9341_operation_state_t target_state = ILI9341_STATE_READY;
static uint16_t init_timeout_ms = 0;
static uint8_t init_sequence_index = 0;
static uint8_t framebuffer[ILI9341_FRAMEBUFFER_SIZE];
static uint16_t line_buffer[ILI9341_WIDTH];

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
    0x36, 1, 0x48, // memory access control
    0x3A, 1, 0x55, // Pixel Format
    0xB1, 2, 0x00, 0x18, // Frame Rate Control (In Normal Mode)
    0xB6, 3, 0x08, 0x82, 0x27, // display function control
    0xF2, 1, 0x02, // disable 3Gamma]
    0x26, 1, 0x01, // Gamma set
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // positive gamma correction
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // negative gamma correction
};

// Convert 4-bit grayscale to RGB565
static uint16_t grayscale_to_rgb565(uint8_t gray4) {
    uint8_t r = (gray4 * 31) / 15;
    uint8_t g = (gray4 * 61) / 15;
    uint8_t b = (gray4 * 31) / 15;

    return (r << 11) | (g << 5) | b;
}

// Convert a row of 4-bit grayscale to RGB565
static void convert_row_to_rgb565(void) {
    for (uint16_t i = 0; i < ILI9341_BYTES_PER_ROW; i++) {
        uint16_t fb_index = dma_control.current_row * ILI9341_BYTES_PER_ROW + i;
        uint8_t byte = framebuffer[fb_index];
        uint8_t gray1 = byte >> 4;
        uint8_t gray2 = byte & 0x0F;
        line_buffer[i * 2] = grayscale_to_rgb565(gray1);
        line_buffer[i * 2 + 1] = grayscale_to_rgb565(gray2);
    }
}

// Send data over SPI
static HAL_StatusTypeDef send_data(ili9341_data_type_t type, uint8_t *data, uint16_t len, bool use_dma) {
    if (!data || len == 0) return HAL_ERROR;
    
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, 
                     (type == ILI9341_DATA_COMMAND) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    
    HAL_StatusTypeDef status = use_dma ?
        HAL_SPI_Transmit_DMA(&hspi2, data, len) :
        HAL_SPI_Transmit_IT(&hspi2, data, len);
    
    if (status != HAL_OK) {
        // Log error (assuming a logging mechanism)
    }
    return status;

}

// Send a single command
static HAL_StatusTypeDef send_command(uint8_t cmd) {
    return send_data(ILI9341_DATA_COMMAND, &cmd, 1, false);
}

// Send data payload
static HAL_StatusTypeDef send_payload(uint8_t *data, uint16_t len, bool use_dma) {
    return send_data(ILI9341_DATA_PAYLOAD, data, len, use_dma);
}

// Set display RAM address window
static void set_memory_window(void) {
    uint8_t column_addr[4] = {0x00, 0x00, 0x01, 0x3F}; // 0 to 319
    uint8_t row_addr[4] = {0x00, 0x00, 0x00, 0xEF};    // 0 to 239
    
    send_command(0x2A); // Column Address Set
    send_payload(column_addr, 4, false);
    
    send_command(0x2B); // Page Address Set
    send_payload(row_addr, 4, false);
    
    send_command(0x2C); // Memory Write
}

// Perform hardware reset
static void hw_reset(void)
{
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_RESET);
    vTaskDelay(1);
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
}

// Execute initialization command sequence
static void execute_init_sequence(void) {
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
}

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

// Start drawing a screen (initial, clear, or framebuffer)
static void start_screen_draw(void) {
    set_memory_window();
    
    if (dma_control.write_type == DMA_WRITE_FRAMEBUFFER) {
        convert_row_to_rgb565();
    } else {
        memset(line_buffer, 0, sizeof(line_buffer));
    }
    
    send_payload((uint8_t*)line_buffer, sizeof(line_buffer), true);
    dma_control.is_writing = true;
    dma_control.current_row = 1;
}

// Draw the next row
static void draw_next_row(void) {
    if (dma_control.current_row >= ILI9341_HEIGHT) {
        if (dma_control.write_type == DMA_WRITE_INIT_SCREEN) {
            init_state = ILI9341_INIT_BACKLIGHT;
        } else if (dma_control.write_type == DMA_WRITE_CLEAR_SCREEN) {
            current_state = ILI9341_STATE_STOPPING;
        }
        reset_dma_control();
        return;
    }
    
    if (dma_control.write_type == DMA_WRITE_FRAMEBUFFER) {
        convert_row_to_rgb565();
    } else {
        memset(line_buffer, 0, sizeof(line_buffer));
    }
    
    send_payload((uint8_t*)line_buffer, sizeof(line_buffer), true);
    dma_control.is_row_completed = false;
    dma_control.current_row++;
}

// Check initialization timeout
static bool has_timeout_expired(uint16_t timeout_ms) {
    return init_timeout_ms >= timeout_ms;
}

// Timer callback for initialization timeouts
void init_timeout_callback(TimerHandle_t timer) {
    switch (init_state)
    {
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
static void process_initialization(void)
{
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
        // Log timeout error
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
        pdMS_TO_TICKS(100),
        pdTRUE,
        NULL,
        init_timeout_callback
    );
    
    if (init_timer) {
        xTimerStart(init_timer, 0);
    } else {
        // Log timer creation failure
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
static uint8_t *get_framebuffer(void) {
    return framebuffer;
}

// DMA completion callback
void ili9341_dma_complete(void) {
    HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    dma_control.is_row_completed = true;
    xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
}

// Display driver structure
static const display_driver_t ili9341_driver = {
    .init = ili9341_driver_init,
    .get_framebuffer = get_framebuffer
};

// Get driver instance
const display_driver_t* ili9341_get_driver(void) {
    return &ili9341_driver;
}