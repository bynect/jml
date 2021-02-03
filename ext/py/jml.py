#!/usr/bin/env python3
# -*- coding: utf-8 -*-


from distutils.core import setup, Extension
from pathlib import Path
from subprocess import run, CalledProcessError
import os


def jml_setup() -> None:
    setup(
        name = "jml",
        version = "0.1.0",
        description = "Python interface for the jml C interpreter.",
        author = "bynect",
        ext_modules = [
            Extension(
                name = "jml",
                language = "c",
                define_macros = [
                    ("JML_NDEBUG", None)
                ],
                include_dirs = [
                    "include"
                ],
                library_dirs = [
                    "lib"
                ],
                libraries = [
                    "jml"
                ],
                extra_compile_args = [
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Wno-unused-parameter",
                    "-std=c99"
                ],
                sources = [
                    "ext/py/jml_py.c"
                ],
            )
        ]
    )


def jml_dll() -> None:
    if os.name == 'posix':
        if not Path("lib/libjml.so").exists():
            try:
                run(["make", "-f", "tool/make/Makefile", "all"])
            except CalledProcessError:
                print("Shared library creation failed")


if __name__ == "__main__":
    jml_dll()
    jml_setup()
