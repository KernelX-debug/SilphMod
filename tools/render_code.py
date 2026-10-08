import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True, help="A local monospace TrueType font")
    args = parser.parse_args()
    source = ROOT / "engine/src/session-transfers.cjs"
    lines = source.read_text(encoding="utf-8").splitlines()
    begin = next(i for i, line in enumerate(lines) if line.strip() == "authorize(id, encounterId) {")
    excerpt = lines[begin:begin + 5]
    font = ImageFont.truetype(str(args.font), 22)
    small = ImageFont.truetype(str(args.font), 18)
    width = max(1080, int(max(font.getlength(line) for line in excerpt)) + 140)
    image = Image.new("RGB", (width, 364), "#10151e")
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((24, 24, width - 24, 340), radius=14, fill="#1d2b38")
    draw.text((48, 44), "engine/src/session-transfers.cjs", font=small, fill="#61e9b6")
    draw.text((48, 76), "Transfer authorized for a current-session catch only", font=small, fill="#b5c3d1")
    for index, line in enumerate(excerpt):
        y = 124 + index * 31
        draw.text((47, y), str(begin + index + 1), font=small, fill="#8b9aaa")
        draw.text((100, y), line, font=font, fill="#f2f7fb")
    target = ROOT / "docs/assets/session-transfer.png"
    target.parent.mkdir(parents=True, exist_ok=True)
    image.save(target)
    print("Rendered exact source excerpt, lines", begin + 1, "to", begin + len(excerpt))


if __name__ == "__main__":
    main()
