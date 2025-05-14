#ifndef DISPLAY_INTERFACE_H
#define DISPLAY_INTERFACE_H

enum
{
    DISPLAY_EVENT_UPDATE = 1,
    DISPLAY_EVENT_PERIOD,
};

typedef struct
{
    const display_driver_t* display_driver;
    void (*update)(void);
} display_interface_t;

void display_init(void);

void display_task(void *param);

#endif /* DISPLAY_INTERFACE_H */