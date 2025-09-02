#include "main.h"
#include "trigger.h"

#define MAX_TASKS   8

static sched_entry_t sched_table[MAX_TASKS];
static uint32_t sched_count = 0;

sched_entry_t* register_scheduler(TaskFunction_t task_func,
                                  const char *name,
                                  uint16_t stack_size,
                                  void *param,
                                  UBaseType_t priority,
                                  EventBits_t bits,
                                  uint32_t period_ms)
{
    if (sched_count >= MAX_TASKS) {
        return NULL; // bảng đầy
    }

    sched_entry_t *entry = &sched_table[sched_count];

    // Create event group for the task
    entry->event_info.event_type = xEventGroupCreate();
    if (entry->event_info.event_type == NULL) {
        return NULL; // lỗi cấp phát
    }

    entry->event_info.trigger_bit = bits;

    // Tạo task
    if (xTaskCreate(task_func, name, stack_size, (void*)entry, priority, &entry->handle) != pdPASS) {
        vEventGroupDelete(entry->event_info.event_type);
        return NULL; // lỗi tạo task
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
                xEventGroupSetBits(entry->event_info.event_type, entry->event_info.trigger_bit);
            }
        }
    }
}
