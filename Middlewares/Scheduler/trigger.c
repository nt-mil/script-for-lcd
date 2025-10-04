#include "main.h"
#include "trigger.h"

static sched_entry_t sched_table[MAX_TASKS];
static uint32_t sched_count = 0;
static EventGroupHandle_t global_event_group = NULL;

void init_events(void) {
    global_event_group = xEventGroupCreate();
    configASSERT(global_event_group != NULL);
}

void set_event(EventBits_t bits) {
    xEventGroupSetBits(global_event_group, bits);
}

EventGroupHandle_t get_event_group(void) {
    return global_event_group;
}

sched_entry_t* register_scheduler(TaskFunction_t task_func,
                                  const char *name,
                                  uint16_t stack_size,
                                  void *param,
                                  UBaseType_t priority,
                                  EventBits_t update_bits,
                                  EventBits_t periodic_bit,
                                  uint32_t period_ms)
{
    if (sched_count >= MAX_TASKS) {
        return NULL;
    }

    sched_entry_t *entry = &sched_table[sched_count];

    // Create event group for the task
    entry->event_info.event_type = xEventGroupCreate();
    if (entry->event_info.event_type == NULL) {
        printf("Cannot create event group!\n");
        return NULL;
    }

    entry->event_info.trigger_bit = update_bits;
    entry->event_info.periodic_bit = periodic_bit;

    // Tạo task
    if (xTaskCreate(task_func, name, stack_size, (void*)entry, priority, &entry->handle) != pdPASS) {
        vEventGroupDelete(entry->event_info.event_type);
        return NULL;
    }

    entry->period_ms = period_ms;
    entry->counter   = 0;

    sched_count++;
    return entry;
}

void scheduler_task(void *arg) {
    (void)arg;
    const TickType_t tick_ms = pdMS_TO_TICKS(1);

    while (1) {
        vTaskDelay(tick_ms);

        for (int i = 0; i < sched_count; i++) {
            sched_entry_t *entry = &sched_table[i];

            entry->counter++;
            if (entry->counter >= entry->period_ms) {
                entry->counter = 0;
                xEventGroupSetBits(entry->event_info.event_type, entry->event_info.periodic_bit);
            }
        }
    }
}
