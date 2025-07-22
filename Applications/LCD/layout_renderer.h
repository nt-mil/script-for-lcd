#ifndef _RENDERING_H_
#define _RENDERING_H_

typedef struct {
    uint16_t x, y;
} POINT;

typedef struct {
    POINT s; // top-left corner
    POINT e; // bottom-right corner (exclusive)
} AREA;

// align struct
typedef struct {
    AREA area;
    uint8_t alignment;
} ALIGN;

// bitmap struct
typedef struct {
    AREA area;
    AREA bitmap_area;
    uint8_t *bitmap;
} BIT_MAP;

// typedef enum {
//     ALIGN_NONE = 0,
//     ALIGN_LEFT,
//     ALIGN_RIGHT,
//     ALIGN_CENTER,
// } ALIGNMENT;

bool get_script_ready(void);
void set_script_ready(void);
bool render_layout(void);

#endif /* _RENDERING_H_ */