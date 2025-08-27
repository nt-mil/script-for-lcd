#include "main.h"
#include "broker.h"

#define MAX_SUBSCRIBERS 8
#define BROKER_QUEUE_LEN 16

typedef struct {
    broker_msg_t msg;
    void *data;
    size_t len;
} broker_msg_item_t;

typedef struct {
    broker_msg_t msg;
    broker_callback_t cb;
} subscriber_t;

static subscriber_t subs[MAX_SUBSCRIBERS];
static int sub_count = 0;

static QueueHandle_t brokerQueue;

static void broker_task(void *arg) {
    broker_msg_item_t item;
    for (;;) {
        if (xQueueReceive(brokerQueue, &item, portMAX_DELAY) == pdPASS) {
            for (int i = 0; i < sub_count; i++) {
                if (subs[i].msg == item.msg && subs[i].cb) {
                    subs[i].cb(item.data, item.len);
                }
            }
        }
    }
}

void broker_init(void) {
    brokerQueue = xQueueCreate(BROKER_QUEUE_LEN, sizeof(broker_msg_item_t));
    xTaskCreate(broker_task, "broker", 512, NULL, tskIDLE_PRIORITY+2, NULL);
}

void broker_subscribe(broker_msg_t msg, broker_callback_t cb) {
    if (sub_count < MAX_SUBSCRIBERS) {
        subs[sub_count].msg = msg;
        subs[sub_count].cb  = cb;
        sub_count++;
    }
}

BaseType_t broker_publish(broker_msg_t msg, void *data, size_t len) {
    broker_msg_item_t item = { msg, data, len };
    return xQueueSend(brokerQueue, &item, 0);
}

BaseType_t broker_publish_from_isr(broker_msg_t msg, void *data, size_t len, BaseType_t *pxHigherPriorityTaskWoken) {
    broker_msg_item_t item = { msg, data, len };
    return xQueueSendFromISR(brokerQueue, &item, pxHigherPriorityTaskWoken);
}
