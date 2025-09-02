#ifndef LAYOUT_CONTROL_H
#define LAYOUT_CONTROL_H

#include "script_types.h"
#include "layout_parser.h"
#include "layout_renderer.h"

#define NUM_LAYOUT_BUFFERS (2)

typedef enum {
    LAYOUT_CONTROL_UPDATE = (1 << 0),
} layout_control_event_t;

void process_layout_script(void);

#endif /* LAYOUT_CONTROL_H */