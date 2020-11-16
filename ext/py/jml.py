#!/usr/bin/env python3

from distutils.core import setup, Extension


def jml_setup():
    setup(
        name = "jml",
        version = "0.1.0",
        description = "Python interface for the jml C interpreter.",
        author = "bynect",
        ext_modules = [
            Extension(
                name = "jml",
                language = "c",
                define_macros = [("JML_NDEBUG", None)],
                include_dirs = [
                    "src", "include"
                ],
                libraries = ["m", "dl"],
                extra_compile_args = [
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Wno-unused-parameter",
                    "-std=c99"
                ],
                sources = [
                    "ext/py/jml_py.c",
                    "src/jml_bytecode.c",
                    "src/jml_compiler.c",
                    "src/jml_core.c",
                    "src/jml_gc.c",
                    "src/jml_lexer.c",
                    "src/jml_module.c",
                    "src/jml_type.c",
                    "src/jml_util.c",
                    "src/jml_value.c",
                    "src/jml_vm.c"
                ],
            )
        ]
    )


if __name__ == "__main__":
    jml_setup()
