# uniconv Example Plugins

This directory contains example plugins demonstrating how to create plugins for uniconv.

## Plugin Types

### CLI Plugins (Recommended for most use cases)

CLI plugins are external executables that uniconv invokes as subprocesses. They can be written in any language (Python, Go, Rust, Node.js, etc.).

**Pros:**

- Easy to develop in any language
- No compilation needed for interpreted languages
- Safe isolation (runs in separate process)

**Cons:**

- Slightly slower due to process overhead
- Requires runtime (Python, Node.js, etc.) to be installed

See: `python/image-grayscale/`, `python/image-ascii/`

### Native Plugins (For performance-critical use cases)

Native plugins are shared libraries (.so, .dylib, .dll) that uniconv loads directly. They must implement the C ABI defined in `uniconv/plugin_api.h`.

**Pros:**

- Maximum performance (no IPC overhead)
- Direct memory access

**Cons:**

- Must be compiled for each platform
- More complex to develop
- Bugs can crash the main process

See: `cpp/image-invert/`

---

## CLI Plugin: image-grayscale (Python)

Converts images to grayscale using Python/Pillow.

### Requirements

```bash
pip install Pillow
```

### Installation

```bash
# Copy to user plugins directory
cp -r python/image-grayscale ~/.uniconv/plugins/

# Make executable
chmod +x ~/.uniconv/plugins/image-grayscale/grayscale.py

# Verify installation
uniconv plugins
```

### Usage

```bash
# Basic usage
uniconv "photo.jpg | grayscale"

# With options
uniconv "photo.jpg | grayscale --method average"

# Specify output
uniconv -o photo_bw.jpg "photo.jpg | grayscale"
```

---

## CLI Plugin: image-ascii (Python)

Converts images to ASCII art using Python/Pillow.

### Requirements

```bash
pip install Pillow
```

### Installation

```bash
# Copy to user plugins directory
cp -r python/image-ascii ~/.uniconv/plugins/

# Make executable
chmod +x ~/.uniconv/plugins/image-ascii/ascii.py

# Verify installation
uniconv plugins
```

### Usage

```bash
# Basic usage (80 chars wide)
uniconv "photo.jpg | ascii"

# Wider output with detailed character set
uniconv "photo.jpg | ascii --width 120 --charset detailed"

# Inverted for dark terminal backgrounds
uniconv "photo.jpg | ascii --invert"

# ANSI colored output (for terminal)
uniconv "photo.jpg | ascii --color"

# HTML output (for browser viewing)
uniconv "photo.jpg | ascii --html --width 150"

# Block characters for solid look
uniconv "photo.jpg | ascii --charset blocks"
```

### Character Sets

- **standard**: ` .:-=+*#%@` (10 levels, balanced)
- **simple**: ` .:*#` (5 levels, minimal)
- **blocks**: ` ░▒▓█` (5 levels, solid Unicode blocks)
- **detailed**: 70 characters (fine gradation)

---

## Native Plugin: image-invert (C++)

Inverts image colors. This is a demonstration of the native plugin API.

### Building

```bash
cd cpp/image-invert

# Create build directory
mkdir build && cd build

# Configure (adjust UNICONV_INCLUDE_DIR if needed)
cmake .. -DUNICONV_INCLUDE_DIR=/path/to/uniconv/include

# Build
cmake --build .
```

### Installation

```bash
# Copy to user plugins directory
mkdir -p ~/.uniconv/plugins/image-invert
cp build/libimage_invert.so ~/.uniconv/plugins/image-invert/
cp plugin.json ~/.uniconv/plugins/image-invert/

# Verify installation
uniconv plugins
```

### Usage

```bash
uniconv "photo.jpg | invert"
```

---

## Creating Your Own Plugin

### CLI Plugin Structure

```
my-plugin/
├── plugin.json      # Manifest (required)
├── my-plugin.py     # Executable (or .sh, .js, etc.)
└── README.md        # Documentation (optional)
```

### Manifest (plugin.json)

```json
{
  "name": "my-plugin",
  "group": "my-plugin",
  "version": "1.0.0",
  "description": "Description of what the plugin does",
  "etl": "transform",
  "interface": "cli",
  "executable": "my-plugin.py",
  "targets": ["target1", "target2"],
  "input_formats": ["jpg", "png"],
  "options": [
    {
      "name": "--my-option",
      "type": "string",
      "default": "value",
      "description": "What this option does"
    }
  ]
}
```

### CLI Protocol

Your executable receives these arguments from uniconv core:

```
# Universal arguments (passed to ALL plugins)
--input <path>       Input file path (required)
--target <target>    Target format/type (required)
--output <path>      Output path (optional)
--force              Overwrite existing files
--dry-run            Don't actually process

# Plugin-specific options (passed after --)
-- [options]         All options declared in manifest
```

**Important:** Domain-specific options like `--quality`, `--width`, `--height` are NOT
universal - they only make sense for image plugins. An audio plugin would declare
`--bitrate`, `--sample-rate` instead.

Each plugin MUST declare ALL its options in the manifest's `options` array. Users pass
these options after `--` on the command line:

```bash
# Image plugin with quality and width
uniconv photo.jpg -t grayscale -- --quality 90 --width 800

# Audio plugin with bitrate
uniconv song.mp3 -t wav -- --bitrate 320
```

Your executable must output JSON to stdout:

```json
{
  "success": true,
  "output": "/path/to/output.jpg",
  "output_size": 12345,
  "extra": {
    "custom_field": "value"
  }
}
```

Or on error:

```json
{
  "success": false,
  "error": "Error message"
}
```

### Native Plugin API

See `include/uniconv/plugin_api.h` for the C ABI specification.

Required functions:

- `uniconv_plugin_info()` - Return plugin information
- `uniconv_plugin_execute()` - Process a file
- `uniconv_plugin_free_result()` - Free result memory
