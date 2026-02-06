---
name: uniconv
version: 0.4.6
description: Universal file converter and content intelligence CLI with ETL-based pipeline syntax
repository: https://github.com/uniconv/uniconv
license: MIT
install: curl -fsSL https://raw.githubusercontent.com/uniconv/uniconv/main/install.sh | bash
binary: uniconv
platform:
  - macos
  - linux
  - windows
capabilities:
  - File format conversion (image, video, audio, document)
  - Content extraction (audio from video, text from documents)
  - Multi-stage pipeline chaining
  - Parallel branching (tee) for multi-format output
  - System clipboard integration
  - Directory watching for auto-conversion
  - Plugin ecosystem with registry
  - Reusable presets
plugin_types:
  - native (C/C++ shared libraries)
  - cli (any language, JSON I/O protocol)
---

# uniconv — Universal File Converter

> A CLI tool for file conversion, content extraction, and upload using pipeline syntax.
> This document helps LLM agents understand and generate valid `uniconv` commands.

## Quick Reference

```bash
uniconv <source> "<pipeline>"          # Convert a file
uniconv -o <output> <source> "<pipe>"  # Convert with explicit output path
uniconv info <file>                    # Inspect a file
uniconv plugin install <name>          # Install a plugin
```

## Core Concept

uniconv uses **ETL (Extract, Transform, Load)** pipelines expressed as a quoted string:

```bash
uniconv photo.heic "jpg --quality 90"
#       ^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^
#       source       pipeline (MUST be quoted)
```

The pipeline describes **what to produce**, not how. uniconv automatically selects the right plugin.

**ETL categories** (auto-determined from target):
- **Transform**: Format conversion — `jpg`, `png`, `pdf`, `mp4`, `docx`
- **Extract**: Content extraction — `audio`, `text`, `faces`, `metadata`
- **Load**: Upload/save — `gdrive`, `s3`, `notion`, `clipboard`

## Pipeline Syntax

### Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `\|` | Sequential stage (pipe output → next) | `"pdf \| docx"` |
| `,` | Parallel elements within a stage | `"jpg, png, webp"` |
| `@` | Explicit plugin selection | `"mp4@ffmpeg"` |
| `--key value` | Plugin-specific option | `"jpg --quality 90"` |

### Single Conversion

```bash
uniconv photo.heic "jpg"
uniconv photo.heic "jpg --quality 90"
uniconv photo.heic "png --width 800 --height 600"
```

### Multi-Stage Pipeline

Stages connected with `|`. Output of one becomes input of the next.

```bash
uniconv photo.jpg "pdf | docx"           # jpg → pdf → docx
uniconv video.mp4 "audio | mp3"          # extract audio → convert to mp3
```

### Branching with `tee`

Split one input into multiple parallel outputs.

```bash
uniconv photo.heic "tee | jpg, png, webp"                    # 1 → 3 formats
uniconv photo.heic "tee | jpg --quality 90, jpg --quality 50" # 1 → 2 qualities
uniconv photo.heic "jpg | tee | grayscale, invert"            # convert then branch
```

**Rules:**
- `tee` CANNOT be the first stage (needs input)
- `tee` CANNOT be the last stage (needs consumers after it)
- The stage after `tee` MUST have multiple elements (`,` separated)

### Clipboard Integration

Copy results to the system clipboard.

```bash
uniconv photo.heic "jpg | clipboard"              # Copy to clipboard, NO file saved
uniconv photo.heic "jpg | clipboard --save"        # Copy to clipboard AND save file
uniconv -o out.jpg photo.heic "jpg | clipboard"    # Copy to clipboard AND save to out.jpg
uniconv notes.md "clipboard"                       # Copy text content directly
```

**Clipboard behavior by format type:**

| Format Type | What gets copied | File saved? |
|-------------|-----------------|-------------|
| Image (jpg, png, gif, bmp, tiff, webp) | Image content | No — use `--save` or `-o` |
| Text (txt, md, json, xml, csv, html, yaml, yml) | Text content | No — use `--save` or `-o` |
| Other (mp4, avi, binary, etc.) | File path | **Error** without `-o` or `--save` |

### Passthrough (`_`)

A no-op stage that passes input unchanged. Aliases: `_`, `echo`, `bypass`, `pass`, `noop`.

```bash
uniconv photo.heic "tee | png, _"     # Branch: convert to PNG + keep original
```

## Global Options

Global options go **before** the source file.

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file or directory path |
| `-f, --force` | Overwrite existing output files |
| `-r, --recursive` | Process directories recursively |
| `-p, --preset <name>` | Use a saved preset |
| `--json` | Output results as JSON |
| `--verbose` | Show detailed logs |
| `--quiet` | Suppress all output |
| `--dry-run` | Preview without executing |

### Output Path Resolution (priority order)

1. `--output` inside pipeline element: `"jpg --output /tmp/out.jpg"`
2. `-o` flag: `-o ./dir/name` → auto-appends target extension if missing
3. Default: `<source_stem>.<target_ext>` in current directory

## Subcommands

### `info` — Inspect a file

```bash
uniconv info photo.jpg
# Returns: format, MIME type, category, size, dimensions, duration
```

### `plugin` — Manage plugins

```bash
uniconv plugin list                    # List installed plugins
uniconv plugin list --registry         # List all available from registry
uniconv plugin install <name>          # Install from registry
uniconv plugin install <name>@1.2.0   # Install specific version
uniconv plugin install +essentials     # Install a plugin collection
uniconv plugin install /local/path     # Install from local directory
uniconv plugin remove <name>           # Uninstall
uniconv plugin info <name>             # Show plugin details + options
uniconv plugin search <query>          # Search registry
uniconv plugin update                  # Update all plugins
uniconv plugin update <name>           # Update specific plugin
```

### `preset` — Reusable pipelines

```bash
uniconv preset create web-ready "tee | jpg --quality 80, webp --quality 75, png"
uniconv preset list
uniconv preset show web-ready
uniconv preset delete web-ready
uniconv preset export web-ready ./preset.json
uniconv preset import ./preset.json

# Use a preset:
uniconv -p web-ready photo.heic
```

### `config` — Settings

```bash
uniconv config list                          # Show all settings
uniconv config get transform.jpg             # Get default plugin for jpg
uniconv config set transform.jpg vips        # Set vips as default for jpg
```

### `watch` — Auto-convert on file changes

```bash
uniconv watch ./inbox "jpg --quality 80"              # Watch directory
uniconv watch -o ./output ./inbox "jpg --quality 80"  # Watch with output dir
uniconv watch -r ./inbox "png"                        # Watch recursively
```

### `update` — Self-update

```bash
uniconv update            # Update to latest version
uniconv update --check    # Check only, don't install
```

## Decision Guide for LLM Agents

**Use this table to pick the right command based on user intent:**

| User wants to... | Command pattern |
|-------------------|----------------|
| Convert a file | `uniconv <file> "<target>"` |
| Convert with quality/size options | `uniconv <file> "<target> --quality 90"` |
| Convert to multiple formats at once | `uniconv <file> "tee \| <f1>, <f2>, <f3>"` |
| Chain conversions (A→B→C) | `uniconv <file> "<B> \| <C>"` |
| Copy result to clipboard | `uniconv <file> "<target> \| clipboard"` |
| Save file AND copy to clipboard | `uniconv <file> "<target> \| clipboard --save"` |
| Convert all files in a directory | `uniconv -r <dir> "<target>"` |
| Auto-convert new files in a folder | `uniconv watch <dir> "<target>"` |
| Check what a file is | `uniconv info <file>` |
| Save a pipeline for reuse | `uniconv preset create <name> "<pipeline>"` |
| Control output location | `uniconv -o <path> <file> "<target>"` |
| Force overwrite existing files | `uniconv -f <file> "<target>"` |
| Preview without executing | `uniconv --dry-run <file> "<target>"` |
| Get machine-readable output | `uniconv --json <file> "<target>"` |
| Use a specific plugin | `uniconv <file> "<target>@<plugin>"` |
| Set default plugin for a format | `uniconv config set transform.<fmt> <plugin>` |
| Find available plugins | `uniconv plugin search <keyword>` |

## Common Patterns

### Image Processing
```bash
uniconv photo.heic "jpg"                                       # Format conversion
uniconv photo.png "webp --quality 80"                          # With quality
uniconv photo.jpg "png --width 800 --height 600"               # Resize
uniconv -r ./photos "jpg --quality 85"                         # Batch convert
uniconv photo.heic "tee | jpg --quality 90, png, webp"         # Multi-format
```

### Document Conversion
```bash
uniconv report.pdf "docx"
uniconv notes.md "pdf"
uniconv source.md "html | pdf"                                 # Chain: md → html → pdf
```

### Media Conversion
```bash
uniconv video.mov "mp4@ffmpeg --crf 23"
uniconv video.mp4 "audio | mp3"                                # Extract audio
uniconv audio.wav "mp3"
```

### Clipboard Workflows
```bash
uniconv photo.heic "png | clipboard"                           # Quick copy (no file)
uniconv photo.heic "png | clipboard --save"                    # Copy + save file
uniconv data.json "clipboard"                                  # Copy text content
```

## Important Rules for LLMs

1. **Pipeline MUST be quoted** — Always use double quotes: `"jpg --quality 90"`
2. **Option placement matters** — Global options (`-o`, `-f`) BEFORE source file; plugin options (`--quality`) INSIDE pipeline after the target
3. **`tee` needs consumers** — NEVER end a pipeline with `tee`; always follow with `| fmt1, fmt2`
4. **Clipboard = no file by default** — Use `--save` or `-o` to also create a file
5. **Non-copyable clipboard needs `-o`** — Video/binary formats ERROR without `-o` or `--save`
6. **Plugin auto-selection** — Usually omit `@plugin`; uniconv picks automatically
7. **Extension auto-append** — `-o ./dir/name` (no ext) gets target extension added
8. **Temp files are automatic** — Multi-stage intermediate files handled internally with `--input-format` hints
