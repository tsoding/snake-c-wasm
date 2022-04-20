#ifndef GAME_H_
#define GAME_H_ 

#define FACTOR 100
#define WIDTH  (16*FACTOR)
#define HEIGHT (9*FACTOR)

typedef unsigned int u32;
typedef int i32;
typedef int b32;
typedef float f32;
typedef unsigned long long u64;

typedef enum {
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
} Key;

void platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color);
void platform_panic(const char *file_path, i32 line, const char *message);
void platform_log(const char *message);
b32 platform_keydown(Key key);

#endif // GAME_H_
