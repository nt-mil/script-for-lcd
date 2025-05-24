#ifndef ILI9341_H
#define ILI9341_H

#include "dev_display.h"

#define WIDTH               320
#define HEIGHT              240
#define BIT_PER_PIXEL       4
#define FB_SIZE             (WIDTH * HEIGHT * BIT_PER_PIXEL / 8)
#define BYTE_PER_ROW        (WIDTH * BIT_PER_PIXEL /8)
#define ACTUAL_BYTE_PER_ROW (WIDTH * 2)

typedef enum {
    STATE_NONE = 0,
    STATE_HW_RESET,
    STATE_INITAL_CMD,
    STATE_SLEEP_OUT,
    STATE_INIT_SCREEN,
    STATE_BACKLIGHT,
    STATE_COMPLETED,
} initial_state_t;

typedef enum {
    STATE_READY = 0,
    STATE_INITIALIZING,
    STATE_RUNNING,
    STATE_STOP,
    STATE_STOP_COMPLETED,
} operation_state_t;

typedef enum {
    SEND_COMMAND = 0,
    SEND_DATA
} data_type_t;

void ili9341_control(void);
const display_driver_t* get_ili9341_display_driver(void);

#endif /* ILI9341_H */