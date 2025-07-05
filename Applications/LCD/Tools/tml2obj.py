import re
import os
import struct
import subprocess

class LayoutBuilder:
    def __init__(self, tml_file="layout.tml", bin_file="layout.bin", obj_file='layout.o'):
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

    def _hex_to_rgb565(self, hex_str):
        hex_str = hex_str.lstrip('#')
        if len(hex_str) == 6:
            r = int(hex_str[0:2], 16)
            g = int(hex_str[2:4], 16)
            b = int(hex_str[4:6], 16)
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            return f"0x{rgb565:04X}"
        return self.color_map.get(hex_str.lower(), f'"{hex_str}"')

    def _hash_id(self, id_str):
        h = 5381
        for c in id_str:
            h = ((h << 5) + h) + ord(c)
        return h & 0xFFFFFFFF

    def _clean_content(self, content):
        # Replace color and background fields
        content = re.sub(r'color:\s*"(#[\da-fA-F]{6}|[a-zA-Z]+)"',
                         lambda m: f'color:{self._hex_to_rgb565(m.group(1))}', content)
        content = re.sub(r'background:\s*"(#[\da-fA-F]{6}|[a-zA-Z]+)"',
                         lambda m: f'background:{self._hex_to_rgb565(m.group(1))}', content)

        # Remove tabs/spaces while keeping syntax clean
        lines = content.splitlines()
        cleaned = []
        for line in lines:
            stripped = line.strip()
            if not stripped:
                continue
            if '{' in stripped or '}' in stripped:
                cleaned.append(stripped.replace(' ', '').replace('\t', ''))
            else:
                parts = stripped.split(':')
                cleaned.append(':'.join(p.strip() for p in parts if p.strip()) if len(parts) > 1 else stripped)
        return '\n'.join(cleaned)

    def _find_placeholders(self, layout_id, layout_text, layout_offset):
        result = []
        for match in re.finditer(r'\$([a-zA-Z0-9_]+)', layout_text):
            result.append({
                'layout_id': layout_id,
                'name': match.group(1),
                'offset': layout_offset + match.start(),
                'length': len(match.group(0))
            })
        return result

    def _find_root(self, content):
        root = re.search(r'Root\s*{', content)
        layout = re.search(r'Layout\s*{', content)

        if not root or not layout:
            print("[❌] Cannot find Root or first Layout block")
            return 0

        root_text = content[root.start():layout.start()] + '}'
        id_match = re.search(r'id\s*:\s*(\w+)', root_text)
        layout_id = id_match.group(1) if id_match else "root"
        offset = 0

        self.layout_table.append({
            'id': layout_id,
            'offset': offset,
            'size': layout.start(),
            'area_count': 0,
            'place_holder': self._find_placeholders(layout_id, root_text, offset)
        })

        return root.start()

    def _find_layouts(self, content):
        matches = list(re.finditer(r'Layout\s*{', content))
        for i, match in enumerate(matches):
            start = match.start()
            end = matches[i + 1].start() if i + 1 < len(matches) else len(content)
            layout_text = content[start:end]
            id_match = re.search(r'id\s*:\s*(\w+)', layout_text)
            if not id_match:
                continue

            layout_id = id_match.group(1)
            area_count = len(re.findall(r'\bArea\s*{', layout_text))
            self.layout_table.append({
                'id': layout_id,
                'offset': start,
                'size': end - start,
                'area_count': area_count,
                'place_holder': self._find_placeholders(layout_id, layout_text, start)
            })

    def _save_layouts(self):
        with open(self.bin_file, 'wb') as f:
            # Write layout content first
            f.write(struct.pack('<I', self.size))
            f.write(struct.pack('<I', len(self.layout_table)))
            f.write(self.content)

            for entry in self.layout_table:
                layout_id = self._hash_id(entry['id'])
                f.write(struct.pack('<IIIII',
                                    layout_id,
                                    entry['offset'],
                                    entry['size'],
                                    entry['area_count'],
                                    len(entry['place_holder'])))

                for ph in entry['place_holder']:
                    name_bytes = ph['name'].encode('utf-8')
                    f.write(struct.pack('<I', len(name_bytes)))
                    f.write(name_bytes)
                    f.write(struct.pack('<II', ph['offset'], ph['length']))

        print(f"[✅] layout.bin generated with {len(self.layout_table)} layouts")

    def build(self):
        if not os.path.exists(self.tml_file):
            print(f"[❌] File not found: {self.tml_file}")
            return False

        with open(self.tml_file, 'r', encoding='utf-8') as f:
            raw = f.read()

        cleaned = self._clean_content(raw)
        root_start = self._find_root(cleaned)
        cleaned = cleaned[root_start:]

        self.content = cleaned.encode('utf-8')
        self.size = len(self.content)

        self._find_layouts(cleaned)
        self._save_layouts()

        cmd = [
            "arm-none-eabi-objcopy",
            "-I", "binary",
            "-O", "elf32-littlearm",
            "-B", "arm",
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


if __name__ == "__main__":
    os.chdir(os.path.dirname(__file__))
    builder = LayoutBuilder()
    builder.build()
