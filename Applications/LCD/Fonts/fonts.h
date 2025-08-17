#ifndef FONTS_H
#define FONTS_H

#include "script_types.h"
#include <stdint.h>

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t* data;
} font_def_t;

extern font_def_t font_table[FONT_TYPE_COUNT];

#endif /* FONTS_H */