#!/bin/sh

set -xe

if [ -d /acme ]; then
  # compiling on plan9
  pcc -c game.c
  6c plan9_main.c
  6l -o plan9_main game.6 plan9_main.6 -lttf -lbio
  exit 0
else
  # compiling on unix - using plan9port for graphics (https://9fans.github.io/plan9port/)
  9c game.c
  9c plan9_main.c
  9l -o plan9port_main game.o plan9_main.o
  exit 0
fi

clang -Wall -Wextra -Wswitch-enum -c game.c
clang -Wall -Wextra -Wswitch-enum -o sdl_main sdl_main.c game.o -lSDL2 -lSDL2_ttf -lm
clang -Wall -Wextra -Wswitch-enum -I./include/ -o raylib_main raylib_main.c game.o -L./lib/ -lraylib -lm

clang -Os -fno-builtin -Wall -Wextra -Wswitch-enum --target=wasm32 --no-standard-libraries -Wl,--export=game_init -Wl,--export=game_render -Wl,--export=game_update -Wl,--export=game_info -Wl,--export=game_keydown -Wl,--no-entry -Wl,--allow-undefined  -o game.wasm game.c
