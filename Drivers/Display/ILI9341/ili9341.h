#ifndef H
#define H

#include "dev_display.h"

#define WIDTH               320
#define HEIGHT              240
#define BIT_PER_PIXEL       4
#define FB_SIZE             (WIDTH * HEIGHT * BIT_PER_PIXEL / 8)
#define BYTE_PER_ROW        (WIDTH * BIT_PER_PIXEL /8)

typedef enum {
    STATE_HW_RESET = 0,
    STATE_SLEEP_OUT,
    STATE_INITAL_CMD,
    STATE_COMPLETED,
} initial_state_t;

typedef enum {
    STATE_READY = 0,
    STATE_INITIALIZING,
    STATE_RUNNING,
    STATE_STOP,
} operation_state_t;

typedef enum {
    SEND_COMMAND = 0,
    SEND_DATA
} data_type_t;

void ili9341_control(void);
const display_driver_t* get_ili9341_display_driver(void);

#endif /* H */