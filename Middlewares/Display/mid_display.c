#include "main.h"
#include "mid_display.h"
#include "ili9341.h"
#include <string.h>

EventGroupHandle_t display_event;

static uint16_t databank_index = 0xFFFE;
static display_interface_t display_interface;

static void init_event_group(void) {
    display_event = xEventGroupCreate();
    if (display_event == NULL) {
        // Log error (assuming a logging mechanism exists)
        // Handle failure, e.g., halt or retry
    }
}

static void register_display_info(display_info_t* info) {
    databank_index = write_to_databank((void*)info);
}

void display_init(void) {
    // Get ILI9341 driver
    const display_driver_t* driver = ili9341_get_driver();
    if (driver == NULL) {
        // Log error: driver not found
        return;
    }

    display_interface.driver = driver;
    display_interface.update = ili9341_controller_task;

    if (display_interface.driver->init) {
        display_interface.driver->init();
    } else {
        // Log error: driver init function missing
    }

    display_info_t* display_info = driver->get_framebuffer();
    register_display_info(display_info);

    init_event_group();

    // Trigger initial display update
    // xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
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

uint16_t get_display_data_bank_index(void) {
    return databank_index;
}