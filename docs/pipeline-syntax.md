# Pipeline Syntax

Pipelines describe a series of transformations applied to an input file.

```
uniconv <source> "<target> [options] | <target> [options] | ..."
```

## Basics

```bash
# Single conversion
uniconv photo.heic "jpg"

# With options
uniconv photo.heic "jpg --quality 85"

# Chained conversions
uniconv photo.heic "jpg | grayscale"

# Multi-stage document conversion
uniconv photo.jpg "pdf | docx"

# Specify output path
uniconv -o thumbnail.gif video.mp4 "gif --width 320 --fps 10"
```

## Explicit plugin selection

When multiple plugins can handle a target, pin a specific one with `@`:

```bash
uniconv video.mov "mp4@ffmpeg --height 720"
```

## Stdin

Use `-` as the source to pipe data from stdin. The input format is auto-detected via libmagic:

```bash
echo "hello world" | uniconv - "translate | txt"
cat data.csv | uniconv - "json"
```

Use `--input-format` to override auto-detection when needed:

```bash
uniconv --input-format txt - "md"
```

## Generators

Use `-` without piped data to run a generator plugin that produces output from scratch:

```bash
uniconv - "random-noise --width 512 | png"
```

## Presets

Save and reuse pipelines:

```bash
# Save a preset
uniconv preset create web-ready "tee | jpg --quality 80, webp --quality 75, png"

# Use it
uniconv --preset web-ready photo.heic

# List presets
uniconv preset list
```
