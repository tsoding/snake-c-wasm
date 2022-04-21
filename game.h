#ifndef GAME_H_
#define GAME_H_ 

typedef unsigned int u32;
typedef unsigned char u8;
typedef int i32;
typedef int b32;
typedef float f32;
typedef unsigned long long u64;

typedef enum {
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_ACCEPT,
} Key;

typedef struct {
    u32 width;
    u32 height;
} Game_Info;

void platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color);
// TODO: custom font for platform_draw_text
void platform_draw_text(i32 x, i32 y, const char *text, u32 size, u32 color);
void platform_panic(const char *file_path, i32 line, const char *message);
void platform_log(const char *message);

void game_init(void);
void game_render(void);
void game_update(f32 dt);
void game_keydown(Key key);
const Game_Info *game_info(void);

#endif // GAME_H_
