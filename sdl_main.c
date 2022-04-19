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

b32 platform_keydown(Key key)
{
    const Uint8* keyboard = SDL_GetKeyboardState(NULL);
    switch (key) {
        case KEY_LEFT:  return keyboard[SDL_SCANCODE_A];
        case KEY_RIGHT: return keyboard[SDL_SCANCODE_D];
        case KEY_UP:    return keyboard[SDL_SCANCODE_W];
        case KEY_DOWN:  return keyboard[SDL_SCANCODE_S];
        default:        return 0;
    }
}

int main()
{
    scc(SDL_Init(SDL_INIT_VIDEO));
    window = scp(SDL_CreateWindow(
                     "Snake Native SDL",
                     0, 0,
                     WIDTH, HEIGHT,
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
            }
            break;
            }
        }

        game_render();
        // TODO: better way to lock 60 FPS
        game_update(1.0f/60.0f);
        SDL_RenderPresent(renderer);
        SDL_Delay(1000/60);
    }

    SDL_Quit();

    return 0;
}
