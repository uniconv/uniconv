<p align="center">
  <img src="uniconv-logo.png" alt="uniconv logo" width="160" />
</p>

<h1 align="center">uniconv</h1>

<p align="center">
  <strong>A universal file converter with a plugin-based pipeline architecture.</strong>
</p>

<p align="center">
  <a href="#installation">Installation</a> &middot;
  <a href="#quick-start">Quick Start</a> &middot;
  <a href="#pipeline-syntax">Pipeline Syntax</a> &middot;
  <a href="#builtin-stages">Builtin Stages</a> &middot;
  <a href="#plugins">Plugins</a> &middot;
  <a href="#contributing">Contributing</a>
</p>

---

uniconv converts files between formats using an intuitive pipeline syntax. Chain multiple operations, use community plugins, and build presets for repeated workflows — all from a single CLI.

```bash
# Convert image to ASCII art
uniconv photo.jpg "ascii"

# Resize video and convert to GIF
uniconv clip.mp4 "gif --width 480 --fps 15"

# Fan out: save PNG locally, copy to clipboard
uniconv photo.heic "tee | png, clipboard"
```

## Why uniconv?

- **Pipeline syntax** — Chain conversions with `|`, branch with `tee`, just like a Unix shell.
- **Plugin system** — Write plugins in any language (C++, Python, Go, Rust, ...). Native and CLI plugins are both first-class.
- **Registry & collections** — Install community plugins with a single command. `uniconv plugin install +essentials` gets you started.
- **Presets** — Save common pipelines and reuse them. `uniconv --preset web-ready photo.heic`
- **JSON output** — Machine-readable results for scripting and automation. `uniconv --json "video.mov | mp4"`
- **Dry run** — Preview what will happen before executing. `uniconv --dry-run "photo.heic | jpg"`

## Installation

### Quick install

```bash
curl -fsSL https://raw.githubusercontent.com/uniconv/uniconv/main/install.sh | bash
```

### Build from source

**Requirements:** CMake 3.20+, a C++20 compiler (Clang 14+, GCC 12+, MSVC 17+)

```bash
git clone https://github.com/anthropics/uniconv.git
cd uniconv/uniconv
mkdir build && cd build
cmake ..
cmake --build .
```

The binary is at `build/bin/uniconv`. Add it to your `PATH` or install system-wide:

```bash
sudo cmake --install .
```

### Update

To update uniconv to the latest version, re-run the install script:

```bash
curl -fsSL https://raw.githubusercontent.com/uniconv/uniconv/main/install.sh | bash
```

## Quick Start

```bash
# Convert a HEIC photo to JPEG
uniconv photo.heic "jpg --quality 85"

# Convert image to ASCII art
uniconv photo.jpg "ascii --width 80"

# Create a GIF from video (resized, 15fps)
uniconv video.mp4 "gif --width 320 --fps 15"

# Resize video to 720p
uniconv video.mp4 "mp4 --height 720"

# Apply grayscale filter
uniconv photo.jpg "grayscale"

# Get file info
uniconv info photo.heic

# Install the essentials plugin collection
uniconv plugin install +essentials

# Update uniconv to the latest version
uniconv update
```

## Pipeline Syntax

Pipelines are the core of uniconv. They describe a series of transformations applied to an input file.

```
uniconv <source> "<target> [options] | <target> [options] | ..."
```

### Basics

```bash
# Single conversion
uniconv photo.heic "jpg"

# Chained conversions (resize then apply filter)
uniconv photo.jpg "png --width 800 | grayscale"

# Specify output path
uniconv -o thumbnail.gif video.mp4 "gif --width 320 --fps 10"
```

### Explicit plugin selection

When multiple plugins can handle a target, pin a specific one with `@`:

```bash
# Use the ffmpeg plugin for mp4 conversion
uniconv video.mov "mp4@ffmpeg --height 720"
```

### Presets

Save and reuse pipelines:

```bash
# Save a preset
uniconv preset create web-ready "tee | jpg --quality 80, webp --quality 75, png"

# Use it
uniconv --preset web-ready photo.heic

# List presets
uniconv preset list
```

## Builtin Stages

uniconv includes built-in stages for pipeline control and system integration.

### tee — Branching

Split a pipeline into multiple parallel branches:

```bash
# Convert to multiple formats at once
uniconv photo.heic "tee | jpg, png, webp"

# Different quality settings for each branch
uniconv photo.jpg "tee | jpg --quality 90, jpg --quality 50 --width 200"
```

### clipboard — Copy to System Clipboard

Copy output directly to system clipboard:

```bash
# Convert and copy image to clipboard (no file saved)
uniconv photo.heic "png | clipboard"

# Copy ASCII art to clipboard
uniconv photo.jpg "ascii | clipboard"

# Save file AND copy to clipboard
uniconv photo.heic "png | clipboard --save"

# With explicit output path
uniconv -o result.png photo.heic "png | clipboard"
```

**Behavior:**
- Image/text formats: content is copied directly to clipboard
- Other formats: file path is copied to clipboard
- By default, no file is created when clipboard is terminal (use `--save` or `-o` to save)

### _ (passthrough) — No-op Stage

Pass input through unchanged. Useful for matching `tee` branches when you only want to process some of them:

```bash
# Convert to PNG and copy to clipboard, pass through original to save
uniconv photo.heic "tee | png, ascii | _, clipboard"
# -> PNG saved to file, ASCII copied to clipboard

# Skip one branch entirely
uniconv video.mp4 "tee | gif --width 320, _ | clipboard, _"
```

**Aliases:** `_`, `echo`, `bypass`, `pass`, `noop`

## Plugins

uniconv's functionality comes from plugins. There are two types:

| Type | Language | Interface | Best for |
|------|----------|-----------|----------|
| **Native** | C/C++ | Shared library (.so/.dylib/.dll) | Performance-critical operations |
| **CLI** | Any | Subprocess + JSON protocol | Rapid development, scripting languages |

### Managing plugins

```bash
# List installed plugins
uniconv plugin list

# Search the registry
uniconv plugin search image

# Install a plugin
uniconv plugin install ascii

# Install a specific version
uniconv plugin install ascii@1.0.0

# Install an entire collection
uniconv plugin install +essentials

# Update plugins
uniconv plugin update --all

# Remove a plugin
uniconv plugin remove ascii
```

### Available plugins

| Plugin | Type | Description |
|--------|------|-------------|
| `ascii` | CLI (Python) | Convert images to ASCII art |
| `image-filter` | CLI (Python) | Apply filters (grayscale, invert) |
| `image-convert` | Native (C++) | Image format conversion via libvips |
| `video-convert` | Native (C++) | Video format conversion via FFmpeg |

Want to write your own plugin? See the [Contributing Guide](CONTRIBUTING.md#writing-plugins).

## CLI Reference

```
uniconv [options] <source> "<pipeline>"
uniconv <command> [args]
```

### Global options

| Flag | Description |
|------|-------------|
| `-o, --output <path>` | Output file path |
| `-f, --force` | Overwrite existing files |
| `-r, --recursive` | Process directories recursively |
| `--json` | Output results as JSON |
| `--dry-run` | Show what would happen without executing |
| `--quiet` | Suppress output |
| `--verbose` | Detailed output |
| `--preset <name>` | Use a saved preset |
| `--version` | Show version |
| `-h, --help` | Show help |

### Commands

| Command | Description |
|---------|-------------|
| `info <file>` | Show file details (format, size, dimensions) |
| `formats` | List supported formats |
| `watch <dir> "<pipeline>"` | Watch directory and process new files |
| `update` | Update uniconv to the latest version |
| `plugin list` | List installed plugins |
| `plugin install <name>` | Install a plugin or collection |
| `plugin remove <name>` | Uninstall a plugin |
| `plugin update [name]` | Update plugins |
| `plugin search <query>` | Search the registry |
| `plugin info <name>` | Show plugin details |
| `preset list` | List saved presets |
| `preset create <name> "<pipeline>"` | Save a preset |
| `preset delete <name>` | Remove a preset |
| `preset show <name>` | Show preset details |
| `config list` | List configuration |
| `config get <key>` | Get a config value |
| `config set <key> <value>` | Set a config value |

## Contributing

Contributions are welcome — whether it's bug reports, new plugins, core improvements, or documentation fixes. See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup, code conventions, architecture overview, and plugin authoring guides.

## License

MIT
