typedef unsigned int u32;
typedef int i32;

#define FACTOR 100
#define WIDTH  (16*FACTOR)
#define HEIGHT (9*FACTOR)
#define CELL_SIZE 100
#define COLS (WIDTH/CELL_SIZE)
#define ROWS (HEIGHT/CELL_SIZE)

#define BACKGROUND_COLOR 0xFF181818
#define CELL1_COLOR BACKGROUND_COLOR
#define CELL2_COLOR 0xFF183018
#define SNAKE_BODY_COLOR 0xFF189018

#define STEP_INTEVAL 0.1f

void platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color);

#define SNAKE_CAP (ROWS*COLS)

typedef enum {
    DIR_RIGHT = 0,
    DIR_UP,
    DIR_LEFT,
    DIR_DOWN,
    COUNT_DIRS,
} Dir;

typedef struct {
    i32 x, y;
} Cell;

static const Cell dir_vecs[COUNT_DIRS] = {
    [DIR_RIGHT] = {.x =  1},
    [DIR_UP]    = {.y = -1},
    [DIR_LEFT]  = {.x = -1},
    [DIR_DOWN]  = {.y =  1},
};

typedef struct {
    Cell body[SNAKE_CAP];
    u32 begin;
    u32 size;
} Snake;

typedef struct {
    Snake snake;
    Dir dir;
    float step_cooldown;
} Game;

static Game game = {
    .snake = {
        .size = 3,
        .body = {
            [0] = {.x = 0, .y = ROWS/2},
            [1] = {.x = 1, .y = ROWS/2},
            [2] = {.x = 2, .y = ROWS/2},
        }
    },
    .dir = DIR_RIGHT,
};

int game_width(void)
{
    return WIDTH;
}

int game_height(void)
{
    return HEIGHT;
}

static void snake_render(Snake *snake)
{
    for (u32 offset = 0; offset < snake->size; ++offset) {
        u32 index = (snake->begin + offset)%SNAKE_CAP;
        Cell *cell = &snake->body[index];
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

static void game_step_snake(void)
{
    
}

void game_render(void)
{
    background_render();
    snake_render(&game.snake);
}

void game_update(float dt)
{
    game.step_cooldown -= dt;
    if (game.step_cooldown <= 0.0f) {
        game_step_snake();
        game.step_cooldown = STEP_INTEVAL;
    }
}
