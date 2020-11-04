#!/usr/bin/env sh

set -e

gcc -I$(pwd)/src -Wall -Wextra ./src/*.c
