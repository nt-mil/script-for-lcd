#ifndef DEV_LCD_H
#define DEV_LCD_H

#define LCD_WIDTH       (320)
#define LCD_HEIGHT      (240)
#define BIT_PER_PIXEL   (4)
#define BYTE_PER_PIXEL  (BIT_PER_PIXEL / 8)

#define BYTE_PER_ROW    (LCD_WIDTH * BYTE_PER_PIXEL)
#define SCREEN_SIZE     (BYTE_PER_ROW * LCD_HEIGHT)

#define SPI_QUEUE_SIZE 10
#define DMA_MAX_COUNTS (10)
#define SPI_TIMEOUT_MS 5

enum
{
    LCD_READY = 0,
    LCD_INITIALIZING,
    LCD_RUNNING,
    LCD_IDLE
};

enum
{
    STATE_RESET_PORT = 0,
    STATE_SENDING_COMMAND,
    STATE_COMPLETED
};

typedef struct
{
    u8 initial_state;
    u8 current_state;
    u8 target_state;
    u8 screen_buffer[SCREEN_SIZE];
} LCD_INFO;

typedef struct
{
	u16 *buffer;
	u16 buffer_size;
} DISPLAY_INFO;

typedef struct
{
	void (*init)(void);
	void (*write)(u16, u16);
	void (*read)(u8 *, u16);
	DISPLAY_INFO *(*get_display_info)(void);
} DISPLAY_OPS;

void control_lcd(void);

#endif /* DEV_LCD_H */