from PIL import Image, ImageDraw, ImageFont
from argparse import ArgumentParser
from math import ceil
from sys import hexversion as python_version

if python_version < 0x030A00F0:
    raise RuntimeError('Unsupported Python version. Python 3.10+ required')

class FontGenerator:
    def __init__(self):
        """Initialize the FontGenerator with argument parsing."""
        self.arg_parser = ArgumentParser(description='Generate C code from TrueType font')
        self.arg_parser.add_argument('-v', '--version', action='version', version="0.0.1")
        self.arg_parser.add_argument('-f', '--font', required=True, type=str, help="Font file with extension (arial.ttf)")
        self.arg_parser.add_argument('-s', '--size', required=True, type=int, help="Size of font in px. Note: actual size of bitmap may be different")
        self.arg_parser.add_argument('-p', '--proportional', action='store_true', help="Generate array of char width (for non-monospaced fonts)")
        self.arg_parser.add_argument('-a', '--atlas', type=str, help="Font atlas file with extension (e.g. atlas.png)")
        self.arg_parser.add_argument('--charset', default="ascii", type=str, help="Specific charset (not supported by library)")
        self.arg_parser.add_argument('--string', type=str)
        self.args = self.arg_parser.parse_args()

        """ Charsets """
        self.ascii = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        self._set_charset()
        self.font = None
        self.x_min = self.args.size * 3
        self.x_max = 0
        self.y_min = self.args.size * 3
        self.y_max = 0
        self.widths = []
        self.res = None
        self.pixels = []

    def _set_charset(self):
        """Set the charset based on the provided argument."""
        match self.args.charset:
            case "ascii":
                self.charset = [char for char in self.ascii]
            case _:
                with open(self.args.charset, 'r', encoding='utf-8') as file:
                    self.charset = [char for char in file.read()]

    def load_font(self):
        """Load the TrueType font file."""
        try:
            self.font = ImageFont.truetype(self.args.font, self.args.size)
        except OSError:
            print("Cannot open font")
            exit(1)

    def calculate_bounding_box(self):
        """Calculate the font boundary box and character widths."""
        for char in self.charset:
            bbox = self.font.getbbox(char, '1')
            if bbox:
                self.x_min = min(self.x_min, bbox[0])
                self.y_min = min(self.y_min, bbox[1])
                self.x_max = max(self.x_max, bbox[2])
                self.y_max = max(self.y_max, bbox[3])
                self.widths.append(bbox[2])
            else:
                self.widths.append(0)
        self.widths = [w - self.x_min if w >= self.x_min else 0 for w in self.widths]
        self.res = (ceil((self.x_max - self.x_min) / 16) * 16, self.y_max - self.y_min)

    def convert_to_bytes(self):
        """Convert font characters to byte arrays."""
        self.pixels = []
        for char in self.charset:
            out = Image.new("1", self.res, 0)
            d = ImageDraw.Draw(out)
            d.text((-self.x_min, -self.y_min), char, font=self.font, fill=1)
            self.pixels.append(out.tobytes())

    def generate_font_c(self):
        """Generate C code for the font."""
        if not self.args.string:
            with open("font.c", "w", encoding='utf-8') as fd:
                fnt_name = self.font.getname()
                fd.write(f"/** Generated {fnt_name[0]} {fnt_name[1]} {self.args.size} "
                         "file by generate.py*/\n")
                words = [[(char[byte], char[byte + 1] if len(char) >= byte + 1 else 0)
                         for byte in range(0, len(char), 2)] for char in self.pixels]
                fd.write("#include <stdint.h>\n")
                fd.write("#include \"ssd1306_fonts.h\"\n\n")
                fd.write(f"static const uint16_t Font{(self.x_max - self.x_min)}x{self.res[1]} [] = {{\n")
                for index, char in enumerate(words):
                    assert (len(char) == (self.res[1] * ceil((self.x_max - self.x_min) / 16)))
                    fd.write(f"/** {self.charset[index]} **/\n")
                    for word in char:
                        fd.write(f"0x{word[0]:02X}{word[1]:02X},")
                    fd.write("\n")
                fd.write("};\n\n")
                if self.args.proportional:
                    fd.write("static const uint8_t char_width[] = {\n")
                    for index, width in enumerate(self.widths):
                        fd.write(f"  {width},  /** {self.charset[index]} **/\n")
                    fd.write("};\n\n")
                fd.write(f"/** Generated {fnt_name[0]} {fnt_name[1]} {self.args.size} */\n")
                fd.write(f"const SSD1306_Font_t Font_{(self.x_max - self.x_min)}x{self.res[1]} = "
                         f"{{{self.x_max - self.x_min}, {self.res[1]}, Font{(self.x_max - self.x_min)}x{self.res[1]}, ")
                if self.args.proportional:
                    fd.write("char_width")
                else:
                    fd.write("NULL")
                fd.write("};\n")

    def generate_string_c(self):
        """Generate C code for a specific string bitmap."""
        if self.args.string:
            with open("string.c", "w", encoding='utf-8') as fd:
                mask = self.font.getmask(self.args.string, '1')
                out = Image.new('1', mask.size)
                out.im.paste(1, (0, 0) + mask.size, mask)
                fnt_name = self.font.getname()
                fd.write(f"/** Generated string \"{self.args.string}\" with "
                         f"{fnt_name[0]} {fnt_name[1]} {self.args.size} "
                         "file by generate.py*/\n\n")
                fd.write(f"/** \"{self.args.string}\" bitmap {mask.size[0]}x{mask.size[1]} */\n")
                fd.write(f"const unsigned char string_{mask.size[0]}x{mask.size[1]} [] = {{\n")
                for byte in out.tobytes():
                    fd.write(f"0x{byte:02X}, ")
                fd.write("};\n")
                out.save('string.png')

    def generate_atlas(self):
        """Generate a font atlas image."""
        if self.args.atlas:
            atlas_res = ((self.x_max - self.x_min) * 16 + 17, (self.res[1] + 1) *
                         ceil(len(self.charset) / 16) + 1)
            atlas = Image.new("RGB", atlas_res, 0)
            d = ImageDraw.Draw(atlas)
            for index in range(len(self.pixels)):
                x = int(index % 16)
                y = int(index / 16)
                char_img = Image.frombytes("1", self.res, self.pixels[index])
                atlas.paste(char_img, (((self.x_max - self.x_min) + 1) * x + 1, (self.res[1] + 1) * y + 1))

            """ Draw line separator """
            for x in range(0, atlas_res[0], (self.x_max - self.x_min) + 1):
                d.line([(x, 0), (x, atlas_res[1])], fill=128, width=1)
            for y in range(0, atlas_res[1], self.res[1] + 1):
                d.line([(0, y), (atlas_res[0], y)], fill=128, width=1)
            if self.args.proportional:
                for index, width in enumerate(self.widths):
                    x = ((self.x_max - self.x_min) + 1) * int(index % 16) + width
                    y = (self.res[1] + 1) * int(index / 16)
                    d.line([(x, y), (x, y + self.res[1])], fill=0xFFFF, width=1)
            atlas.save(self.args.atlas)

    def process(self):
        """Execute the full font generation process."""
        self.load_font()
        self.calculate_bounding_box()
        self.convert_to_bytes()
        self.generate_font_c()
        self.generate_string_c()
        self.generate_atlas()

if __name__ == "__main__":
    generator = FontGenerator()
    generator.process()