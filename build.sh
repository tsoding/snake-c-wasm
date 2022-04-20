#!/bin/sh

set -xe

clang -Wall -Wextra -c game.c
clang -Wall -Wextra -o sdl_main sdl_main.c game.o -lSDL2

clang -Os -fno-builtin -Wall -Wextra --target=wasm32 --no-standard-libraries -Wl,--export=game_init -Wl,--export=game_render -Wl,--export=game_update -Wl,--export=game_info -Wl,--no-entry -Wl,--allow-undefined  -o game.wasm game.c
