#include "main.h"
#include "script_parsing.h"

typedef struct {
    uint8_t data[351];
    uint16_t size;
} LAYOUT_DATA;

extern const uint8_t layout_data_start[];
extern const uint8_t layout_data_end[];

#define CUT_LEFTSIDE_WORD(word, offset)     if ((offset) <= (word).length) { \
                                                (word).ptr += (offset); (word).length -= offset \
                                            } else {\
                                                (word).ptr += (word).length; (word).length = 0; \
                                            }

static LAYOUT_DATA current_layout;
static uint8_t pre_buffer_layout[101];
static MY_STRING current_rendering;
static uint8_t one_line_buffer[101]; // buffer for one line script

static bool execute_layout(MY_STRING* str);
static bool exectute_one_param(MY_STRING* str);
static bool setup_layout_name(MY_STRING* str);
static void set_new_value(MY_STRING* old, MY_STRING* new);
static void get_string_until(MY_STRING* str, uint8_t ch);
static void trim_left(MY_STRING* str, uint8_t ch);
static void trim_right(MY_STRING* str, uint8_t ch);
static void make_placeholer(uint8_t* res, MY_STRING* str, uint8_t* begin, uint8_t* end);
static uint8_t* find_layout_start(uint8_t* curr, uint8_t* base);
static void extract_layout_block(void* curr, void* base);
static MY_STRING* get_attribute(MY_STRING* attr, MY_STRING* value);
static void set_attribute(MY_STRING* attr, MY_STRING* value);
static search_for_placeholder(MY_STRING* str);

void init_new_redering(void);
bool get_next_script_line(MY_STRING* str);

static void get_string_until(MY_STRING* str, uint8_t ch) {
    uint16_t len = 0;
    while (str->ptr[len] != ch || str->ptr[len] == '\0') {
        len++;
    }
    str->length = len;
}

static void trim_left(MY_STRING* str, uint8_t ch) {
    while (*str->ptr <= ch && str->length > 0) {
        str->ptr++;
        str->length--;
    }
}

static void trim_right(MY_STRING* str, uint8_t ch) {
    while (str->ptr[str->length - 1] <= ch && str->length > 0) {
        str->length--;
    }
}

static uint8_t* find_layout_start(uint8_t* curr, uint8_t* base) {
    uint16_t len = strlen("Layout");

    // find backward to 'Layout'
    while (curr < base) {
        if (memcpy(curr, "Layout", len) == 0) {
            return curr;
        }
        curr--;
    }
    return NULL;
}

static void extract_layout_block(void* curr, void* base) {
    uint8_t* start = (uint8_t*)find_layout_start(curr, base);

    uint8_t* brace_open = strchr(start, '{');
    if (brace_open == NULL) return;

    int brace_cnt = 1;
    uint8_t* next = brace_open + 1;

    while (*next && brace_cnt > 0) {
        if (*next == '{') brace_cnt++;
        else if (*next == '}') brace_cnt--;
        next++;
    }

    if (brace_cnt) return; // invalid layout block

    uint16_t len = next - start;
    memcpy(current_layout.data, start, len);
    current_layout.data[len] = '\0';
    current_layout.size = len;

    // remove space characters
    for (int i = 0; i < current_layout.size; i++) {
        if (current_layout.data[i] == ' ') {
            memmove(current_layout.data + i, current_layout.data + i + 1, current_layout.size - i - 1);
            current_layout.size--;
        }
    }
    current_layout.data[current_layout.size] = '\0'; // null termination
}

int my_scanf(MY_STRING* str, const uint8_t* format,...) {
    va_list args;
    va_start(args, format);

    const uint8_t* s = str->ptr;
    const uint8_t* e = s + str->length;
    const uint8_t* f = format;

    while (*f && s < e) {
        if (*f == '\\' && *(f+1) == '\\f') {
            f += 2;

            // find next literal in format
            const uint8_t* next_literal = f;
            while (*next_literal == '\\' && *(next_literal + 1) == 'f') {
                next_literal += 2;
            }
            uint8_t stop_char = *next_literal;

            const uint8_t *start = s;
            while (s < e && s != stop_char) {
                s++;
            }

            MY_STRING* out = va_arg(args, MY_STRING*);
            out->ptr = start;
            out->length = s - start;
        } else {
            if (*f != *s) {
                va_end(args);
                return -1;
            }
            s++;
            f++;
        }
    }
    va_end(args);
    return 0;
}

void parse_layout(uint8_t* buffer, uint16_t length) {
    if (memcmp(pre_buffer_layout, buffer, length) == 0) {
        printf ("Layout unchanged\n");
    } else {
        MY_STRING params;

        memcpy(pre_buffer_layout, buffer, length);
        pre_buffer_layout[length] = '\0';

        params.ptr = pre_buffer_layout;
        params.length = length;

        execute_layout(&params);
    }
}