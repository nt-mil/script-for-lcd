#ifndef DISPLAY_INTERFACE_H
#define DISPLAY_INTERFACE_H

#include "dev_display.h"

// Display event flags
typedef enum {
    DISPLAY_EVENT_UPDATE = (1 << 0), // Trigger display update
    DISPLAY_EVENT_PERIOD = (1 << 1), // Periodic display event
} display_event_t;

// Display interface structure
typedef struct {
    const display_driver_t* driver; // Pointer to the display driver
    void (*update)(void);          // Function to handle display updates
} display_interface_t;

// Function prototypes
void display_init(void);
void display_task(void *param);
uint16_t get_display_data_bank_index(void);

#endif /* DISPLAY_INTERFACE_H */