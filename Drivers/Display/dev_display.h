#ifndef DEV_LCD_H
#define DEV_LCD_H

typedef struct {
    void (*init)(void);
    // void (*update)(void);
    // void (*update_window)(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    uint8_t* (*get_framebuffer)(void);
} display_driver_t;

#endif /* DEV_LCD_H */