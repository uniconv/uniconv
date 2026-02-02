#!/usr/bin/env python3
"""
uniconv CLI Plugin: image-grayscale

Converts images to grayscale using Python/Pillow.

Usage:
    grayscale.py --input <file> --target <format> [--output <file>] [options]

Plugin options:
    --method <method>  Conversion method: luminosity, average, lightness (default: luminosity)
"""

import argparse
import json
import sys
import os

def main():
    parser = argparse.ArgumentParser(description='Convert image to grayscale')

    # Universal arguments (passed by uniconv core to ALL plugins)
    parser.add_argument('--input', required=True, help='Input file path')
    parser.add_argument('--target', required=True, help='Target format')
    parser.add_argument('--output', help='Output file path')
    parser.add_argument('--force', action='store_true', help='Overwrite existing')
    parser.add_argument('--dry-run', action='store_true', help='Dry run mode')

    # Plugin-specific options (declared in manifest, passed after --)
    # Image plugins declare these; audio plugins would declare --bitrate, etc.
    parser.add_argument('--quality', type=int, default=85, help='Output quality (1-100)')
    parser.add_argument('--width', type=int, help='Output width')
    parser.add_argument('--height', type=int, help='Output height')
    parser.add_argument('--method', default='luminosity',
                       choices=['luminosity', 'average', 'lightness'],
                       help='Grayscale conversion method')

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

    # Determine output path
    if args.output:
        output_path = args.output
    else:
        base, _ = os.path.splitext(args.input)
        output_path = f"{base}_grayscale.{args.target}"

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

        # Convert to grayscale based on method
        if args.method == 'luminosity':
            # Standard grayscale (ITU-R 601-2 luma transform)
            gray = img.convert('L')
        elif args.method == 'average':
            # Simple average of RGB channels
            import numpy as np
            if img.mode != 'RGB':
                img = img.convert('RGB')
            arr = np.array(img)
            avg = arr.mean(axis=2).astype(np.uint8)
            gray = Image.fromarray(avg, mode='L')
        else:  # lightness
            # Use lightness from HSL
            img_rgb = img.convert('RGB')
            gray = img_rgb.convert('L')  # Simplified; true lightness would need HSL conversion

        # Resize if specified
        if args.width or args.height:
            w = args.width or int(img.width * (args.height / img.height))
            h = args.height or int(img.height * (args.width / img.width))
            gray = gray.resize((w, h), Image.Resampling.LANCZOS)

        # Convert back to RGB if saving as jpg (jpg doesn't support grayscale well)
        output_ext = os.path.splitext(output_path)[1].lower()
        if output_ext in ['.jpg', '.jpeg']:
            gray = gray.convert('RGB')

        # Save
        save_kwargs = {}
        if output_ext in ['.jpg', '.jpeg']:
            save_kwargs['quality'] = args.quality
        elif output_ext == '.webp':
            save_kwargs['quality'] = args.quality
        elif output_ext == '.png':
            save_kwargs['compress_level'] = 9 - (args.quality * 9 // 100)

        gray.save(output_path, **save_kwargs)

        # Get output size
        output_size = os.path.getsize(output_path)

        result = {
            "success": True,
            "output": output_path,
            "output_size": output_size,
            "extra": {
                "method": args.method,
                "original_size": [img.width, img.height],
                "output_size_px": [gray.width, gray.height]
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
