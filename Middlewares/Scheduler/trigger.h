#ifndef TRIGGER_H
#define TRIGGER_H

#define MAX_TASKS  8

typedef enum {
    /* Display event flags */
    DISPLAY_EVENT_UPDATE = (1 << 0), // Trigger display update
    DISPLAY_EVENT_PERIOD = (1 << 1), // Periodic display event
} event_flag_t;

typedef struct {
    EventGroupHandle_t event_type;
    EventBits_t trigger_bit;  // A dedicated bit for update events
    EventBits_t periodic_bit; // A dedicated bit for periodic events (used by scheduler)
} trigger_event_t;

typedef struct {
    TaskHandle_t handle;
    uint32_t     period_ms;
    uint32_t     counter;
    trigger_event_t event_info;
} sched_entry_t;

void init_events(void);
void set_event(EventBits_t bits);
EventGroupHandle_t get_event_group(void);

sched_entry_t* register_scheduler(TaskFunction_t task_func,
                                  const char *name,
                                  uint16_t stack_size,
                                  void *param,
                                  UBaseType_t priority,
                                  EventBits_t update_bits,
                                  EventBits_t periodic_bit,
                                  uint32_t period_ms);

void scheduler_task(void *arg);

#endif /* TRIGGER_H */