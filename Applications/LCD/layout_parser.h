#ifndef _LAYOUT_BINARY_H_
#define _LAYOUT_BINARY_H_

#include <stdint.h>

#define SCRIPT_DATA_BASE        layout_data_start
#define SCRIPT_HEADER_SIZE      8   // 4 bytes for script_size + 4 bytes for layout_count

#define RENDERED_LAYOUT_MAX_SIZE 512
#define MAX_BUFFER_LEN 101
#define MAX_PLACEHOLDERS 10
#define MAX_NAME_LEN     32
#define MAX_VALUE_LEN    32

typedef struct {
    string_buffer_t name;
    string_buffer_t value;
} placeholder_pair_t;

// External layout data from layout.o
extern const uint8_t layout_data_start[];
extern const uint8_t layout_data_end[];
extern const uint16_t layout_data_size[];

// Function prototypes
uint32_t djb2_hash(const char* str, size_t len);
void* memmem(const void* haystack, size_t hlen, const void* needle, size_t nlen);
void string_replace_all(char* buffer, size_t buf_size, const char* find, const char* replace);
void initialize_layout_binary_info(void);
void parse_layout(uint8_t* str, uint16_t length);
bool get_next_script_line(string_buffer_t* script, string_buffer_t* line_out);
uint8_t* get_prepared_layout(void);
default_info_t* get_root_info(void);
uint16_t parse_field_u16(const uint8_t* content, const char* key);
uint8_t parse_field_u8(const uint8_t* content, const char* key);
int parse_field(const uint8_t* content, const char* key, void* value, size_t value_size, field_type_t type);
#endif /* _LAYOUT_BINARY_H_ */