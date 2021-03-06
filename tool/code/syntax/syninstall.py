# -*- coding: utf-8 -*-
# python .\tool\code\syntax\syninstall.py


from pathlib import Path
import glob, os, shutil


extensions = Path().home() / ".vscode" / "extensions"
folder = "jml.syntax-highlighting"

source = "tool/code/syntax"
dest = extensions / folder

dest.mkdir(parents=True, exist_ok=True)

files = glob.iglob(os.path.join(source, "*.json"))

for file in files:
    shutil.copy2(file, str(dest))
