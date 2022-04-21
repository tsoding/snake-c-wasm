#include "./game.h"

#define FACTOR 100
#define WIDTH  (16*FACTOR)
#define HEIGHT (9*FACTOR)

#define NULL ((void*)0)

#define TRUE 1
#define FALSE 0

// NOTE: being able to disable logging allows to cut the size of the executable dramatically
// by not including the sprintf implementation
#define GAME_LOGGING
#ifdef GAME_LOGGING
#define STB_SPRINTF_IMPLEMENTATION 
#include "stb_sprintf.h"

static char logf_buf[4096] = {0};
#define LOGF(...) \
    do { \
        stbsp_snprintf(logf_buf, sizeof(logf_buf), __VA_ARGS__); \
        platform_log(logf_buf); \
    } while(0)
#else
#define LOGF(...)
#endif

void platform_assert(const char *file, i32 line, b32 cond, const char *message)
{
    if (!cond) platform_panic(file, line, message);
}
#define ASSERT(cond, message) platform_assert(__FILE__, __LINE__, cond, message)
#define UNREACHABLE() platform_panic(__FILE__, __LINE__, "unreachable")

#define CELL_SIZE 100
#define COLS (WIDTH/CELL_SIZE)
#define ROWS (HEIGHT/CELL_SIZE)

#define BACKGROUND_COLOR 0xFF181818
#define CELL1_COLOR BACKGROUND_COLOR
#define CELL2_COLOR 0xFF183018
#define SNAKE_BODY_COLOR 0xFF189018
#define EGG_COLOR 0xFF31A6FF

#define SNAKE_INIT_SIZE 3

#define STEP_INTEVAL 0.2f

#define RAND_A 6364136223846793005ULL
#define RAND_C 1442695040888963407ULL

static u32 rand(void)
{
    static u64 rand_state = 0;
    rand_state = rand_state*RAND_A + RAND_C;
    return (rand_state >> 32)&0xFFFFFFFF;
}

static inline void *memset(void *mem, u32 c, u32 n)
{
    void *result = mem;
    u8 *bytes = mem;
    while (n-- > 0) *bytes++ = c;
    return result;
}

typedef enum {
    DIR_RIGHT = 0,
    DIR_UP,
    DIR_LEFT,
    DIR_DOWN,
    COUNT_DIRS,
} Dir;

static Dir dir_opposite(Dir dir)
{
    switch (dir) {
        case DIR_RIGHT: return DIR_LEFT;
        case DIR_LEFT:  return DIR_RIGHT;
        case DIR_UP:    return DIR_DOWN;
        case DIR_DOWN:  return DIR_UP;
        case COUNT_DIRS:
        default: {
            UNREACHABLE();
        }
    }
    return 0;
}

typedef struct {
    i32 x, y;
} Cell;

#define SNAKE_CAP (ROWS*COLS)
typedef struct {
    Cell items[SNAKE_CAP];
    u32 begin;
    u32 size;
} Snake;

typedef enum {
    STATE_GAMEPLAY = 0,
    STATE_GAMEOVER
} State;

#define DIR_QUEUE_CAP 3
typedef struct {
    u32 begin;
    u32 size;
    Dir items[DIR_QUEUE_CAP];
} Dir_Queue;

typedef struct {
    State state;
    Snake snake;
    Cell egg;

    Dir dir;
    Dir_Queue next_dirs;
    b32 dir_keys[COUNT_DIRS];

    f32 step_cooldown;

    u32 score;
    char score_buffer[256];
} Game;

static Game game = {0};

#define ring_empty(ring) ((ring)->size == 0)

#define ring_cap(ring) (sizeof((ring)->items)/sizeof((ring)->items[0]))

#define ring_push_back(ring, item) \
    do { \
        ASSERT((ring)->size < ring_cap(ring), "Ring buffer overflow"); \
        u32 index = ((ring)->begin + (ring)->size)%ring_cap(ring); \
        (ring)->items[index] = (item); \
        (ring)->size += 1; \
    } while (0)

#define ring_displace_back(ring, item) \
    do { \
        u32 index = ((ring)->begin + (ring)->size)%ring_cap(ring); \
        (ring)->items[index] = (item); \
        if ((ring)->size < ring_cap(ring)) { \
            (ring)->size += 1; \
        } else { \
            (ring)->begin = ((ring)->begin + 1)%ring_cap(ring); \
        } \
    } while (0)

#define ring_pop_front(ring) \
    do { \
        ASSERT((ring)->size > 0, "Ring buffer underflow"); \
        (ring)->begin = ((ring)->begin + 1)%ring_cap(ring); \
        (ring)->size -= 1; \
    } while (0)

#define ring_back(ring) \
    (ASSERT((ring)->size > 0, "Ring buffer is empty"), \
     &(ring)->items[((ring)->begin + (ring)->size - 1)%ring_cap(ring)])
#define ring_front(ring) \
    (ASSERT((ring)->size > 0, "Ring buffer is empty"), \
     &(ring)->items[(ring)->begin])

static inline b32 cell_eq(const Cell *a, const Cell *b)
{
    return a->x == b->x && a->y == b->y;
}

static b32 is_cell_snake_body(const Cell *cell)
{
    for (u32 offset = 0; offset < game.snake.size; ++offset) {
        u32 index = (game.snake.begin + offset)%SNAKE_CAP;
        if (cell_eq(&game.snake.items[index], cell)) {
            return TRUE;
        }
    }
    return FALSE;
}

static void random_egg(void)
{
    do {
        game.egg.x = rand()%COLS;
        game.egg.y = rand()%ROWS;
    } while (is_cell_snake_body(&game.egg));
}

static void game_restart(void)
{
    memset(&game, 0, sizeof(game));
    for (u32 i = 0; i < SNAKE_INIT_SIZE; ++i) {
        Cell head = {.x = i, .y = ROWS/2};
        ring_push_back(&game.snake, head);
    }
    random_egg();
    game.dir = DIR_RIGHT;
    stbsp_snprintf(game.score_buffer, sizeof(game.score_buffer), "Score: %u", game.score);
}

static void snake_render(Snake *snake)
{
    for (u32 offset = 0; offset < snake->size; ++offset) {
        u32 index = (snake->begin + offset)%SNAKE_CAP;
        Cell *cell = &snake->items[index];
        platform_fill_rect(cell->x*CELL_SIZE, cell->y*CELL_SIZE, CELL_SIZE, CELL_SIZE, SNAKE_BODY_COLOR);
    }
}

static void background_render(void)
{
    platform_fill_rect(0, 0, WIDTH, HEIGHT, BACKGROUND_COLOR);
    for (i32 col = 0; col < COLS; ++col) {
        for (i32 row = 0; row < ROWS; ++row) {
            u32 color = (row + col)%2 == 0 ? CELL1_COLOR : CELL2_COLOR;
            i32 x = col*CELL_SIZE;
            i32 y = row*CELL_SIZE;
            i32 w = CELL_SIZE;
            i32 h = CELL_SIZE;
            platform_fill_rect(x, y, w, h, color);
        }
    }
}

static inline i32 emod(i32 a, i32 b)
{
    return (a%b + b)%b;
}

static Cell step_cell(Cell head, Dir dir)
{
    switch (dir) {
    case DIR_RIGHT:
        head.x += 1;
        break;

    case DIR_UP:
        head.y -= 1;
        break;

    case DIR_LEFT:
        head.x -= 1;
        break;

    case DIR_DOWN:
        head.y += 1;
        break;

    case COUNT_DIRS:
    default: {
        UNREACHABLE();
    }
    }

    head.x = emod(head.x, COLS);
    head.y = emod(head.y, ROWS);

    return head;
}

void game_init()
{
    game_restart();
    LOGF("Game initialized");
}

#define SCORE_PADDING 100
#define SCORE_FONT_SIZE 48
#define SCORE_FONT_COLOR 0xFFFFFFFF

void game_render(void)
{
    background_render();
    platform_fill_rect(game.egg.x*CELL_SIZE, game.egg.y*CELL_SIZE, CELL_SIZE, CELL_SIZE, EGG_COLOR);
    snake_render(&game.snake);
    platform_draw_text(SCORE_PADDING, SCORE_PADDING, game.score_buffer, SCORE_FONT_SIZE, SCORE_FONT_COLOR);

    if (game.state == STATE_GAMEOVER) {
        // TODO: draw text "Game Over"
        // TODO: render the game differently (maybe everything black and white)
    }
}

void game_keydown(Key key)
{
    switch (game.state) {
        case STATE_GAMEPLAY: {
            switch (key) {
                case KEY_UP: 
                    ring_displace_back(&game.next_dirs, DIR_UP);
                    break;
                case KEY_DOWN: 
                    ring_displace_back(&game.next_dirs, DIR_DOWN);
                    break;
                case KEY_LEFT: 
                    ring_displace_back(&game.next_dirs, DIR_LEFT);
                    break;
                case KEY_RIGHT: 
                    ring_displace_back(&game.next_dirs, DIR_RIGHT);
                    break;
                default: {}
            }
        } break;
        
        case STATE_GAMEOVER: {
            if (key == KEY_ACCEPT) {
                game_restart();
            }
        } break;
    }
}

void game_update(f32 dt)
{
    switch (game.state) {
        case STATE_GAMEPLAY: {
            game.step_cooldown -= dt;
            if (game.step_cooldown <= 0.0f) {
                if (!ring_empty(&game.next_dirs)) {
                    if (dir_opposite(game.dir) != *ring_front(&game.next_dirs)) {
                        game.dir = *ring_front(&game.next_dirs);
                    }
                    ring_pop_front(&game.next_dirs);
                }

                Cell next_head = step_cell(*ring_back(&game.snake), game.dir);

                if (cell_eq(&game.egg, &next_head)) {
                    ring_push_back(&game.snake, next_head);
                    random_egg();
                    game.score += 1;
                    stbsp_snprintf(game.score_buffer, sizeof(game.score_buffer), "Score: %u", game.score);
                } else if (is_cell_snake_body(&next_head)) {
                    game.state = STATE_GAMEOVER;
                    return;
                } else {
                    ring_push_back(&game.snake, next_head);
                    ring_pop_front(&game.snake);
                }

                game.step_cooldown = STEP_INTEVAL;
                // TODO: pause on SPACE
            }
        } break;

        case STATE_GAMEOVER: {} break;

        default: {
            UNREACHABLE();
        }
    }
}

const Game_Info *game_info(void)
{
    // TODO: the platform should dictate the resolution, not the other way around
    static const Game_Info gi = {
        .width = WIDTH,
        .height = HEIGHT,
    };
    return &gi;
}
