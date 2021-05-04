#!/usr/bin/env python3

import sys
import re

name = sys.argv[1].replace(".so", ".c", 1)
with open(name, "r") as f:
    lines = f.readline() + f.readline()
    pattern = re.compile(r"--\s*link\s*:\s*(\w+)", re.MULTILINE)

    for match in pattern.findall(lines):
        if match != "":
            print("-l" + match, end=" ")
