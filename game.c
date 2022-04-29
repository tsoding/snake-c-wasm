#include "./game.h"

// #define FEATURE_DYNAMIC_CAMERA

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

static void platform_assert(const char *file, i32 line, b32 cond, const char *message)
{
    if (!cond) platform_panic(file, line, message);
}
#define ASSERT(cond, message) platform_assert(__FILE__, __LINE__, cond, message)
#define UNREACHABLE() platform_panic(__FILE__, __LINE__, "unreachable")

// NOTE: This implies that the platform has to carefully choose the resolution so the cells fit into the screen
#define CELL_SIZE 100
#define COLS 16
#define ROWS 9

#define BACKGROUND_COLOR 0xFF181818
#define CELL1_COLOR BACKGROUND_COLOR
#define CELL2_COLOR 0xFF183018
#define SNAKE_BODY_COLOR 0xFF189018
#define EGG_COLOR 0xFF31A6FF
#define DEBUG_COLOR 0xFF0000FF

#define SNAKE_INIT_SIZE 3

#define STEP_INTEVAL 0.125f

#define RAND_A 6364136223846793005ULL
#define RAND_C 1442695040888963407ULL

static u32 rand(void)
{
    static u64 rand_state = 0;
    rand_state = rand_state*RAND_A + RAND_C;
    return (rand_state >> 32)&0xFFFFFFFF;
}

static void *memset(void *mem, u32 c, u32 n)
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
    ASSERT(0 <= dir && dir < COUNT_DIRS, "Invalid direction");
    return (dir + 2)%COUNT_DIRS;
}

typedef struct {
    f32 x, y, w, h;
} Rect;

typedef struct {
    f32 lens[COUNT_DIRS];
} Sides;

static Sides rect_sides(Rect rect)
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

static Rect sides_rect(Sides sides)
{
    Rect rect = {
        .x = sides.lens[DIR_LEFT],
        .y = sides.lens[DIR_UP],
        .w = sides.lens[DIR_RIGHT] - sides.lens[DIR_LEFT],
        .h = sides.lens[DIR_DOWN] - sides.lens[DIR_UP],
    };
    return rect;
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

typedef struct {
    Rect items[SNAKE_CAP];
    Vec vels[SNAKE_CAP];
    u32 size;
} Dead_Snake;

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

    Vec camera_pos;
    Vec camera_vel;

    State state;
    Snake snake;
    Dead_Snake dead_snake;
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

static Rect cell_rect(Cell cell)
{
    Rect result = {
        .x = cell.x*CELL_SIZE,
        .y = cell.y*CELL_SIZE,
        .w = CELL_SIZE,
        .h = CELL_SIZE,
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
    (ASSERT((ring)->size > 0, "Ring buffer is empty"), \
     &(ring)->items[((ring)->begin + (index))%ring_cap(ring)])

static b32 cell_eq(Cell a, Cell b)
{
    return a.x == b.x && a.y == b.y;
}

static b32 is_cell_snake_body(Cell cell)
{
    // TODO: ignoring the tail feel hacky @tail-ignore
    for (u32 index = 1; index < game.snake.size; ++index) {
        if (cell_eq(*ring_get(&game.snake, index), cell)) {
            return TRUE;
        }
    }
    return FALSE;
}

static Cell random_cell(void)
{
    Cell result = {0};
    result.x = rand()%COLS;
    result.y = rand()%ROWS;
    return result;
}

static i32 emod(i32 a, i32 b)
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

#ifndef FEATURE_DYNAMIC_CAMERA
    head.x = emod(head.x, COLS);
    head.y = emod(head.y, ROWS);
#endif

    return head;
}

static Cell random_cell_outside_of_snake(void)
{
    // TODO: prevent running out of space
    // Should not be a problem with infinite field mechanics
    ASSERT(game.snake.size < ROWS*COLS, "No place");
    Cell cell;
    do {
        cell = random_cell();
    } while (is_cell_snake_body(cell));
    return cell;
}

// TODO: animation on restart
static void game_restart(u32 width, u32 height)
{
    memset(&game, 0, sizeof(game));

    game.width        = width;
    game.height       = height;
    game.camera_pos.x = width/2;
    game.camera_pos.y = height/2;

    for (u32 i = 0; i < SNAKE_INIT_SIZE; ++i) {
        Cell head = {.x = i, .y = ROWS/2};
        ring_push_back(&game.snake, head);
    }
    game.egg = random_cell_outside_of_snake();
    game.dir = DIR_RIGHT;
    stbsp_snprintf(game.score_buffer, sizeof(game.score_buffer), "Score: %u", game.score);
}

static f32 lerpf(f32 a, f32 b, f32 t)
{
    return (b - a)*t + a;
}

static f32 ilerpf(f32 a, f32 b, f32 v)
{
    return (v - a)/(b - a);
}

static void fill_rect(Rect rect, u32 color)
{
    platform_fill_rect(
        rect.x - game.camera_pos.x + game.width/2,
        rect.y - game.camera_pos.y + game.height/2,
        rect.w, rect.h, color);
}

static Rect scale_rect(Rect r, float a)
{
    r.x = lerpf(r.x, r.x + r.w*0.5f, 1.0f - a);
    r.y = lerpf(r.y, r.y + r.h*0.5f, 1.0f - a);
    r.w = lerpf(0.0f, r.w, a);
    r.h = lerpf(0.0f, r.h, a);
    return r;
}

static void fill_cell(Cell cell, u32 color, f32 a)
{
    fill_rect(scale_rect(cell_rect(cell), a), color);
}

static void fill_sides(Sides sides, u32 color)
{
    fill_rect(sides_rect(sides), color);
}

static Dir cells_dir(Cell a, Cell b)
{
    for (Dir dir = 0; dir < COUNT_DIRS; ++dir) {
        if (cell_eq(step_cell(a, dir), b)) return dir;
    }
    UNREACHABLE();
    return 0;
}

static Vec cell_center(Cell a)
{
    return (Vec) {
        .x = a.x*CELL_SIZE + CELL_SIZE/2,
        .y = a.y*CELL_SIZE + CELL_SIZE/2,
    };
}

static Sides slide_sides(Sides sides, Dir dir, f32 a)
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
    i32 col1 = (i32)(game.camera_pos.x - game.width/2 - CELL_SIZE)/CELL_SIZE;
    i32 col2 = (i32)(game.camera_pos.x + game.width/2 + CELL_SIZE)/CELL_SIZE;
    i32 row1 = (i32)(game.camera_pos.y - game.height/2 - CELL_SIZE)/CELL_SIZE;
    i32 row2 = (i32)(game.camera_pos.y + game.height/2 + CELL_SIZE)/CELL_SIZE;

    for (i32 col = col1; col <= col2; ++col) {
        for (i32 row = row1; row <= row2; ++row) {
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

static u32 color_alpha(u32 color, f32 a)
{
    return (color&0x00FFFFFF)|((u32)(a*0xFF)<<(3*8));
}

static void egg_render(void)
{
    if (game.eating_egg) {
        f32 t = 1.0f - game.step_cooldown/STEP_INTEVAL;
        f32 a = lerpf(1.5f, 1.0f, t*t);
        fill_cell(game.egg, color_alpha(EGG_COLOR, t*t), a);
    } else {
        fill_cell(game.egg, EGG_COLOR, 1.0f);
    }
}

static void dead_snake_render(void)
{
    // @tail-ignore
    for (u32 i = 1; i < game.dead_snake.size; ++i) {
        fill_rect(game.dead_snake.items[i], SNAKE_BODY_COLOR);
    }
}

void game_render(void)
{
    switch (game.state) {
    case STATE_GAMEPLAY: {
        background_render();
        egg_render();
        snake_render();
        platform_fill_text(SCORE_PADDING, SCORE_PADDING, game.score_buffer, SCORE_FONT_SIZE, SCORE_FONT_COLOR, ALIGN_LEFT);
    }
    break;

    case STATE_PAUSE: {
        background_render();
        egg_render();
        snake_render();
        platform_fill_text(SCORE_PADDING, SCORE_PADDING, game.score_buffer, SCORE_FONT_SIZE, SCORE_FONT_COLOR, ALIGN_LEFT);
        // TODO: "Pause", "Game Over" are not centered vertically
        platform_fill_text(game.width/2, game.height/2, "Pause", PAUSE_FONT_SIZE, PAUSE_FONT_COLOR, ALIGN_CENTER);
    }
    break;

    case STATE_GAMEOVER: {
        background_render();
        egg_render();
        dead_snake_render();
        platform_fill_text(SCORE_PADDING, SCORE_PADDING, game.score_buffer, SCORE_FONT_SIZE, SCORE_FONT_COLOR, ALIGN_LEFT);
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
        default:
        {}
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

static Vec vec_sub(Vec a, Vec b)
{
    return (Vec) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

static f32 vec_len(Vec a)
{
    return platform_sqrtf(a.x*a.x + a.y*a.y);
}

void game_resize(u32 width, u32 height)
{
    game.width = width;
    game.height = height;
}

void game_update(f32 dt)
{
#ifdef FEATURE_DYNAMIC_CAMERA
#define CAMERA_VELOCITY_FACTOR 0.50f
    game.camera_pos.x += game.camera_vel.x*CAMERA_VELOCITY_FACTOR*dt;
    game.camera_pos.y += game.camera_vel.y*CAMERA_VELOCITY_FACTOR*dt;
    game.camera_vel = vec_sub(
                          cell_center(*ring_back(&game.snake)),
                          game.camera_pos);
#endif

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
                game.state = STATE_GAMEOVER;

                game.dead_snake.size = game.snake.size;
                Vec head_center = cell_center(next_head);
                for (u32 i = 0; i < game.snake.size; ++i) {
#define GAMEOVER_EXPLOSION_RADIUS 1000.0f
#define GAMEOVER_EXPLOSION_MAX_VEL 200.0f
                    Cell cell = *ring_get(&game.snake, i);
                    game.dead_snake.items[i] = cell_rect(cell);
                    if (!cell_eq(cell, next_head)) {
                        Vec vel_vec = vec_sub(cell_center(cell), head_center);
                        f32 vel_len = vec_len(vel_vec);
                        f32 t = ilerpf(0.0f, GAMEOVER_EXPLOSION_RADIUS, vel_len);
                        if (t > 1.0f) t = 1.0f;
                        t = 1.0f - t;
                        f32 noise_x = (rand()%1000)*0.01;
                        f32 noise_y = (rand()%1000)*0.01;
                        vel_vec.x = vel_vec.x/vel_len*GAMEOVER_EXPLOSION_MAX_VEL*t + noise_x;
                        vel_vec.y = vel_vec.y/vel_len*GAMEOVER_EXPLOSION_MAX_VEL*t + noise_y;
                        game.dead_snake.vels[i] = vel_vec;
                        // TODO: additional velocities along the body of the dead snake
                    } else {
                        game.dead_snake.vels[i].x = 0;
                        game.dead_snake.vels[i].y = 0;
                    }
                }

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
    {} break;

    case STATE_GAMEOVER: {
        // @tail-ignore
        for (u32 i = 1; i < game.dead_snake.size; ++i) {
            game.dead_snake.vels[i].x *= 0.99f;
            game.dead_snake.vels[i].y *= 0.99f;
            game.dead_snake.items[i].x += game.dead_snake.vels[i].x*dt;
            game.dead_snake.items[i].y += game.dead_snake.vels[i].y*dt;
        }
    }
    break;

    default: {
        UNREACHABLE();
    }
    }
}

// TODO: indicate the borders of the snake's body
// TODO: inifinite field mechanics
// TODO: moving around egg
