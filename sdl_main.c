#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#include "game.h"

void game_update(float dt);
void game_render(void);

void scc(int code)
{
    if (code < 0) {
        fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
        exit(1);
    }
}

void *scp(void *ptr)
{
    if (ptr == NULL) {
        fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
        exit(1);
    }
    return ptr;
}

static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;

void platform_draw_text(i32 x, i32 y, const char *text, u32 size, u32 color)
{
    (void) x;
    (void) y;
    (void) text;
    (void) size;
    (void) color;
    // TODO: platform_draw_text for SDL2 platform
}

void platform_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    assert(renderer != NULL);
    SDL_Rect rect = {.x = x, .y = y, .w = w, .h = h,};
    uint8_t r = (color>>(8*0))&0xFF;
    uint8_t g = (color>>(8*1))&0xFF;
    uint8_t b = (color>>(8*2))&0xFF;
    uint8_t a = (color>>(8*3))&0xFF;
    scc(SDL_SetRenderDrawColor(renderer, r, g, b, a));
    scc(SDL_RenderFillRect(renderer, &rect));
}

void platform_panic(const char *file_path, int line, const char *message)
{
    fprintf(stderr, "%s:%d: GAME ASSERTION FAILED: %s\n", file_path, line, message);
    exit(1);
}

void platform_log(const char *message)
{
    printf("[LOG] %s\n", message);
}

int main()
{
    game_init();
    const Game_Info *gi = game_info();

    scc(SDL_Init(SDL_INIT_VIDEO));
    window = scp(SDL_CreateWindow(
                     "Snake Native SDL",
                     0, 0,
                     gi->width, gi->height,
                     SDL_WINDOW_RESIZABLE));

    renderer = scp(SDL_CreateRenderer(
                       window,
                       -1,
                       SDL_RENDERER_ACCELERATED));

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: {
                quit = true;
            } break;
            
            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                    case SDLK_a:     game_keydown(KEY_LEFT);    break;
                    case SDLK_d:     game_keydown(KEY_RIGHT);   break;
                    case SDLK_s:     game_keydown(KEY_DOWN);    break;
                    case SDLK_w:     game_keydown(KEY_UP);      break;
                    case SDLK_SPACE: game_keydown(KEY_ACCEPT);  break;
                }
            } break;
            }
        }

        game_update(1.0f/60.0f);
        game_render();
        // TODO: better way to lock 60 FPS
        SDL_RenderPresent(renderer);
        SDL_Delay(1000/60);
    }

    SDL_Quit();

    return 0;
}
