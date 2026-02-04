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
  <a href="#plugins">Plugins</a> &middot;
  <a href="#contributing">Contributing</a>
</p>

---

uniconv converts files between formats using an intuitive pipeline syntax. Chain multiple operations, use community plugins, and build presets for repeated workflows — all from a single CLI.

```bash
# Simple conversion
uniconv "photo.heic | jpg"

# Pipeline with options
uniconv "photo.heic | jpg --quality 90 | grayscale"

# Fan out to multiple formats
uniconv "photo.heic | tee | jpg, png, webp"
```

## Why uniconv?

- **Pipeline syntax** — Chain conversions with `|`, branch with `tee`, just like a Unix shell.
- **Plugin system** — Write plugins in any language (C++, Python, Go, Rust, ...). Native and CLI plugins are both first-class.
- **Registry & collections** — Install community plugins with a single command. `uniconv plugin install +essentials` gets you started.
- **Presets** — Save common pipelines and reuse them. `uniconv --preset web-ready photo.heic`
- **JSON output** — Machine-readable results for scripting and automation. `uniconv --json "video.mov | mp4"`
- **Dry run** — Preview what will happen before executing. `uniconv --dry-run "photo.heic | jpg"`

## Installation

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

## Quick Start

```bash
# Convert a HEIC photo to JPEG
uniconv "photo.heic | jpg"

# Convert with quality setting
uniconv "photo.heic | jpg --quality 85"

# Convert a video to GIF
uniconv "clip.mov | gif"

# Get file info
uniconv info photo.heic

# See available formats
uniconv formats

# Install the essentials plugin collection
uniconv plugin install +essentials
```

## Pipeline Syntax

Pipelines are the core of uniconv. They describe a series of transformations applied to an input file.

```
uniconv "<source> | <target> [options] | <target> [options] | ..."
```

### Basics

```bash
# Single conversion
uniconv "photo.heic | jpg"

# Chained conversions
uniconv "photo.heic | jpg --quality 90 | grayscale"

# Specify output path
uniconv -o output.jpg "photo.heic | jpg"
```

### Branching with tee

Use `tee` to fan out into multiple parallel targets:

```bash
# Convert to three formats at once
uniconv "photo.heic | tee | jpg, png, webp"
```

### Explicit plugin selection

When multiple plugins can handle a target, pin a specific one with `@`:

```bash
# Use the ffmpeg scope for mp4 conversion
uniconv "video.mov | mp4@ffmpeg"
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
uniconv [options] "<pipeline>"
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
| `--watch` | Watch directory for changes |
| `--version` | Show version |
| `-h, --help` | Show help |

### Commands

| Command | Description |
|---------|-------------|
| `info <file>` | Show file details (format, size, dimensions) |
| `formats` | List supported formats |
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
