#ifndef GAME_H_
#define GAME_H_ 

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int i32;
typedef int b32;
typedef float f32;

typedef enum {
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_CENTER,
} Align;

void platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color);
void platform_stroke_rect(i32 x, i32 y, i32 w, i32 h, u32 color);
void platform_fill_text(i32 x, i32 y, const char *text, u32 size, u32 color, Align align);
void platform_stroke_line(i32 x1, i32 y1, i32 x2, i32 y2, u32 color);
void platform_panic(const char *file_path, i32 line, const char *message);
void platform_log(const char *message);

void game_init(u32 width, u32 height);
void game_resize(u32 width, u32 height);
void game_render(void);
void game_update(f32 dt);
void game_keydown(int key);

#endif // GAME_H_
