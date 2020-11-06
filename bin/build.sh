#!/usr/bin/env sh

set -e

gcc -I$(pwd)/src -Wall -Wextra -Wno-unused-parameter ./src/*.c
