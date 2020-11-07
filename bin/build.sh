#!/usr/bin/env sh

set -e

#gcc -I$(pwd)/src -Wall -Wextra -Wno-unused-parameter -Wno-switch ./src/*.c

gcc -Wall -Wextra -Wno-unused-parameter -g -std=c99 -I$(pwd)/include -I$(pwd)/src src/*.c cli/main.c -o bin/main -lm
