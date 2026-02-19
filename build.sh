#!/usr/bin/env sh

g++ camera.cpp \
    -I/opt/homebrew/Cellar/sdl3/3.4.0/include \
    -L/opt/homebrew/Cellar/sdl3/3.4.0/lib -lSDL3 \
    -g -Wall -Wextra -O0 -D_GLIBCXX_ASSERTIONS -fsanitize=address
