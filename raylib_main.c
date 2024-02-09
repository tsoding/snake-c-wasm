#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <raylib.h>

#include "game.h"

#define FACTOR 100
#define WIDTH (16*FACTOR)
#define HEIGHT (9*FACTOR)

static Font font = {0};

void platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color)
{
    DrawRectangle(x, y, w, h, *(Color*)&color);
}

void platform_stroke_rect(i32 x, i32 y, i32 w, i32 h, u32 color)
{
    DrawRectangleLines(x, y, w, h, *(Color*)&color);
}

u32 platform_text_width(const char *text, u32 size)
{
    return MeasureText(text, size);
}

void platform_fill_text(i32 x, i32 y, const char *text, u32 fontSize, u32 color)
{
    Vector2 size = MeasureTextEx(font, text, fontSize, 0);
    Vector2 position = {.x = x, .y = y - size.y};
    DrawTextEx(font, text, position, fontSize, 0.0, *(Color*)&color);
}

void platform_panic(const char *file_path, i32 line, const char *message)
{
    fprintf(stderr, "%s:%d: GAME ASSERTION FAILED: %s\n", file_path, line, message);
    abort();
}

void platform_log(const char *message)
{
    TraceLog(LOG_INFO, "%s", message);
}

int main(void)
{
    InitWindow(WIDTH, HEIGHT, "Snake");
    game_init(WIDTH, HEIGHT);

    font = LoadFontEx("fonts/AnekLatin-Light.ttf", 48, NULL, 0);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose()) {
        const char *keys = "ADWS RZXC";
        size_t n = strlen(keys);
        for (size_t i = 0; i < n; ++i) {
            if (IsKeyPressed(keys[i])) {
                game_keydown(tolower(keys[i]));
            }
        }

        BeginDrawing();
        game_update(GetFrameTime());
        game_render();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
