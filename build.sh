#!/bin/sh

set -xe

clang -Wall -Wextra -Wswitch-enum -c game.c
clang -Wall -Wextra -Wswitch-enum -o sdl_main sdl_main.c game.o -lSDL2 -lSDL2_ttf -lm
clang -Wall -Wextra -Wswitch-enum -I./include/ -o raylib_main raylib_main.c game.o -L./lib/ -lraylib -lm

clang -Os -fno-builtin -Wall -Wextra -Wswitch-enum --target=wasm32 --no-standard-libraries -Wl,--export=game_init -Wl,--export=game_render -Wl,--export=game_update -Wl,--export=game_info -Wl,--export=game_keydown -Wl,--no-entry -Wl,--allow-undefined  -o game.wasm game.c
