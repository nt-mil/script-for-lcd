#pragma once
#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)

typedef struct {
    uint16_t x;
    uint16_t y;
} point_t;

typedef struct {
    point_t start; // top-left
    point_t end; // bottom-right (exclusive)
} area_t;

typedef enum {
    FONT_S = 0,
    FONT_M,
    FONT_L,
} font_type_t;

typedef struct {
    uint16_t x_pos, y_pos;
    uint16_t width, height;
    uint16_t color, bg_color;
} default_info_t;

/* ------ String wrapper ------ */
typedef struct {
    uint8_t* data_ptr;
    size_t length;
} string_buffer_t;

/* ------ Alignment ------ */
typedef enum {
    ALIGN_NONE = 0,
    ALIGN_CENTER,
    ALIGN_LEFT,
    ALIGN_RIGHT
} alignment_type_t;

/* ------ Layout Entry Table ------ */
typedef struct {
    uint32_t hash_id;
    uint16_t offset;
    uint16_t size;
    uint8_t area_count;
    uint8_t placeholder_count;
} layout_info_entry_t;

/* ------ Placeholder Info Table ------ */
typedef struct {
    uint8_t length;
    uint8_t name[50];
    uint16_t offset;
} placeholder_info_table_t;

/* ------ Layout Content Wrapper ------ */
typedef struct {
    string_buffer_t* content;
} layout_data_t;

/* ------ Text Properties ------ */
typedef struct {
    string_buffer_t* text;
    uint8_t font_id;
    alignment_type_t align;
} text_properties_t;

/* ------ Layout Render Region ------ */
typedef struct {
    uint32_t hash_id;
    uint8_t x_pos;
    uint8_t y_pos;
    uint8_t width;
    uint8_t height;
    uint16_t color;
    uint16_t bg_color;
    text_properties_t text;
} layout_structure_t;

#pragma pack(pop)