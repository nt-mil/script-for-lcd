#include "main.h"
#include "script_types.h"
#include "layout_parser.h"

/* ----------------- Static Variables --------------------- */
// Buffer to store last parsed layout command (to avoid re-rendering same layout)
static uint8_t last_layout_buffer[MAX_BUFFER_LEN];

// Total size of script content (layout data)
static uint32_t script_content_size;

// Number of layout entries found in script
static uint32_t layout_entry_count;

// Pointer to layout data (content section)
static const uint8_t* layout_content_start;

// Pointer to layout entry table (after content section)
static const uint8_t* layout_entry_base;

// Pointer to placeholder entry table (after layout entries)
static const layout_info_entry_t* layout_info_table;
// static const placeholder_info_table_t* placeholder_info_table;

// Defaut script's values
static default_info_t root_info;

// Current layout content
static char prepared_layout[RENDERED_LAYOUT_MAX_SIZE];

/* ----------------- Function Declarations --------------------- */
static void execute_layout(string_buffer_t* str);
static bool extract_layout_id(string_buffer_t* buffer, string_buffer_t* layout_id_out);
static uint8_t extract_placeholders(string_buffer_t* buffer, placeholder_pair_t* pairs, int max_pairs);
static void replace_placeholders(placeholder_pair_t* pairs, uint8_t pair_count);
static void extract_root_info(void);
static bool extract_layout_content(string_buffer_t* id);

/* ----------------- Function Implementation --------------------- */

uint32_t djb2_hash(const char* str, size_t len) {
    uint32_t hash = 5381;

    while (str && len--) {
        // hash * 33 + current character (cast to uint8_t to ensure 0–255 range)
        hash = ((hash << 5) + hash) + (uint8_t)(*str);
        str++;
    }

    return hash;
}

void* memmem(const void* haystack, size_t hlen, const void* needle, size_t nlen) {
    const uint8_t* h = (const uint8_t*)haystack;
    const uint8_t* n = (const uint8_t*)needle;

    if (!hlen || hlen < nlen) {
        return NULL;
    }

    for (size_t i = 0; i <= hlen - nlen; i++) {
        if (memcmp(h + i, n, nlen) == 0) {
            return (void*)(h + i);
        }
    }

    return NULL;
}

void string_replace_all(char* buffer, size_t buf_size, const char* find, const char* replace) {
    char temp[RENDERED_LAYOUT_MAX_SIZE];
    char* p = buffer;
    size_t find_len = strlen(find);
    size_t replace_len = strlen(replace);

    temp[0] = '\0';

    while (*p && (strlen(temp) + replace_len < buf_size - 1)) {
        char* pos = strstr(p, find);
        if (!pos) {
            strncat(temp, p, buf_size - strlen(temp) - 1);
            break;
        }

        strncat(temp, p, pos - p);
        strncat(temp, replace, buf_size - strlen(temp) - 1);
        p = pos + find_len;
    }

    strncpy(buffer, temp, buf_size);
}

void initialize_layout_binary_info(void) {
    // Read script size and layout count from header (8 bytes total)
    script_content_size = *((uint32_t*)SCRIPT_DATA_BASE);         // First 4 bytes
    layout_entry_count  = *((uint32_t*)(SCRIPT_DATA_BASE + 4));   // Next 4 bytes

    if (script_content_size == 0 || layout_entry_count == 0)
        return;

    // Align script content size to 4 bytes
    uint32_t aligned_script_size = (script_content_size + 3) & ~0x03;

    // Set layout content and table pointers
    layout_content_start = (uint8_t*)(SCRIPT_DATA_BASE + SCRIPT_HEADER_SIZE);
    layout_entry_base    = SCRIPT_DATA_BASE + SCRIPT_HEADER_SIZE + aligned_script_size;
    layout_info_table    = (const layout_info_entry_t*)layout_entry_base;

    // Calculate total size: header + script (aligned) + layout table
    uint32_t total_size = SCRIPT_HEADER_SIZE
                        + aligned_script_size
                        + layout_entry_count * sizeof(layout_info_entry_t);

    // Check for overflow beyond allocated memory
    if (total_size > *layout_data_size) {
        // Error: malformed binary
        return;
    }

    // Proceed to extract layout root information
    extract_root_info();
}

void parse_layout(uint8_t* buffer, uint16_t length) {
    // Skip if layout is the same as last time
    if (memcmp(last_layout_buffer, buffer, length) == 0) {
        return;
    }

    // Save current layout
    memcpy(last_layout_buffer, buffer, length);
    last_layout_buffer[length] = '\0';

    string_buffer_t str = {
        .data_ptr = last_layout_buffer,
        .length = length
    };

    // Process layout
    execute_layout(&str);
}

bool get_next_script_line(string_buffer_t* script, string_buffer_t* line_out) {
    static size_t script_offset = 0;

    // Reset output
    line_out->data_ptr = NULL;
    line_out->length = 0;

    if (!script || !script->data_ptr || script->length == 0 || script_offset >= script->length) {
        script_offset = 0;  // reset for next layout
        return false;
    }

    uint8_t* base = script->data_ptr + script_offset;
    size_t remaining = script->length - script_offset;
    size_t i = 0;

    // Find end of line (by newline or null terminator)
    while (i < remaining && base[i] != '\n' && base[i] != '\0') {
        i++;
    }

    // Skip empty lines
    while (i == 0 && (base[0] == '\n' || base[0] == '\r')) {
        script_offset++;
        if (script_offset >= script->length) {
            return false;
        }
        base = script->data_ptr + script_offset;
        remaining = script->length - script_offset;
        i = 0;
        while (i < remaining && base[i] != '\n' && base[i] != '\0') {
            i++;
        }
    }

    // Output line
    line_out->data_ptr = base;
    line_out->length = i;

    // Advance offset (skip \n if present)
    script_offset += (i < remaining && base[i] == '\n') ? i + 1 : i;

    return true;
}

uint8_t* get_prepared_layout(void) {
    return  (uint8_t*)prepared_layout;
}

default_info_t* get_root_info(void) {
    return &root_info;
}

// Helper: parse int16_t field from root content
int16_t parse_field_u16(const uint8_t* content, const char* key) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "%s:", key);

    const char* found = strstr((const char*)content, pattern);
    if (!found) return -1;

    uint16_t val = 0;
    sscanf(found + strlen(pattern), "%hi", &val);
    return val;
}

// Helper: parse uint8_t field
uint8_t parse_field_u8(const uint8_t* content, const char* key) {
    return (uint8_t)parse_field_u16(content, key);
}

int parse_field(const uint8_t* content, const char* key, void* value, size_t value_size, field_type_t type) {
    if (!content || !key || !value) {
        return -1; // Invalid input
    }

    char pattern[32];
    if (snprintf(pattern, sizeof(pattern), "%s:", key) >= (int)sizeof(pattern)) {
        return -1; // Key too long
    }

    const char* found = strstr((const char*)content, pattern);
    if (!found) return 0; // Key not found

    found += strlen(pattern); // Move past the pattern
    int result = 0;

    switch (type) {
        case FIELD_TYPE_STRING:
            if (value_size < 1) return -1; // Ensure space for null terminator
            result = sscanf(found, " %[^\n\r]", (char*)value);
            ((char*)value)[value_size - 1] = '\0'; // Ensure null termination
            break;
        case FIELD_TYPE_UINT8:
            if (value_size != sizeof(uint8_t)) return -1;
            {
                uint8_t val = 0;
                result = sscanf(found, " %hhu", &val);
                if (result > 0) *(uint8_t*)value = val;
            }
            break;
        case FIELD_TYPE_UINT16:
            if (value_size != sizeof(uint16_t)) return -1;
            {
                uint16_t val = 0;
                result = sscanf(found, " %hu", &val);
                if (result > 0) *(uint16_t*)value = val;
            }
            break;
        default:
            return -1; // Unsupported type
    }

    return (result > 0) ? 1 : 0; // Success if parsed
}

// Helper: convert 16-bit value to RGB565
uint16_t swap_byte(uint16_t value) {
    return (value >> 8) | (value << 8);
}

static void execute_layout(string_buffer_t* str) {
    string_buffer_t layout_id;
    placeholder_pair_t pairs[MAX_PLACEHOLDERS];

    if (!extract_layout_id(str, &layout_id)) {
        printf("Layout ID invalid!!!\n");
    }

    if (!extract_layout_content(&layout_id)) {
        printf("Layout content not found!!!\n");
    }

    uint8_t pair_cnt = extract_placeholders(str, pairs, MAX_PLACEHOLDERS);

    replace_placeholders(pairs, pair_cnt);
}

static void extract_root_info(void) {
    if (layout_info_table == NULL) {
        return;
    }

    layout_info_entry_t root_entry = layout_info_table[0];
    const uint8_t* root_content = layout_content_start + root_entry.offset;

    root_info.x_pos       = parse_field_u16(root_content, "x");
    root_info.y_pos       = parse_field_u16(root_content, "y");

    root_info.width       = parse_field_u16(root_content, "width");
    root_info.height      = parse_field_u16(root_content, "height");

    root_info.color       = parse_field_u16(root_content, "color");
    root_info.bg_color    = parse_field_u16(root_content, "background");

    // Assign default value to driver layer
    // 1. Get driver info (Driver pointer)
    uint16_t index = get_display_data_bank_index();
    display_info_t* display_info = (display_info_t*)read_from_databank(index);

    display_info->fg_color = (root_info.color == -1) ? DEFAULT_FG_COLOR : swap_byte(root_info.color);
    display_info->bg_color = (root_info.color == -1) ? DEFAULT_BG_COLOR : swap_byte(root_info.bg_color);
}

static bool extract_layout_id(string_buffer_t* buffer, string_buffer_t* layout_id_out) {
    const char* id_key = "$id:";
    uint8_t* start = (uint8_t*)strstr((char*)buffer->data_ptr, id_key);
    if (!start) return false;

    start += strlen(id_key);  // skip "$id:"

    uint8_t* end = (uint8_t*)strchr((char*)start, ';');
    if (!end) end = buffer->data_ptr + buffer->length; // if not ';', get the the whole input 'til the '\0'

    layout_id_out->data_ptr = start;
    layout_id_out->length = end - start;

    return true;
}

static uint8_t extract_placeholders(string_buffer_t* buffer, placeholder_pair_t* pairs, int max_pairs) {
    uint8_t count = 0;
    uint8_t* ptr = buffer->data_ptr;
    uint8_t* end = buffer->data_ptr + buffer->length - 1;

    while (ptr < end && count < max_pairs) {
        if (*ptr == '$') {
            uint8_t* name_start = ptr + 1;
            uint8_t* colon = (uint8_t*)strchr((char*)name_start, ':');
            if (!colon || colon >= end) break;

            uint8_t* value_start = colon + 1;
            uint8_t* semi = (uint8_t*)strchr((char*)value_start, ';');
            if (!semi || semi >= end) semi = end;

            size_t name_len = colon - name_start;
            size_t value_len = semi - value_start;

            // skip id field
            if (name_len == 2 && strncmp((char*)name_start, "id", 2) == 0) {
                ptr = (*semi == ';') ? semi + 1 : semi;
                continue;
            }

            pairs[count].name.data_ptr = name_start;
            pairs[count].name.length = name_len;

            pairs[count].value.data_ptr = value_start;
            pairs[count].value.length = value_len;

            count++;
            ptr = (*semi == ';') ? semi + 1 : semi;
        } else {
            ptr++;
        }
    }

    return count;
}

static bool extract_layout_content(string_buffer_t* id) {
    layout_info_entry_t* found = NULL;
    uint32_t hash = djb2_hash((const char*)id->data_ptr, id->length);

    for (uint8_t i = 0; i < layout_entry_count; i++) {
        if (layout_info_table[i].hash_id == hash) {
            found = (layout_info_entry_t*)&layout_info_table[i];
            break;
        }
    }

    if (!found) {
        printf("Layout not found\n");
        return false;
    }

    const uint8_t* layout_src = layout_content_start + found->offset;
    uint16_t layout_size = found->size;

    memcpy(prepared_layout, layout_src, layout_size);
    prepared_layout[layout_size] = '\0';

    return true;
}

static void replace_placeholders(placeholder_pair_t* pairs, uint8_t pair_count) {
    for (uint8_t i = 0; i < pair_count; i++) {
        char placeholder_name[64] = {0,};
        snprintf(placeholder_name, sizeof(placeholder_name), "$%.*s", (int)pairs[i].name.length, pairs[i].name.data_ptr);

        char replacement[64] = {0, };
        snprintf(replacement, sizeof(replacement), "%.*s", (int)pairs[i].value.length, pairs[i].value.data_ptr);
    
        string_replace_all(prepared_layout, RENDERED_LAYOUT_MAX_SIZE, placeholder_name, replacement);
    }
}