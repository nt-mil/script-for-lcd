#include "main.h"
#include "script_types.h"
#include "layout_parser.h"
#include "layout_renderer.h"

static ALIGN align;
static AREA area;
static uint8_t* rendering_layout;
static string_buffer_t script;
static string_buffer_t line;
static uint16_t x_pos, y_pos;
static uint16_t width, height, color, bg_color;

static bool script_ready = false;

bool is_script_ready(void) {
    return script_ready;
}

void set_script_state(bool ready) {
    script_ready = ready;
}

static void init_rendering_layout(void) {
    rendering_layout = get_prepared_layout();

    script.data_ptr = rendering_layout;
    script.length = strlen(rendering_layout);
}

static void init_layout_info(default_info_t* info) {
    x_pos = info->x_pos;
    y_pos = info->y_pos;

    width = info->width;
    height = info->height;

    color = info->color;
    color = info->bg_color;
}

static void execute_rendering(void) {
     while (get_next_script_line(&script, &line)) {
        const char* ptr = (const char*)line.data_ptr;

        if (strchr(ptr, '{')) {

        } else if (strchr(ptr, '}')) {

        } else if (strstr(ptr, "x:")) {
            x_pos = parse_field_u16(line.data_ptr, "x");
        } else if (strstr(ptr, "y:")) {
            y_pos = parse_field_u16(line.data_ptr, "y");
        } else if (strstr(ptr, "width:")) {
            width = parse_field_u16(line.data_ptr, "width");
        } else if (strstr(ptr, "height:")) {
            height = parse_field_u16(line.data_ptr, "height");
        } else if (strstr(ptr, "color:")) {
            color = parse_field_u16(line.data_ptr, "color");
        } else if (strstr(ptr, "background:")) {
            bg_color = parse_field_u16(line.data_ptr, "background");
        } else if (strstr(ptr, "text:")) {
            x_pos = parse_field_u16(line.data_ptr, "x");
        } else if (strstr(ptr, "font:")) {
            x_pos = parse_field_u16(line.data_ptr, "x");
        } else if (strstr(ptr, "align:")) {
            x_pos = parse_field_u16(line.data_ptr, "x");
        } else if (strstr(ptr, "total:")) {
            x_pos = parse_field_u16(line.data_ptr, "x");
        } else if (strstr(ptr, "current:")) {

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