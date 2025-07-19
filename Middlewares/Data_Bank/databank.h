#ifndef _DATABANK_H_
#define _DATABANK_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void* info;
} device_info_entry_t;

void initialize_databank(void);
uint16_t write_to_databank(void* device_info);
bool read_from_databank(uint16_t index);

#endif /* _DATABANK_H_ */