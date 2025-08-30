#include "main.h"
#include "databank.h"

// Maximum number of device entries supported in the databank.
#define MAX_DEVICE_COUNT 5

typedef struct {
    uint16_t entry_count;
    device_info_entry_t data_entries[MAX_DEVICE_COUNT];
} databank_storage_t;

static databank_storage_t device_databank;

void initialize_databank(void) {
    device_databank.entry_count = 0;
    memset(device_databank.data_entries, 0, sizeof(device_databank.data_entries));
}

uint16_t write_to_databank(void* device_info) {
    if (device_databank.entry_count >= MAX_DEVICE_COUNT) {
        return 0xFFFE; // Indicate failure due to full databank
    }

    uint16_t index = 0;
    device_info_entry_t* entry = &device_databank.data_entries[device_databank.entry_count];

    entry->info = device_info;
    index = device_databank.entry_count;
    device_databank.entry_count++;

    return index;
}

void* read_from_databank(uint16_t index) {
    if (index >= device_databank.entry_count || index >= MAX_DEVICE_COUNT) {
        return false; // Invalid index
    }
    return device_databank.data_entries[index].info;
}