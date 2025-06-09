import re
import subprocess
import shutil
import os
import sys

def tml2bin(tml_file, bin_file):
    try:
        with open(tml_file, 'rt', encoding='utf-8') as f:
            contents = f.read()
    except FileNotFoundError:
        print(f"[❌] File not found: {tml_file}")
        sys.exit(1)

    # 1. Check duplicated IDs
    ids = re.findall(r'\bid\s*:\s*(\w+)', contents)
    seen = set()
    duplicates = set()
    for id_ in ids:
        if id_ in seen:
            duplicates.add(id_)
        else:
            seen.add(id_)

    if duplicates:
        print(f"[❌] Duplicate IDs found: {', '.join(duplicates)}")
        sys.exit(1)
    else:
        print("[✅] No duplicate IDs found")

    # 2. Check font before text
    lines = contents.splitlines()
    stack = []
    block_errors = []

    for i, line in enumerate(lines):
        stripped = line.strip()
        if '{' in stripped:
            stack.append({'lines': [], 'start_line': i + 1})
        elif '}' in stripped and stack:
            block = stack.pop()
            block_lines = block['lines']
            font_line = text_line = -1
            for idx, content in enumerate(block_lines):
                if 'font:' in content and font_line == -1:
                    font_line = idx
                if 'text:' in content and text_line == -1:
                    text_line = idx
            if text_line != -1 and (font_line == -1 or font_line > text_line):
                block_errors.append((block['start_line'], block_lines))
        elif stack:
            stack[-1]['lines'].append(stripped)

    if block_errors:
        print(f"[❌] Found {len(block_errors)} block(s) where 'text' appears before 'font':")
        for line_no, block in block_errors:
            print(f" - At block starting at line {line_no}:\n" + "\n".join(block) + "\n")
        sys.exit(1)
    else:
        print("[✅] All text blocks have 'font' declared before 'text'")

    # 3. Write binary file
    with open(bin_file, 'wb') as f:
        f.write(contents.encode('utf-8'))
    print("[📦] Binary file created:", bin_file)

def bin2obj(bin_file, obj_file):
    cmd = [
        "arm-none-eabi-objcopy",
        "-I", "binary",
        "-O", "elf32-littlearm",
        "-B", "arm",
        "--rename-section", ".data=.rodata",
        bin_file, obj_file
    ]
    try:
        subprocess.run(cmd, check=True)
        print("[🔧] Object file generated:", obj_file)
    except subprocess.CalledProcessError as e:
        print("[❌] Objcopy failed:", e)
        sys.exit(1)

def move_obj_to_parent(obj_file):
    parent_dir = os.path.abspath(os.path.join(os.getcwd(), ".."))
    dest_file = os.path.join(parent_dir, os.path.basename(obj_file))
    try:
        shutil.move(obj_file, dest_file)
        print(f"[📂] Moved {obj_file} to {dest_file}")
    except Exception as e:
        print(f"[❌] Failed to move {obj_file}: {e}")
        sys.exit(1)

if __name__ == "__main__":
    tml_file = "layout.tml"
    bin_file = "layout.bin"
    obj_file = "layout.o"

    tml2bin(tml_file, bin_file)
    bin2obj(bin_file, obj_file)
    move_obj_to_parent(obj_file)

    print("[✅] Done.")
