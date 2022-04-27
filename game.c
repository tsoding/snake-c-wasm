#include "./game.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

#define NULL ((void*)0)

#define TRUE 1
#define FALSE 0

static char logf_buf[4096] = {0};
#define LOGF(...) \
    do { \
        stbsp_snprintf(logf_buf, sizeof(logf_buf), __VA_ARGS__); \
        platform_log(logf_buf); \
    } while(0)

void platform_assert(const char *file, i32 line, b32 cond, const char *message)
{
    if (!cond) platform_panic(file, line, message);
}
#define ASSERT(cond, message) platform_assert(__FILE__, __LINE__, cond, message)
#define UNREACHABLE() platform_panic(__FILE__, __LINE__, "unreachable")

#define COLS 16
#define ROWS 9

#define BACKGROUND_COLOR 0xFF181818
#define CELL1_COLOR BACKGROUND_COLOR
#define CELL2_COLOR 0xFF183018
#define SNAKE_BODY_COLOR 0xFF189018
#define EGG_COLOR 0xFF31A6FF
#define DEBUG_COLOR 0xFF0000FF

#define SNAKE_INIT_SIZE 3

#define STEP_INTEVAL 0.1f

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

static inline Dir dir_opposite(Dir dir)
{
    ASSERT(0 <= dir && dir < COUNT_DIRS, "Invalid direction");
    return (dir + 2)%COUNT_DIRS;
}

typedef struct {
    f32 x, y, w, h;
} Rect;

typedef struct {
    f32 lens[COUNT_DIRS];
} Sides;

Sides rect_sides(Rect rect)
{
    Sides sides = {
        .lens = {
            [DIR_LEFT]  = rect.x,
            [DIR_RIGHT] = rect.x + rect.w,
            [DIR_UP]    = rect.y,
            [DIR_DOWN]  = rect.y + rect.h,
        }
    };
    return sides;
}

inline b32 sides_overlap(Sides a, Sides b)
{
    f32 r1 = a.lens[DIR_RIGHT];
    f32 l1 = a.lens[DIR_LEFT];
    f32 t1 = a.lens[DIR_UP];
    f32 b1 = a.lens[DIR_DOWN];

    f32 r2 = b.lens[DIR_RIGHT];
    f32 l2 = b.lens[DIR_LEFT];
    f32 t2 = b.lens[DIR_UP];
    f32 b2 = b.lens[DIR_DOWN];

    return !(r1 < l2 || r2 < l1 || b1 < t2 || b2 < t1);
}

inline b32 rects_overlap(Rect a, Rect b)
{
    return sides_overlap(rect_sides(a), rect_sides(b));
}

typedef struct {
    i32 x, y;
} Cell;

typedef struct {
    f32 x, y;
} Vec;

#define SNAKE_CAP (ROWS*COLS)
typedef struct {
    Cell items[SNAKE_CAP];
    u32 begin;
    u32 size;
} Snake;

typedef enum {
    STATE_GAMEPLAY = 0,
    STATE_PAUSE,
    STATE_GAMEOVER,
} State;

#define DIR_QUEUE_CAP 3
typedef struct {
    u32 begin;
    u32 size;
    Dir items[DIR_QUEUE_CAP];
} Dir_Queue;

typedef struct {
    u32 width;
    u32 height;
    u32 cell_width;
    u32 cell_height;

    State state;
    Snake snake;
    Cell egg;
    b32 eating_egg;

    Dir dir;
    Dir_Queue next_dirs;
    b32 dir_keys[COUNT_DIRS];

    f32 step_cooldown;

    u32 score;
    char score_buffer[256];
} Game;

static Game game = {0};

Rect cell_rect(Cell cell)
{
    Rect result = {
        .x = cell.x*game.cell_width,
        .y = cell.y*game.cell_height,
        .w = game.cell_width,
        .h = game.cell_height,
    };
    return result;
}

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
#define ring_get(ring, index) \
    (ASSERT((u32)(index) < (ring)->size, "Invalid index"), \
     &(ring)->items[((ring)->begin + (index))%ring_cap(ring)])

static inline b32 cell_eq(Cell a, Cell b)
{
    return a.x == b.x && a.y == b.y;
}

static b32 is_cell_snake_body(Cell cell)
{
    for (u32 index = 1; index < game.snake.size; ++index) {
        if (cell_eq(*ring_get(&game.snake, index), cell)) {
            return TRUE;
        }
    }
    return FALSE;
}

static inline Cell random_cell(void)
{
    Cell result = {0};
    result.x = rand()%COLS;
    result.y = rand()%ROWS;
    return result;
}

static inline Cell random_cell_outside_of_snake(void)
{
    Cell cell;
    do {
        cell = random_cell();
    } while (is_cell_snake_body(cell));
    return cell;
}

static void game_restart(u32 width, u32 height)
{
    memset(&game, 0, sizeof(game));

    game.width       = width;
    game.height      = height;
    // NOTE: This implies that the platform has to carefully choose the resolution so the cells stay squared
    game.cell_width  = width / COLS;
    game.cell_height = height / ROWS;
    for (u32 i = 0; i < SNAKE_INIT_SIZE; ++i) {
        Cell head = {.x = i, .y = ROWS/2};
        ring_push_back(&game.snake, head);
    }
    game.egg = random_cell_outside_of_snake();
    game.dir = DIR_RIGHT;
    stbsp_snprintf(game.score_buffer, sizeof(game.score_buffer), "Score: %u", game.score);
}

f32 lerpf(f32 a, f32 b, f32 t)
{
    return (b - a)*t + a;
}

f32 ilerpf(f32 a, f32 b, f32 v)
{
    return (v - a)/(b - a);
}

static void fill_cell(Cell cell, u32 color, f32 a)
{
    f32 x = cell.x*game.cell_width;
    f32 y = cell.y*game.cell_height;
    f32 w = game.cell_width;
    f32 h = game.cell_height;
    platform_fill_rect(
            lerpf(x, x + w*0.5f, 1.0f - a), 
            lerpf(y, y + h*0.5f, 1.0f - a), 
            lerpf(0.0f, w, a), 
            lerpf(0.0f, h, a), 
            color);
}

void stroke_cell(Cell cell, u32 color)
{
    platform_stroke_rect(cell.x*game.cell_width, cell.y*game.cell_height, game.cell_width, game.cell_height, color);
}

void stroke_sides(Sides sides, u32 color)
{
    platform_stroke_rect(
        sides.lens[DIR_LEFT],
        sides.lens[DIR_UP],
        sides.lens[DIR_RIGHT] - sides.lens[DIR_LEFT],
        sides.lens[DIR_DOWN] - sides.lens[DIR_UP],
        color);
}

void fill_sides(Sides sides, u32 color)
{
    platform_fill_rect(
        sides.lens[DIR_LEFT],
        sides.lens[DIR_UP],
        sides.lens[DIR_RIGHT] - sides.lens[DIR_LEFT],
        sides.lens[DIR_DOWN] - sides.lens[DIR_UP],
        color);
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

Dir cells_dir(Cell a, Cell b)
{
    for (Dir dir = 0; dir < COUNT_DIRS; ++dir) {
        if (cell_eq(step_cell(a, dir), b)) return dir;
    }
    UNREACHABLE();
    return 0;
}

Vec cell_center(Cell a)
{
    return (Vec) {
        .x = a.x*game.cell_width + game.cell_width*0.5f,
        .y = a.y*game.cell_height + game.cell_height*0.5f,
    };
}

Sides cut_sides(Sides sides, Dir dir, f32 a)
{
    sides.lens[dir_opposite(dir)] = lerpf(sides.lens[dir_opposite(dir)], sides.lens[dir], a);
    return sides;
}

Sides slide_sides(Sides sides, Dir dir, f32 a)
{
    f32 d = sides.lens[dir] - sides.lens[dir_opposite(dir)];
    sides.lens[dir]               += lerpf(0, d, a);
    sides.lens[dir_opposite(dir)] += lerpf(0, d, a);
    return sides;
}

static void snake_render(void)
{
    f32 t = game.step_cooldown / STEP_INTEVAL;

    if (game.eating_egg) {
        Cell  head_cell   = *ring_back(&game.snake);
        Sides head_sides  = rect_sides(cell_rect(head_cell));
        Dir   head_dir    = game.dir;

        fill_sides(head_sides, EGG_COLOR);
        fill_sides(slide_sides(head_sides, dir_opposite(head_dir), t), SNAKE_BODY_COLOR);

        for (u32 index = 1; index < game.snake.size - 1; ++index) {
            fill_cell(*ring_get(&game.snake, index), SNAKE_BODY_COLOR, 1.0f);
        }
    } else {
        Cell  tail_cell   = *ring_front(&game.snake);
        Sides tail_sides  = rect_sides(cell_rect(tail_cell));
        Dir   tail_dir    = cells_dir(*ring_get(&game.snake, 0), *ring_get(&game.snake, 1));

        fill_sides(slide_sides(tail_sides, tail_dir, 1.0f - t), SNAKE_BODY_COLOR);

        Cell  head_cell   = *ring_back(&game.snake);
        Sides head_sides  = rect_sides(cell_rect(head_cell));
        Dir   head_dir    = game.dir;

        fill_sides(slide_sides(head_sides, dir_opposite(head_dir), t), SNAKE_BODY_COLOR);

        for (u32 index = 1; index < game.snake.size - 1; ++index) {
            fill_cell(*ring_get(&game.snake, index), SNAKE_BODY_COLOR, 1.0f);
        }
    }
}

static void background_render(void)
{
    for (i32 col = 0; col < COLS; ++col) {
        for (i32 row = 0; row < ROWS; ++row) {
            u32 color = (row + col)%2 == 0 ? CELL1_COLOR : CELL2_COLOR;
            Cell cell = { .x = col, .y = row, };
            fill_cell(cell, color, 1.0f);
        }
    }
}



// TODO: controls tutorial
void game_init(u32 width, u32 height)
{
    game_restart(width, height);
    LOGF("Game initialized");
}

#define SCORE_PADDING 100
// TODO: font size relative to the resolution
#define SCORE_FONT_SIZE 48
#define SCORE_FONT_COLOR 0xFFFFFFFF
#define PAUSE_FONT_COLOR SCORE_FONT_COLOR
#define PAUSE_FONT_SIZE SCORE_FONT_SIZE
#define GAMEOVER_FONT_COLOR SCORE_FONT_COLOR
#define GAMEOVER_FONT_SIZE SCORE_FONT_SIZE

void egg_render(void)
{
    if (game.eating_egg) {
        f32 t = 1.0f - game.step_cooldown/STEP_INTEVAL;
        f32 px = 0.75f;
        f32 py = 1.25f;
        if (t < px) {
            f32 a = lerpf(0.0f, py, ilerpf(0.0f, px, t));
            fill_cell(game.egg, EGG_COLOR, a*a);
        } else {
            f32 a = lerpf(py, 1.0f, ilerpf(px, 1.0f, t));
            fill_cell(game.egg, EGG_COLOR, a);
        }
    } else {
        fill_cell(game.egg, EGG_COLOR, 1.0f);
    }
}

void game_render(void)
{
    background_render();
    snake_render();
    egg_render();
    platform_fill_text(SCORE_PADDING, SCORE_PADDING, game.score_buffer, SCORE_FONT_SIZE, SCORE_FONT_COLOR, ALIGN_LEFT);

    switch (game.state) {
    case STATE_GAMEPLAY: {
    } break;

    case STATE_PAUSE: {
        // TODO: "Pause", "Game Over" are not centered vertically
        platform_fill_text(game.width/2, game.height/2, "Pause", PAUSE_FONT_SIZE, PAUSE_FONT_COLOR, ALIGN_CENTER);
    }
    break;

    case STATE_GAMEOVER: {
        platform_fill_text(game.width/2, game.height/2, "Game Over", GAMEOVER_FONT_SIZE, GAMEOVER_FONT_COLOR, ALIGN_CENTER);
    }
    break;

    default: {
        UNREACHABLE();
    }
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
        case KEY_ACCEPT:
            game.state = STATE_PAUSE;
            break;
        case KEY_RESTART:
            game_restart(game.width, game.height);
            break;
        default:
        {}
        }
    } break;

    case STATE_PAUSE: {
        switch (key) {
            case KEY_ACCEPT:
                game.state = STATE_GAMEPLAY;
                break;
            case KEY_RESTART:
                game_restart(game.width, game.height);
                break;
            case KEY_LEFT:
            case KEY_RIGHT:
            case KEY_UP:
            case KEY_DOWN:
            default: {}
        }
    }
    break;

    case STATE_GAMEOVER: {
        if (key == KEY_ACCEPT || key == KEY_RESTART) {
            game_restart(game.width, game.height);
        }
    }
    break;

    default: {
        UNREACHABLE();
    }
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

            if (cell_eq(game.egg, next_head)) {
                ring_push_back(&game.snake, next_head);
                game.egg = random_cell_outside_of_snake();
                game.eating_egg = TRUE;
                game.score += 1;
                stbsp_snprintf(game.score_buffer, sizeof(game.score_buffer), "Score: %u", game.score);
            } else if (is_cell_snake_body(next_head)) {
                // NOTE: reseting step_cooldown to 0 is important bcause the whole smooth movement is based on it.
                // Without this reset the head of the snake "detaches" from the snake on the Game Over, when
                // step_cooldown < 0.0f
                game.step_cooldown = 0.0f;
                // TODO: interesting animation on Game Over
                game.state = STATE_GAMEOVER;
                return;
            } else {
                ring_push_back(&game.snake, next_head);
                ring_pop_front(&game.snake);
                game.eating_egg = FALSE;
            }

            game.step_cooldown = STEP_INTEVAL;
        }
    }
    break;

    case STATE_PAUSE:
    case STATE_GAMEOVER:
    {} break;

    default: {
        UNREACHABLE();
    }
    }
}

// TODO: moving around egg
