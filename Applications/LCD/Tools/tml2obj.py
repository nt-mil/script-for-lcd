import re
import subprocess
import shutil
import os
import sys

class LayoutConverter:
    def __init__(self, tml_file="layout.tml", bin_file="layout.bin", obj_file="layout.o"):
        """Initialize the LayoutConverter with file paths."""
        self.tml_file = tml_file
        self.bin_file = bin_file
        self.obj_file = obj_file
        self.success = False
        # Extended color name to RGB565 mapping
        self.color_map = {
            "white": "0xFFFF",  # White
            "black": "0x0000",  # Black
            "red": "0xF800",    # Red
            "green": "0x07E0",  # Green
            "blue": "0x001F",   # Blue
            "cyan": "0x07FF",   # Cyan
            "magenta": "0xF81F",# Magenta
            "yellow": "0xFFE0", # Yellow
            "gray": "0x8410",   # Gray
            "orange": "0xFC00", # Orange
            # Add more colors as needed
        }

    def _hex_to_rgb565(self, hex_str):
        """Convert #RRGGBB to RGB565 format or return mapped color if not a hex."""
        hex_str = hex_str.lstrip('#')
        if len(hex_str) == 6 and all(c in '0123456789abcdefABCDEF' for c in hex_str):
            r = int(hex_str[0:2], 16)
            g = int(hex_str[2:4], 16)
            b = int(hex_str[4:6], 16)
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            return f"0x{rgb565:04X}"
        elif hex_str.lower() in self.color_map:
            return self.color_map[hex_str.lower()]
        else:
            print(f"[⚠] Unknown color format or name: {hex_str}, keeping as is")
            return f'"{hex_str}"'  # Preserve original string if unrecognized

    def _check_duplicate_ids(self, contents):
        """Check for duplicate IDs in the layout file."""
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
            return False
        print("[✅] No duplicate IDs found")
        return True

    def _remove_spaces_and_tabs(self, content):
        """Remove all space and tab characters while preserving newlines and ensuring { and } format."""
        lines = content.splitlines()
        formatted_lines = []
        for line in lines:
            stripped = line.strip()  # Remove leading/trailing spaces and tabs
            if stripped:  # Only process non-empty lines
                # Ensure proper structure with { and }
                if '{' in stripped:
                    formatted_lines.append(stripped.replace(' ', '').replace('\t', ''))
                elif '}' in stripped:
                    formatted_lines.append(stripped.replace(' ', '').replace('\t', ''))
                else:
                    # Split by colon, remove spaces, and join back
                    parts = stripped.split(':')
                    if len(parts) > 1:
                        formatted_lines.append(':'.join(part.strip() for part in parts if part.strip()))
                    else:
                        formatted_lines.append(stripped)

        # Join lines with newlines, ensuring no trailing newline
        return '\n'.join(formatted_lines)

    def convert_tml_to_bin(self):
        """Convert TML file to binary file with validation checks."""
        try:
            with open(self.tml_file, 'rt', encoding='utf-8') as f:
                contents = f.read()
        except FileNotFoundError:
            print(f"[❌] File not found: {self.tml_file}")
            return False

        if not self._check_duplicate_ids(contents):
            return False

        try:
            with open(self.bin_file, 'wb') as f:
                f.write(contents.encode('utf-8'))
            print("[📦] Binary file created:", self.bin_file)
            return True
        except Exception as e:
            print(f"[❌] Failed to write binary file: {e}")
            return False

    def tml_to_bin_with_color_conversion(self):
        """Convert TML file to binary file with color and background conversion and space removal."""
        try:
            with open(self.tml_file, 'r', encoding='utf-8') as f:
                contents = f.read()
        except FileNotFoundError:
            print(f"[❌] File not found: {self.tml_file}")
            return False

        # Find and replace all 'color': "#RRGGBB" or 'color': "colorname" and 'background': "#RRGGBB" or 'background': "colorname"
        pattern_color = re.compile(r'color:\s*"(#[0-9a-fA-F]{6}|[a-zA-Z]+)"')
        pattern_background = re.compile(r'background:\s*"(#[0-9a-fA-F]{6}|[a-zA-Z]+)"')
        contents_converted = pattern_color.sub(lambda m: f'color:{self._hex_to_rgb565(m.group(1))}', contents)
        contents_converted = pattern_background.sub(lambda m: f'background:{self._hex_to_rgb565(m.group(1))}', contents_converted)

        # Remove spaces and tabs, storing in a variable
        modified_content = self._remove_spaces_and_tabs(contents_converted)

        try:
            with open(self.bin_file, 'wb') as f:
                f.write(modified_content.encode('utf-8'))
            print(f"[✅] Converted color/background, removed spaces, and written to binary: {self.bin_file}")
            return True
        except Exception as e:
            print(f"[❌] Failed to write binary file with color conversion: {e}")
            return False

    def convert_bin_to_obj(self):
        """Convert binary file to object file using objcopy."""
        cmd = [
            "arm-none-eabi-objcopy",
            "-I", "binary",
            "-O", "elf32-littlearm",
            "-B", "arm",
            "--rename-section", ".data=.rodata",
            "--redefine-sym", "_binary_layout_bin_start=layout_data_start",
            "--redefine-sym", "_binary_layout_bin_end=layout_data_end",
            self.bin_file, self.obj_file
        ]
        try:
            subprocess.run(cmd, check=True)
            print("[🔧] Object file generated:", self.obj_file)
            return True
        except subprocess.CalledProcessError as e:
            print("[❌] Objcopy failed:", e)
            return False

    def move_obj_to_parent(self):
        """Move the object file to the parent directory."""
        parent_dir = os.path.abspath(os.path.join(os.getcwd(), ".."))
        dest_file = os.path.join(parent_dir, os.path.basename(self.obj_file))
        try:
            shutil.move(self.obj_file, dest_file)
            print(f"[📂] Moved {self.obj_file} to {dest_file}")
            return True
        except Exception as e:
            print(f"[❌] Failed to move {self.obj_file}: {e}")
            return False

    def process(self):
        """Execute the full conversion process with color and background conversion and space removal."""
        if self.tml_to_bin_with_color_conversion() and self.convert_bin_to_obj() and self.move_obj_to_parent():
            self.success = True
            print("[✅] Done.")
        else:
            print("[❌] Process failed.")
            self.success = False

if __name__ == "__main__":
    converter = LayoutConverter()
    converter.process()