#ifndef ILI9341_H
#define ILI9341_H

#include "dev_display.h"

// Display dimensions and buffer configuration
#define ILI9341_WIDTH                   320
#define ILI9341_HEIGHT                  240
#define ILI9341_BITS_PER_PIXEL          4
#define ILI9341_FRAMEBUFFER_SIZE        (ILI9341_WIDTH * ILI9341_HEIGHT * ILI9341_BITS_PER_PIXEL / 8)
#define ILI9341_BYTES_PER_ROW           (ILI9341_WIDTH * ILI9341_BITS_PER_PIXEL /8)
#define ILI9341_ACTUAL_BYTES_PER_ROW    (ILI9341_WIDTH * 2)

// Initialization states for display setup
typedef enum {
    ILI9341_INIT_NONE = 0,
    ILI9341_INIT_HW_RESET,
    ILI9341_INIT_COMMANDS,
    ILI9341_INIT_SLEEP_OUT,
    ILI9341_INIT_SCREEN,
    ILI9341_INIT_BACKLIGHT,
    ILI9341_INIT_COMPLETED,
} ili9341_init_state_t;

// Operational states for display control
typedef enum {
    ILI9341_STATE_READY = 0,
    ILI9341_STATE_INITIALIZING,
    ILI9341_STATE_RUNNING,
    ILI9341_STATE_STOPPING,
    ILI9341_STATE_STOPPED,
} ili9341_operation_state_t;

// Data transfer types
typedef enum {
    ILI9341_DATA_COMMAND = 0,
    ILI9341_DATA_PAYLOAD
} ili9341_data_type_t;

// Function prototypes
void ili9341_controller_task(void);
const display_driver_t* ili9341_get_driver(void);

#endif /* ILI9341_H */