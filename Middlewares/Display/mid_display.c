#include "main.h"
#include "mid_display.h"

EventGroupHandle_t display_event;

static display_interface_t interface;

/**
 * @brief User Event initialization
 * @retval None
*/
static void event_init(void)
{
    display_event = xEventGroupCreate();
}

void display_init(void)
{
    const display_driver_t* display_driver;
    display_driver = get_ili9341_display_driver();

    interface.display_driver = display_driver;
    interface.update = ili9341_control;

    interface.display_driver->init();
    event_init();
}

void display_task(void* param)
{
    EventBits_t bit_is_waiting_for;
    EventBits_t bit_pending;

    bit_is_waiting_for = DISPLAY_EVENT_UPDATE;

    for (;;)
    {
        bit_pending = xEventGroupWaitBits(display_event, bit_is_waiting_for, pdTRUE, pdFALSE, pdMS_TO_TICKS(portMAX_DELAY));

        if (bit_pending & DISPLAY_EVENT_UPDATE)
        {
            interface.update();
        }
    }
}