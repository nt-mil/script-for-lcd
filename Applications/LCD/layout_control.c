#include "main.h"
#include "layout_control.h"

void process_layout_script(void) {
     uint8_t test_buffer[1][101] = {
        "$id:clock_and_date;$hour:12;$min:34;$sec:56;$day:1;$month:1;",
        // "$id:clock_and_date;$hour:1;$min:1;$sec:1;$day:12;$month:12;",
    };

    initialize_layout_binary_info();

    for (int i = 0; i < 1; i++) {
        uint8_t* ptr = &test_buffer[i][0];
        parse_layout(ptr, sizeof(test_buffer[0]));
    }
}