import re
import os
import struct
import subprocess


class LayoutBuilder:
    def __init__(self, tml_file="layout.tml", bin_file="layout.bin", obj_file="layout.o"):
        self.tml_file = tml_file
        self.bin_file = bin_file
        self.obj_file = obj_file
        self.size = 0
        self.content = b""
        self.layout_table = []

        self.color_map = {
            "white": "0xFFFF", "black": "0x0000", "red": "0xF800",
            "green": "0x07E0", "blue": "0x001F", "cyan": "0x07FF",
            "magenta": "0xF81F", "yellow": "0xFFE0", "gray": "0x8410",
            "orange": "0xFC00",
        }

        self.align_map = {
            "center": "0x00",
            "right": "0x01",
            "left": "0x02"
        }

        self.font_map = {
            "small": "0x00",
            "medium": "0x01",
            "large": "0x02"
        }

    def _pad_to_4(self, f):
        padding = (4 - (f.tell() % 4)) % 4
        f.write(b'\x00' * padding)

    def _hex_to_rgb565(self, value):
        value = value.lstrip("#")
        if len(value) == 6:
            r, g, b = int(value[:2], 16), int(value[2:4], 16), int(value[4:], 16)
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            return f"0x{rgb565:04X}"
        return self.color_map.get(value.lower(), f'"{value}"')

    def _hash_id(self, text):
        h = 5381
        for c in text.encode('latin1'):
            h = ((h << 5) + h) + c
        return h & 0xFFFFFFFF

    def _clean_content(self, raw):
        # Replace colors
        raw = re.sub(r'color:\s*"(#[\da-fA-F]{6}|[a-zA-Z]+)"',
                     lambda m: f'color:{self._hex_to_rgb565(m.group(1))}', raw)
        raw = re.sub(r'background:\s*"(#[\da-fA-F]{6}|[a-zA-Z]+)"',
                     lambda m: f'background:{self._hex_to_rgb565(m.group(1))}', raw)

        # Replace align
        raw = re.sub(r'align:\s*([a-zA-Z]+)',
                     lambda m: f'align:{self.align_map.get(m.group(1), m.group(1))}' ,raw)

        # Replace font
        raw = re.sub(r'font:\s*([a-zA-Z]+)',
                     lambda m: f'font:{self.font_map.get(m.group(1), m.group(1))}' ,raw)
        
        # Remove double qoutes
        raw = re.sub(r'"(.*?)"', r'\1', raw)

        # Normalize lines
        lines = raw.splitlines()
        cleaned = []
        for line in lines:
            stripped = line.strip()
            if not stripped:
                continue
            if '{' in stripped or '}' in stripped:
                cleaned.append(stripped.replace(" ", "").replace("\t", ""))
            else:
                parts = stripped.split(':')
                cleaned.append(':'.join(p.strip() for p in parts) if len(parts) > 1 else stripped)
        return '\n'.join(cleaned)

    def _insert_area_markers(self, content):
        pattern = r'(Area\s*{)'
        area_positions = list(re.finditer(pattern, content))
        modified = content[:area_positions[0].start()]

        for match in area_positions:
            start = match.start()
            block_start = start + len(match.group()) + 1

            open_braces = 1
            pos = block_start
            while pos < len(content) and open_braces > 0:
                if content[pos] == '{':
                    open_braces += 1
                elif content[pos] == '}':
                    open_braces -= 1
                pos += 1

            area_content = content[start:pos]
            s = len("Area{") + 1
            e = len(area_content) - 1

            modified += "<START>\n" 
            modified += area_content[s:e]
            modified += "<END>\n"

        return modified

    def _count_placeholders(self, text):
        return len(list(re.finditer(r'\$([a-zA-Z0-9_]+)', text)))

    def _process_layouts(self, content):
        root_match = re.search(r'Root\s*{', content)
        layout_matches = list(re.finditer(r'Layout\s*{', content))

        if not root_match or not layout_matches:
            print("[❌] Missing Root or Layout")
            return ""

        output = content[root_match.start():layout_matches[0].start()]
        for i, layout in enumerate(layout_matches):
            start = layout.start()
            end = layout_matches[i + 1].start() if i + 1 < len(layout_matches) else len(content)
            layout_text = content[start:end - 1]
            processed = self._insert_area_markers(layout_text)
            output += processed
        return output

    def _strip_braces(self, content):
        return '\n'.join(
            line for line in content.splitlines()
            if line.strip() and '{' not in line and '}' not in line
        )

    def _build_layout_table(self, content):
        layout_matches = list(re.finditer(r'id\s*:', content))

        for i, match in enumerate(layout_matches):
            start = match.start()
            end = layout_matches[i + 1].start() if i + 1 < len(layout_matches) else len(content)
            layout_text = content[start:end - 1]

            id_match = re.search(r'id\s*:\s*(\w+)', layout_text)
            if not id_match:
                continue
            layout_id = id_match.group(1)
            self.layout_table.append({
                "id": layout_id,
                "offset": start,
                "size": end - start - 1,
                "area_count": layout_text.count('<START>'),
                "ph_cnt": self._count_placeholders(layout_text),
            })

    def _write_binary(self):
        with open(self.bin_file, 'wb') as f:
            f.write(struct.pack('<I', self.size))
            f.write(struct.pack('<I', len(self.layout_table)))
            f.write(self.content)
            self._pad_to_4(f)

            for entry in self.layout_table:
                f.write(struct.pack('<IHHBB',
                                    self._hash_id(entry['id']),
                                    entry['offset'],
                                    entry['size'],
                                    entry['area_count'],
                                    entry['ph_cnt']))
        print(f"[✅] layout.bin generated with {len(self.layout_table)} layouts")

    def _generate_object_file(self):
        cmd = [
            "arm-none-eabi-objcopy", "-I", "binary", "-O", "elf32-littlearm", "-B", "arm",
            "--rename-section", ".data=.rodata",
            "--redefine-sym", "_binary_layout_bin_start=layout_data_start",
            "--redefine-sym", "_binary_layout_bin_end=layout_data_end",
            "--redefine-sym", "_binary_layout_bin_size=layout_data_size",
            self.bin_file, self.obj_file
        ]
        try:
            subprocess.run(cmd, check=True)
            print("[🔧] layout.o created successfully")
        except subprocess.CalledProcessError as e:
            print("[❌] Objcopy failed:", e)
            return False
        return True

    def build(self):
        if not os.path.exists(self.tml_file):
            print(f"[❌] File not found: {self.tml_file}")
            return False

        with open(self.tml_file, 'r', encoding='utf-8') as f:
            raw = f.read()

        cleaned = self._clean_content(raw)
        modified = self._process_layouts(cleaned)
        final_content = self._strip_braces(modified)

        print(final_content)

        self.content = final_content.encode('utf-8')
        self.size = len(self.content)
        self._build_layout_table(final_content)
        self._write_binary()

        return self._generate_object_file()


if __name__ == "__main__":
    os.chdir(os.path.dirname(__file__))
    builder = LayoutBuilder()
    builder.build()
