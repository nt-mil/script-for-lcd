#ifndef _SCRIPT_PARSING_H_
#define _SCRIPT_PARSING_H_

typedef struct {
    uint8_t* ptr;
    uint16_t length;
} MY_STRING;

int my_scanf(MY_STRING* str, const uint8_t* format,...);
void parse_layout(uint8_t* buffer, uint16_t length);
void* memmem();
void init_new_redering(void);
bool get_next_script_line(MY_STRING* str);

#endif /* _SCRIPT_PARSING_H_ */