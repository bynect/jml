# -*- coding: utf-8 -*-
# python .\tool\msvc\defgen.py


from subprocess import Popen, PIPE
import sys
import os
import glob
import re


def jml_defgen() -> None:
    def sub_command(sub, cmd) -> None:
        sub.stdin.write(f"{cmd}\r\n".encode())

    def parse_symbols(raw: str, out_name: str) -> None:
        str_1 = f"Dump of file {out_name}"
        str_2 = "Archive member name at"
        str_3 = "correct header end\r\n\r\n    122 public symbols\r\n\r\n"

        start = raw.find(str_1)
        start = raw.find(str_3, start) + len(str_3)
        stop = raw.find(str_2, start + len(str_2)) - 4

        return re.findall(r"^(?:[a-fA-F\d ]{10})(\w*)", raw[start:stop], re.M)

    # config
    src_dir = "src"
    out_dir = "lib"
    out_name = "lib\\tmpstatic.lib"
    out_name2 = out_name.replace(".lib", "") + "__2"
    def_name = "tool/msvc/jml.def"
    cur_dir = os.getcwd()

    includes = "/I src /I include"
    sources = list(glob.glob(f"{src_dir}/*.c"))
    objects = [f.replace(".c", ".obj").replace(src_dir, out_dir) for f in sources]

    sub = Popen(["cmd.exe"], stdin=PIPE, stdout=PIPE, stderr=sys.stderr)
    sub_command(sub, f'cd "{cur_dir}"')

    # init msvc
    vcvarsall = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat"
    sub_command(sub, f'"{vcvarsall}" amd64')

    clexe = f"cl.exe {src_dir}/*.c {includes} /Fo{out_dir}\\ /EHsc /LD /Fe{out_name2} /DEF {def_name} /link /out:{out_name2}.dll"
    sub_command(sub, clexe)

    sub_command(sub, f"lib {out_dir}/*.obj /out:{out_name}")
    sub_command(sub, f"dumpbin /LINKERMEMBER {out_name}")

    sub.stdin.close()
    output = sub.stdout.read().decode()

    symbols = [
        sym + "\n" for sym in parse_symbols(output, out_name) if sym.startswith("jml")
    ]

    with open(def_name, "w") as file:
        file.write("EXPORTS\n")
        file.writelines(symbols)

    # cleanup
    os.remove(out_name)

    for ext in [".exp", ".dll", ".lib"]:
        try:
            os.remove(out_name2 + ext)
        except:
            pass

    for obj in objects:
        try:
            os.remove(obj)
        except:
            pass


if __name__ == "__main__":
    if sys.platform.startswith("win"):
        jml_defgen()
    else:
        print("windows only")
