#ifndef TRIGGER_H
#define TRIGGER_H

#define MAX_TASKS  8

typedef struct {
    EventGroupHandle_t event_type;
    EventBits_t trigger_bit;
} trigger_event_t;

typedef struct {
    TaskHandle_t handle;
    uint32_t     period_ms;
    uint32_t     counter;
    trigger_event_t event_info;
} sched_entry_t;

sched_entry_t* register_scheduler(TaskFunction_t task_func,
                                  const char *name,
                                  uint16_t stack_size,
                                  void *param,
                                  UBaseType_t priority,
                                  EventBits_t bits,
                                  uint32_t period_ms);

void scheduler_task(void *arg);

#endif /* TRIGGER_H */