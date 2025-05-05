#ifndef DEV_DISPLAY_H
#define DEV_DISPLAY_H

#define LCD_WIDTH       (100)
#define LCD_HEIGHT      (50)
#define BIT_PER_PIXEL   (24)

#define SCREEN_SIZE     (LCD_WIDTH * LCD_HEIGHT * BIT_PER_PIXEL/8)

#define LCD_CS_PORT
#define LCD_CS_PIN

#define LCD_DC_PORT
#define LCD_DC_PIN



enum
{
    LCD_READY = 0,
    LCD_INITIALIZING,
    LCD_RUNNING,
    LCD_IDLE
} LCD_STATE;

enum
{
    STATE_RESET_PORT = 0,
    STATE_SENDING_COMMAND,
    STATE_COMPLETED
}LCD_INIT_STATE;

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

#endif /* DEV_DISPLAY_H */