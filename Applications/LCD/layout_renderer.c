#include "main.h"
#include "script_types.h"
#include "layout_parser.h"
#include "layout_renderer.h"
#include "fonts.h"

#define MAX_LINES 10
#define MAX_CHAR_PER_LINE 50

static ALIGN align;
static AREA area;
static uint8_t* rendering_layout;
static string_buffer_t script;
static string_buffer_t line;
static uint8_t total, current;
static uint16_t x_pos, y_pos;
static uint16_t width, height, color, bg_color;
static char text[50];
static font_type_t font;

static const char* line_starts[MAX_LINES];
static size_t line_lengths[MAX_LINES];

// Define field mappings
static const field_mapping_t field_mappings[] = {
    {"x", &x_pos, sizeof(uint16_t), FIELD_TYPE_UINT16},
    {"y", &y_pos, sizeof(uint16_t), FIELD_TYPE_UINT16},
    {"width", &width, sizeof(uint16_t), FIELD_TYPE_UINT16},
    {"height", &height, sizeof(uint16_t), FIELD_TYPE_UINT16},
    {"color", &color, sizeof(uint16_t), FIELD_TYPE_UINT16},
    {"background", &bg_color, sizeof(uint16_t), FIELD_TYPE_UINT16},
    {"text", text, sizeof(text), FIELD_TYPE_STRING},
    {"align", &align.alignment, sizeof(uint8_t), FIELD_TYPE_UINT8},
    {"font", &font, sizeof(uint8_t), FIELD_TYPE_UINT8},
    {"align", &total, sizeof(uint8_t), FIELD_TYPE_UINT8},
    {"align", &current, sizeof(uint8_t), FIELD_TYPE_UINT8},
};

static const size_t num_mappings = sizeof(field_mappings) / sizeof(field_mapping_t);

static bool script_ready = false;

static void draw_char_1ppb(uint8_t* framebuffer, int x, int y, char character, 
                          uint16_t font_width, uint16_t font_height, const uint16_t* font_data) {
    // Validate inputs
    if (!framebuffer || !font_data || font_width == 0 || font_height == 0 || 
        character < 32 || character > 126) {
        return;
    }

    // Calculate character index in font data
    const int char_index = (character - 32) * font_height;


    // Iterate over each row of the character
    for (int row = 0; row < font_height; ++row) {
        uint16_t row_bitmap = font_data[char_index + row];

        // Iterate over each column of the character
        for (int col = 0; col < font_width; ++col) {
            // Extract bit from right to left (MSB at col 0)
            uint8_t pixel_bit = (row_bitmap >> (15 - col)) & 0x01;

            // Calculate absolute pixel coordinates
            int pixel_x = x + col;
            int pixel_y = y + row;

            // Check bounds
            if (pixel_x < 0 || pixel_x >= ILI9341_WIDTH || 
                pixel_y < 0 || pixel_y >= ILI9341_HEIGHT) {
                continue;
            }

            // Calculate framebuffer byte and bit offset
            const int byte_offset = (pixel_y * ILI9341_WIDTH + pixel_x) / 8;
            const int bit_shift = 7 - (pixel_x % 8);

            // Set or clear pixel based on bitmap
            if (pixel_bit) {
                framebuffer[byte_offset] |= (1 << bit_shift);
            } else {
                framebuffer[byte_offset] &= ~(1 << bit_shift);
            }
        }
    }
}

// Check if string fits without wrapping
static bool is_fit_screen_size(const char* str, const font_def_t* font_info, int spacing) {
    if (!str || !font_info || !font_info->data) return false;
    size_t length = strlen(str);
    uint16_t total_width = (length * font_info->width) + ((length - 1) * spacing);
    return total_width <= ILI9341_WIDTH;
}

// Compute line breaks and store in static arrays
static uint16_t compute_line_breaks(const char* str, const font_def_t* font_info, int spacing,
                                   uint16_t* max_line_width) {
    const char* start = str;
    uint16_t line_count = 0;
    *max_line_width = 0;

    while (*start && line_count < MAX_LINES) {
        uint16_t line_width = 0;
        const char* end = start;
        const char* last_space = NULL;
        size_t chars_in_line = 0;

        while (*end && line_width < ILI9341_WIDTH) {
            if (*end == ' ') last_space = end;
            line_width += font_info->width + (chars_in_line > 0 ? spacing : 0);
            if (line_width > ILI9341_WIDTH && last_space) {
                end = last_space;
                break;
            }
            ++end;
            ++chars_in_line;
        }

        size_t segment_length = (end - start);
        if (line_width > ILI9341_WIDTH && last_space) {
            segment_length = last_space - start;
        } else if (!*end) {
            segment_length = strlen(start);
        }

        // Recalculate line_width based on the actual segment length
        line_width = (segment_length * font_info->width) + ((segment_length > 1) ? ((segment_length - 1) * spacing) : 0);

        line_starts[line_count] = start;
        line_lengths[line_count] = segment_length;

        if (line_width > *max_line_width) *max_line_width = line_width;
        ++line_count;

        start = (*end && last_space) ? end + 1 : end;
    }
    return line_count;
}

// Calculate the aligned base position for the text block
static void calculate_block_position(uint16_t line_count, uint16_t max_line_width,
                                    uint16_t font_height, uint16_t* base_x, uint16_t* base_y) {
    *base_x = (uint16_t)x_pos;
    *base_y = (uint16_t)y_pos;
    uint16_t total_height = line_count * font_height;

    if (align.alignment != ALIGN_NONE) {
        if (align.alignment == ALIGN_CENTER) {
            *base_x = (uint16_t)(x_pos + ((width - max_line_width) >> 1));
        } else if (align.alignment == ALIGN_RIGHT) {
            *base_x = (uint16_t)(x_pos + (width - max_line_width));
        }

        *base_y = (uint16_t)(y_pos + ((height - total_height) >> 1)); // Auto apply vertical alignment

        // Clamp to valid range
        if (*base_x >= ILI9341_WIDTH) *base_x = ILI9341_WIDTH - 1;
        if (*base_y >= ILI9341_HEIGHT) *base_y = ILI9341_HEIGHT - 1;
    }
}

// Draw a single line with alignment
static void draw_one_line(const char* segment, uint16_t draw_x, uint16_t draw_y,
                         const font_def_t* font_info, int spacing,
                         const display_info_t* display_info) {
    uint16_t font_width = font_info->width;
    uint16_t draw_pos_x = draw_x;

    for (const char* p = segment; *p; ++p) {
        if (*p < 32 || *p > 126) continue; // Skip non-printable

        draw_char_1ppb(display_info->data, draw_pos_x, draw_y, *p, font_info->width, font_info->height, font_info->data);
        draw_pos_x += font_width + spacing;

        if (draw_pos_x >= ILI9341_WIDTH) {
            break;
        }
    }
}

// void draw_string_aligned(uint8_t *fb, int fb_width, int x, int y,
//                          const char *str, int len, TextAlign align,
//                          int layout_width) {
//     int text_width = get_string_width(str, len);

//     int start_x = x;
//     if (align == ALIGN_CENTER) {
//         start_x = x + (layout_width - text_width) / 2;
//     } else if (align == ALIGN_RIGHT) {
//         start_x = x + (layout_width - text_width);
//     }

//     int cursor_x = start_x;
//     for (int i = 0; i < len; i++) {
//         int w = get_char_width(str[i]);
//         draw_char(fb, fb_width, cursor_x, y, str[i]);
//         cursor_x += w;
//     }
// }

// Main function to draw multi-line string
static void draw_string(const char* str, int spacing) {
    // Validate inputs
    if (!str || font >= FONT_TYPE_COUNT || spacing < 0) {
        return;
    }

    // Retrieve font definition
    const font_def_t* font_info = &font_table[font];
    if (!font_info || !font_info->data) {
        return;
    }

    // Retrieve display data
    uint16_t bank_index = get_display_data_bank_index();
    const display_info_t* display_info = (display_info_t*)read_from_databank(bank_index);
    if (!display_info || !display_info->data) {
        return;
    }

    // Check if string fits without wrapping
    if (is_fit_screen_size(str, font_info, spacing)) {
        size_t text_length = strlen(str);
        uint16_t text_width = (text_length * font_info->width) + ((text_length - 1) * spacing);
        uint16_t base_x, base_y;
        calculate_block_position(1, text_width, font_info->height, &base_x, &base_y);
        draw_one_line(str, base_x, base_y, font_info, spacing, display_info);
        return;
    }

    // Precompute line breaks
    uint16_t max_line_width;
    uint16_t line_count = compute_line_breaks(str, font_info, spacing, &max_line_width);

    // Calculate block position
    uint16_t base_x, base_y;
    calculate_block_position(line_count, max_line_width, font_info->height, &base_x, &base_y);

    // Draw each line
    for (uint16_t line = 0; line < line_count; ++line) {
        // Extract line segment
        char segment[MAX_CHAR_PER_LINE];
        strncpy(segment, line_starts[line], line_lengths[line]);
        segment[line_lengths[line]] = '\0';

        // Calculate line-specific width
        uint16_t line_pixel_width = (line_lengths[line] * font_info->width) +
                                   ((line_lengths[line] - 1) * spacing);

        // Apply horizontal alignment for this line
        uint16_t draw_x = base_x;
        if (align.alignment == ALIGN_CENTER) {
            draw_x = (uint16_t)(base_x + ((max_line_width - line_pixel_width) >> 1));
        } else if (align.alignment == ALIGN_RIGHT) {
            draw_x = (uint16_t)(base_x + (max_line_width - line_pixel_width));
        }
        draw_x = (draw_x >= ILI9341_WIDTH) ? (ILI9341_WIDTH - 1) : ((draw_x < 0) ? 0 : draw_x);
        uint16_t draw_y = base_y + (line * font_info->height);

        // Draw the current line
        draw_one_line(segment, draw_x, draw_y, font_info, spacing, display_info);

        // Stop if off screen
        if (draw_y >= ILI9341_HEIGHT) break;
    }
}

static void draw_layout(void) {
    if (text[0] == '\0') return;

    size_t text_len = strlen(text);

    if (text_len > sizeof(text) || text_len == 0) return;

    draw_string((const char*)text, 1);
}

bool is_script_ready(void) {
    return script_ready;
}

void set_script_state(bool ready) {
    script_ready = ready;
}

static void init_rendering_layout(void) {
    rendering_layout = get_prepared_layout();

    script.data_ptr = rendering_layout;
    script.length = strlen((const char*)rendering_layout);
}

static void init_layout_info(default_info_t* info) {
    x_pos = info->x_pos;
    y_pos = info->y_pos;

    width = info->width;
    height = info->height;

    color = info->color;
    bg_color = info->bg_color;
}

static void execute_rendering(void) {
    char temp[50] = {0}; // Buffer for line data, capped at 49 chars + null
    printf("%d\n", num_mappings);

     while (get_next_script_line(&script, &line)) {
        if (line.length >= sizeof(temp)) {
            line.length = sizeof(temp) - 1; // Truncate to fit
        }

        strncpy(temp, (const char*)line.data_ptr, line.length);
        temp[line.length] = '\0';

        if (strstr(temp, "id:")) {
            continue;
        }
        else if (strstr(temp, "<START>")) {
            // Layout start
            continue;
        } else if (strstr(temp, "<END>")) {
            // Layout end
            draw_layout();
            continue;
        } else {
            // Parse fields
            for (size_t i = 0; i < num_mappings; i++) {
                char pattern[32];
                snprintf(pattern, sizeof(pattern), "%s:", field_mappings[i].key);
                if (strstr(temp, pattern)) {
                    if (parse_field(line.data_ptr, field_mappings[i].key, field_mappings[i].value,
                                field_mappings[i].size, field_mappings[i].type) != 1) {
                        printf("Failed to parse field: %s\n", field_mappings[i].key);
                        // Continue on failure to attempt other fields
                    }
                    break; // Assume one field per line, move to next line
                }
            }
        }
    }
}

bool render_layout(void) {
    bool res = false;

    default_info_t* default_info = get_root_info();

    init_layout_info(default_info);

    init_rendering_layout();

    execute_rendering();

    return res;
}