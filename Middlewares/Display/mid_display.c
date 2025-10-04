#include "main.h"
#include "mid_display.h"
#include "ili9341.h"
#include <string.h>

static EventGroupHandle_t display_event = NULL;

static uint16_t databank_index = 0xFFFE;
static display_interface_t display_interface;

static void init_event_group(void) {
    display_event = get_event_group();
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

    init_events();

    // Trigger initial display update
    set_event(DISPLAY_EVENT_UPDATE);
}

// Display task to handle updates
void display_task(void *param) {
    sched_entry_t* entry = (sched_entry_t*)param;

    for (;;) {
        // Wait for display update events
        EventBits_t events = xEventGroupWaitBits(
            entry->event_info.event_type,
            entry->event_info.trigger_bit | entry->event_info.periodic_bit,
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for any bit
            portMAX_DELAY
        );

        // Process periodic event
        if (events & entry->event_info.periodic_bit) {
            if (display_interface.update) {
                display_interface.update();
            } else {
                // Log error: update function not set
            }
        }

         // Process update event
        if (events & entry->event_info.trigger_bit) {
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