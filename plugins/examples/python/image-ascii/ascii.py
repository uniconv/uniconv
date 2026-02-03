#!/usr/bin/env python3
"""
uniconv CLI Plugin: image-ascii

Converts images to ASCII art using Python/Pillow.

Usage:
    ascii.py --input <file> --target <format> [--output <file>] [options]

Plugin options:
    --width <chars>     Output width in characters (default: 80)
    --charset <name>    Character set: standard, simple, blocks, detailed
    --invert            Invert brightness (for dark backgrounds)
    --color             Output ANSI colored ASCII art
    --html              Output HTML with colored spans
"""

import argparse
import json
import sys
import os

# Character sets for different detail levels
CHARSETS = {
    # Standard ASCII art characters (10 levels)
    'standard': ' .:-=+*#%@',
    # Simple set (5 levels)
    'simple': ' .:*#',
    # Block characters for more solid look
    'blocks': ' ░▒▓█',
    # Detailed set (more gradation, 16 levels)
    'detailed': ' .\'`^",:;Il!i><~+_-?][}{1)(|/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$',
}


def rgb_to_ansi(r, g, b):
    """Convert RGB to ANSI 256-color code."""
    # Use the 6x6x6 color cube (codes 16-231)
    r_idx = int(r / 255 * 5)
    g_idx = int(g / 255 * 5)
    b_idx = int(b / 255 * 5)
    return 16 + 36 * r_idx + 6 * g_idx + b_idx


def rgb_to_html_color(r, g, b):
    """Convert RGB to HTML hex color."""
    return f'#{r:02x}{g:02x}{b:02x}'


def image_to_ascii(img, width, charset, invert=False, color=False, html=False):
    """
    Convert a PIL Image to ASCII art.

    Args:
        img: PIL Image object
        width: Output width in characters
        charset: Character set to use
        invert: Invert brightness mapping
        color: Use ANSI colors
        html: Output HTML format

    Returns:
        ASCII art string
    """
    # Get character set
    chars = CHARSETS.get(charset, CHARSETS['standard'])
    if invert:
        chars = chars[::-1]

    # Calculate dimensions
    # ASCII characters are typically ~2x taller than wide
    aspect_ratio = img.height / img.width
    height = int(width * aspect_ratio * 0.5)

    # Resize image
    img_resized = img.resize((width, height))

    # Convert to RGB for color processing
    if img_resized.mode != 'RGB':
        img_resized = img_resized.convert('RGB')

    # Also get grayscale for character selection
    img_gray = img_resized.convert('L')

    pixels = list(img_resized.getdata())
    gray_pixels = list(img_gray.getdata())

    lines = []

    if html:
        lines.append('<!DOCTYPE html>')
        lines.append('<html><head>')
        lines.append('<style>')
        lines.append('body { background: #000; }')
        lines.append('pre { font-family: monospace; font-size: 10px; line-height: 1; }')
        lines.append('</style>')
        lines.append('</head><body><pre>')

    for y in range(height):
        line_chars = []
        for x in range(width):
            idx = y * width + x
            gray_val = gray_pixels[idx]
            r, g, b = pixels[idx]

            # Map gray value to character
            char_idx = int(gray_val / 256 * len(chars))
            char_idx = min(char_idx, len(chars) - 1)
            char = chars[char_idx]

            if html:
                color_hex = rgb_to_html_color(r, g, b)
                line_chars.append(f'<span style="color:{color_hex}">{char}</span>')
            elif color:
                ansi_code = rgb_to_ansi(r, g, b)
                line_chars.append(f'\033[38;5;{ansi_code}m{char}\033[0m')
            else:
                line_chars.append(char)

        lines.append(''.join(line_chars))

    if html:
        lines.append('</pre></body></html>')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Convert image to ASCII art')

    # Universal arguments (passed by uniconv core to ALL plugins)
    parser.add_argument('--input', required=True, help='Input file path')
    parser.add_argument('--target', required=True, help='Target format')
    parser.add_argument('--output', help='Output file path')
    parser.add_argument('--force', action='store_true', help='Overwrite existing')
    parser.add_argument('--dry-run', action='store_true', help='Dry run mode')

    # Plugin-specific options
    parser.add_argument('--width', type=int, default=80,
                       help='Output width in characters')
    parser.add_argument('--charset', default='standard',
                       choices=['standard', 'simple', 'blocks', 'detailed'],
                       help='Character set for ASCII conversion')
    parser.add_argument('--invert', action='store_true',
                       help='Invert brightness (for dark backgrounds)')
    parser.add_argument('--color', action='store_true',
                       help='Output ANSI colored ASCII art')
    parser.add_argument('--html', action='store_true',
                       help='Output HTML with colored spans')

    args, _ = parser.parse_known_args()

    # Check if Pillow is available
    try:
        from PIL import Image
    except ImportError:
        result = {
            "success": False,
            "error": "Pillow not installed. Run: pip install Pillow"
        }
        print(json.dumps(result))
        return 1

    # Validate input
    if not os.path.exists(args.input):
        result = {
            "success": False,
            "error": f"Input file not found: {args.input}"
        }
        print(json.dumps(result))
        return 1

    # Determine output extension based on options
    if args.html:
        output_ext = '.html'
    else:
        output_ext = '.txt'

    # Use target name as suffix
    target_suffix = f"_{args.target}"

    # Determine output path
    if args.output:
        base, ext = os.path.splitext(args.output)
        if ext in ['.txt', '.html']:
            output_path = args.output
        else:
            output_path = f"{base}{target_suffix}{output_ext}"
    else:
        base, _ = os.path.splitext(args.input)
        output_path = f"{base}{target_suffix}{output_ext}"

    # Check if output exists
    if os.path.exists(output_path) and not args.force:
        result = {
            "success": False,
            "error": f"Output file exists (use --force to overwrite): {output_path}"
        }
        print(json.dumps(result))
        return 1

    # Dry run
    if args.dry_run:
        result = {
            "success": True,
            "output": output_path,
            "extra": {"dry_run": True}
        }
        print(json.dumps(result))
        return 0

    try:
        # Open image
        img = Image.open(args.input)

        # Generate ASCII art
        ascii_art = image_to_ascii(
            img,
            width=args.width,
            charset=args.charset,
            invert=args.invert,
            color=args.color,
            html=args.html
        )

        # Write output
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(ascii_art)

        # Get output size
        output_size = os.path.getsize(output_path)

        # Calculate ASCII dimensions
        aspect_ratio = img.height / img.width
        ascii_height = int(args.width * aspect_ratio * 0.5)

        result = {
            "success": True,
            "output": output_path,
            "output_size": output_size,
            "extra": {
                "charset": args.charset,
                "ascii_width": args.width,
                "ascii_height": ascii_height,
                "color_mode": "html" if args.html else ("ansi" if args.color else "plain"),
                "inverted": args.invert,
                "original_size": [img.width, img.height]
            }
        }
        print(json.dumps(result))
        return 0

    except Exception as e:
        result = {
            "success": False,
            "error": str(e)
        }
        print(json.dumps(result))
        return 1


if __name__ == '__main__':
    sys.exit(main())
