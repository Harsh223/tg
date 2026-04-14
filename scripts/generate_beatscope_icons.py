#!/usr/bin/env python3
"""
Generate Beatscope branding icons (PNG, ICO, SVG).

Outputs:
- icons/<size>x<size>/apps/beatscope.png
- icons/<size>x<size>/mimetypes/application-x-beatscope-data.png
- icons/beatscope.ico
- icons/beatscope-document.ico
- icons/scalable/apps/beatscope.svg
- icons/scalable/mimetypes/application-x-beatscope-data.svg
"""

import argparse
from pathlib import Path
from typing import Iterable, Tuple

from PIL import Image, ImageDraw

DEFAULT_SIZES = (16, 22, 32, 48, 64, 128, 256)


def _rounded_rect(
    draw: ImageDraw.ImageDraw,
    box: Tuple[int, int, int, int],
    radius: int,
    fill,
    outline=None,
    width=1,
):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def draw_app_icon(size: int) -> Image.Image:
    """Create the Beatscope app icon: watch dial + beat waveform on dark rounded square."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    r = max(2, size // 6)
    _rounded_rect(d, (0, 0, size - 1, size - 1), r, fill=(22, 26, 38, 255))

    # Subtle top-to-bottom tint
    for y in range(size):
        alpha = int(90 * (1 - y / max(1, size - 1)))
        d.line((0, y, size, y), fill=(30, 120, 220, alpha), width=1)

    # Watch dial
    cx = size * 0.38
    cy = size * 0.50
    dial_r = size * 0.22
    ring_w = max(1, size // 18)
    d.ellipse(
        (cx - dial_r, cy - dial_r, cx + dial_r, cy + dial_r),
        outline=(220, 236, 255, 255),
        width=ring_w,
    )

    # Hands
    hand_w = max(1, size // 24)
    d.line((cx, cy, cx, cy - dial_r * 0.60), fill=(122, 245, 220, 255), width=hand_w)
    d.line((cx, cy, cx + dial_r * 0.45, cy), fill=(122, 245, 220, 255), width=hand_w)
    pivot = max(1, size // 40)
    d.ellipse(
        (cx - pivot, cy - pivot, cx + pivot, cy + pivot), fill=(122, 245, 220, 255)
    )

    # Beat waveform
    x0 = int(size * 0.58)
    ym = int(size * 0.50)
    amp = max(2, int(size * 0.11))
    pts = [
        (x0, ym),
        (int(size * 0.64), ym),
        (int(size * 0.67), ym - amp),
        (int(size * 0.70), ym + amp),
        (int(size * 0.73), ym - amp // 2),
        (int(size * 0.78), ym),
        (int(size * 0.86), ym),
    ]
    d.line(pts, fill=(122, 245, 220, 255), width=max(1, size // 20), joint="curve")

    # Border highlight
    _rounded_rect(
        d,
        (0, 0, size - 1, size - 1),
        r,
        fill=None,
        outline=(255, 255, 255, 55),
        width=1,
    )
    return img


def draw_mime_icon(size: int) -> Image.Image:
    """Create MIME icon: document sheet with folded corner + waveform."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    pad = max(1, size // 10)
    fold = max(2, size // 4)
    stroke = max(1, size // 40)

    _rounded_rect(
        d,
        (pad, pad, size - pad, size - pad),
        radius=max(1, size // 12),
        fill=(243, 248, 255, 255),
        outline=(120, 145, 178, 255),
        width=stroke,
    )

    d.polygon(
        [(size - pad - fold, pad), (size - pad, pad), (size - pad, pad + fold)],
        fill=(214, 228, 246, 255),
        outline=(120, 145, 178, 255),
    )

    # Small dial marker
    d.ellipse(
        (int(size * 0.24), int(size * 0.28), int(size * 0.38), int(size * 0.42)),
        outline=(33, 127, 214, 255),
        width=max(1, size // 28),
    )

    # Waveform
    y = int(size * 0.58)
    x1 = int(size * 0.24)
    waveform = [
        (x1, y),
        (int(size * 0.34), y),
        (int(size * 0.39), y - int(size * 0.12)),
        (int(size * 0.45), y + int(size * 0.12)),
        (int(size * 0.51), y - int(size * 0.06)),
        (int(size * 0.60), y),
        (int(size * 0.75), y),
    ]
    d.line(waveform, fill=(33, 127, 214, 255), width=max(1, size // 18), joint="curve")

    return img


def ensure_dirs(root: Path, sizes: Iterable[int]) -> None:
    for s in sizes:
        (root / f"{s}x{s}" / "apps").mkdir(parents=True, exist_ok=True)
        (root / f"{s}x{s}" / "mimetypes").mkdir(parents=True, exist_ok=True)
    (root / "scalable" / "apps").mkdir(parents=True, exist_ok=True)
    (root / "scalable" / "mimetypes").mkdir(parents=True, exist_ok=True)


def write_svg_assets(root: Path) -> None:
    app_svg = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#2b467a"/>
      <stop offset="1" stop-color="#151a27"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="42" fill="url(#bg)"/>
  <rect x="8" y="8" width="240" height="240" rx="42" fill="none" stroke="#ffffff" stroke-opacity="0.18" stroke-width="2"/>
  <circle cx="96" cy="128" r="56" fill="none" stroke="#e6f4ff" stroke-width="8"/>
  <line x1="96" y1="128" x2="96" y2="93" stroke="#7af5dc" stroke-width="7" stroke-linecap="round"/>
  <line x1="96" y1="128" x2="123" y2="128" stroke="#7af5dc" stroke-width="7" stroke-linecap="round"/>
  <circle cx="96" cy="128" r="4" fill="#7af5dc"/>
  <polyline points="147,128 164,128 172,103 180,153 188,116 198,128 220,128"
            fill="none" stroke="#7af5dc" stroke-width="8" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
"""
    mime_svg = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <rect x="28" y="16" width="200" height="224" rx="20" fill="#f3f8ff" stroke="#7a90b3" stroke-width="6"/>
  <polygon points="176,16 228,16 228,68" fill="#d8e5f7" stroke="#7a90b3" stroke-width="6"/>
  <circle cx="88" cy="96" r="18" fill="none" stroke="#217fd6" stroke-width="6"/>
  <polyline points="62,150 88,150 100,122 114,178 126,136 146,150 190,150"
            fill="none" stroke="#217fd6" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
"""
    (root / "scalable" / "apps" / "beatscope.svg").write_text(app_svg, encoding="utf-8")
    (root / "scalable" / "mimetypes" / "application-x-beatscope-data.svg").write_text(
        mime_svg, encoding="utf-8"
    )


def save_ico(root: Path) -> None:
    app_256 = root / "256x256" / "apps" / "beatscope.png"
    mime_256 = root / "256x256" / "mimetypes" / "application-x-beatscope-data.png"
    ico_sizes = [
        (16, 16),
        (24, 24),
        (32, 32),
        (48, 48),
        (64, 64),
        (128, 128),
        (256, 256),
    ]

    Image.open(app_256).save(root / "beatscope.ico", sizes=ico_sizes)
    Image.open(mime_256).save(root / "beatscope-document.ico", sizes=ico_sizes)


def parse_sizes(raw: str) -> Tuple[int, ...]:
    values = []
    for p in raw.split(","):
        p = p.strip()
        if not p:
            continue
        n = int(p)
        if n <= 0:
            raise ValueError(f"Invalid icon size: {n}")
        values.append(n)
    if not values:
        raise ValueError("No valid sizes provided")
    return tuple(sorted(set(values)))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Beatscope icons")
    parser.add_argument(
        "--root", default="icons", help="Icon root directory (default: icons)"
    )
    parser.add_argument(
        "--sizes",
        default="16,22,32,48,64,128,256",
        help="Comma-separated icon sizes (default: 16,22,32,48,64,128,256)",
    )
    args = parser.parse_args()

    root = Path(args.root)
    sizes = parse_sizes(args.sizes)

    ensure_dirs(root, sizes)

    for s in sizes:
        app = draw_app_icon(s)
        app.save(root / f"{s}x{s}" / "apps" / "beatscope.png")

        mime = draw_mime_icon(s)
        mime.save(root / f"{s}x{s}" / "mimetypes" / "application-x-beatscope-data.png")

    if 256 in sizes:
        save_ico(root)

    write_svg_assets(root)
    print(f"Generated Beatscope icon assets in: {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
