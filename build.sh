#!/bin/sh

set -xe

clang -Wall -Wextra -c game.c
clang -Wall -Wextra -o sdl_main sdl_main.c game.o -lSDL2

# TODO: find alternative to --export-all to let the compiler know what's the API and what's private stuff so it can optimize things better
clang -Wall -Wextra --target=wasm32 --no-standard-libraries -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined  -o game.wasm game.c
