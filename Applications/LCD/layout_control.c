#include "main.h"
#include "layout_control.h"

extern EventGroupHandle_t display_event;

EventGroupHandle_t layout_control_event = NULL;

static layout_buffer_t layout_data[NUM_LAYOUT_BUFFERS] = {{0}};
static uint8_t latest_index = 0xFF; // Invalid index by default

static void init_event_group(void) {
    layout_control_event = xEventGroupCreate();
    if (layout_control_event == NULL) {
        printf("Failed to create layout control event group\n");
        while (1);
    }
}

static void initialize_layout_buffer(void) {
    for (int i = 0; i < NUM_LAYOUT_BUFFERS; i++) {
        layout_data[i].state = LAYOUT_BUFFER_FREE;
        layout_data[i].data[0] = '\0'; // Ensure null-terminated
        layout_data[i].size = 0;
    }
}

static void init_layout_control(void) {
    init_event_group();
    initialize_layout_binary_info();
    initialize_layout_buffer();
}

void process_layout_script(void) {
    uint8_t current_index = latest_index;

    // Validate index and buffer state
    if (current_index >= NUM_LAYOUT_BUFFERS || layout_data[current_index].data[0] == '\0') {
        return;
    }

    bool process_ok = false;
    portENTER_CRITICAL();
    if (layout_data[current_index].state == LAYOUT_BUFFER_IN_USE) {
        process_ok = true;
    } else if (layout_data[current_index].state == LAYOUT_BUFFER_FREE) {
        layout_data[current_index].state = LAYOUT_BUFFER_IN_USE;
        process_ok = (layout_data[current_index].data[0] != '\0');
    }
    portEXIT_CRITICAL();

    if (process_ok) {
        parse_layout(layout_data[current_index].data, layout_data[current_index].size);
        render_layout();

        portENTER_CRITICAL();
        layout_data[current_index].state = LAYOUT_BUFFER_FREE;
        portEXIT_CRITICAL();
        xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);
    }
}

void set_layout_data(uint8_t* data, uint16_t size) {
    // Check input
    if (!data || size == 0 || data[0] == '\0') {
        return;
    }

    uint8_t current_index = 0xFF; // Invalid index initialization

    for (int i = 0; i < NUM_LAYOUT_BUFFERS; i++) {
        if (layout_data[i].state == LAYOUT_BUFFER_FREE) {
            portENTER_CRITICAL();
            if (layout_data[i].state == LAYOUT_BUFFER_FREE) {
                layout_data[i].state = LAYOUT_BUFFER_WRITING;
                current_index = i;
                break;
            }
            portEXIT_CRITICAL();
        }
    }

    if (current_index == 0xFF) {
        return; // No free buffer available
    }

    uint8_t* dst = layout_data[current_index].data;
    size_t data_size = (size > MAX_LAYOUT_DATA_SIZE) ? MAX_LAYOUT_DATA_SIZE : size;

    memcpy(dst, data, data_size);
    dst[data_size] = '\0';

    portENTER_CRITICAL();
    layout_data[current_index].state = LAYOUT_BUFFER_IN_USE; // Transition to in use after writing
    portEXIT_CRITICAL();

    latest_index = current_index;

    xEventGroupSetBits(layout_control_event, LAYOUT_CONTROL_UPDATE);
}

void control_layout_task(void* param) {
    (void)param;

    init_layout_control();

    for (;;) {
        EventBits_t events = xEventGroupWaitBits(
            layout_control_event,
            LAYOUT_CONTROL_UPDATE, // Event to wait for
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for any bit
            portMAX_DELAY
        );

        if (events & LAYOUT_CONTROL_UPDATE) {
            process_layout_script();
        }
    }
}