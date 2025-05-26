#include "main.h"
#include "mid_display.h"
#include "ili9341.h"
#include <string.h>

// FreeRTOS event group for display events
EventGroupHandle_t display_event;

// Static display interface instance
static display_interface_t display_interface;

// Initialize FreeRTOS event group for display events
static void init_event_group(void) {
    display_event = xEventGroupCreate();
    if (display_event == NULL) {
        // Log error (assuming a logging mechanism exists)
        // Handle failure, e.g., halt or retry
    }
}

// Initialize the display interface
void display_init(void) {
    // Get ILI9341 driver
    const display_driver_t* driver = ili9341_get_driver();
    if (driver == NULL) {
        // Log error: driver not found
        return;
    }

    // Configure display interface
    display_interface.driver = driver;
    display_interface.update = ili9341_controller_task;

    // Initialize the driver
    if (display_interface.driver->init) {
        display_interface.driver->init();
    } else {
        // Log error: driver init function missing
    }

    // Initialize event group
    init_event_group();

    // Trigger initial display update
    xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
}

// Display task to handle updates
void display_task(void *param) {
    (void)param; // Unused parameter
    EventBits_t events_to_wait = DISPLAY_EVENT_UPDATE;

    for (;;) {
        // Wait for display update events
        EventBits_t events = xEventGroupWaitBits(
            display_event,
            events_to_wait,
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for any bit
            portMAX_DELAY
        );

        // Process update event
        if (events & DISPLAY_EVENT_UPDATE) {
            if (display_interface.update) {
                display_interface.update();
            } else {
                // Log error: update function not set
            }
        }
    }
}