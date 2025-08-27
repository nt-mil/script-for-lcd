#ifndef BROKER_H
#define BROKER_H

typedef enum {
    MSG_UART_RX,
    MSG_KEY_EVENT,
    MSG_DISPLAY_READY,
    // thêm loại message khác
} broker_msg_t;

typedef void (*broker_callback_t)(void *data, size_t len);

void broker_init(void);
void broker_subscribe(broker_msg_t msg, broker_callback_t cb);
BaseType_t broker_publish_from_isr(broker_msg_t msg, void *data, size_t len, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t broker_publish(broker_msg_t msg, void *data, size_t len);

#endif /* BROKER_H */