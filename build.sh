#!/bin/sh

set -xe

cc -Wall -Wextra -o sdl_main sdl_main.c -lSDL2
clang --target=wasm32 --no-standard-libraries -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined  -o game.wasm game.c
