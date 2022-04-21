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
    KEY_RESTART,
} Key;

typedef struct {
    u32 width;
    u32 height;
} Game_Info;

void platform_set_resolution(u32 w, u32 h);
void platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color);
void platform_panic(const char *file_path, i32 line, const char *message);
void platform_log(const char *message);
b32 platform_keydown(Key key);

void game_init(void);
void game_render(void);
void game_update(f32 dt);
const Game_Info *game_info(void);

#endif // GAME_H_