# Plugins

uniconv's functionality comes from plugins. There are two types:

| Type | Language | Interface | Best for |
|------|----------|-----------|----------|
| **Native** | C/C++ | Shared library (.so/.dylib/.dll) | Performance-critical operations |
| **CLI** | Any | Subprocess + JSON protocol | Rapid development, scripting languages |

## Managing plugins

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

# Show plugin details
uniconv plugin info ascii
```

## Available plugins

| Plugin | Type | Targets | Description |
|--------|------|---------|-------------|
| `image-convert` | Native (C++) | jpg, png, webp, gif, heic, ... | Image format conversion via libvips |
| `video-convert` | Native (C++) | mp4, mov, avi, webm, gif, ... | Video format conversion via FFmpeg |
| `doc-convert` | CLI (Python) | pdf, docx, odt, xlsx, md, ... | Document conversion via LibreOffice |
| `ascii` | CLI (Python) | ascii, ascii-art, text-art | Convert images to ASCII art |
| `image-filter` | CLI (Python) | grayscale, invert, negative | Apply image filters |

## Plugin specifier syntax

In pipelines, use `[scope/plugin:]target[.extension]` to specify plugins explicitly:

```bash
# Automatic plugin resolution
uniconv photo.heic "jpg"

# Explicit plugin
uniconv photo.heic "uniconv/image-convert:jpg --quality 90"

# Explicit output extension (when target != extension)
uniconv data.postgis "geo/postgis:extract.geojson"
```

## Plugin manifest

Each plugin has a `plugin.json` manifest. Key fields:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `targets` | `map` or `array` | — | Supported targets. Array shortcut: `["jpg", "png"]`. Map: `{"extract": ["geojson", "csv"]}` |
| `accepts` | `string[]` (optional) | omit = accept all | Input formats accepted. Omitted = any, `[]` = none |
| `sink` | `bool` | `false` | Terminal plugin that owns output (upload, save) |

## Windows notes

All plugins support Windows. Key points:

- **Native plugins** (image-convert, video-convert, audio-convert) bundle dependency DLLs alongside the plugin `.dll`. These are loaded automatically via `LoadLibraryEx`.
- **doc-convert** bundles Pandoc. LibreOffice must be installed on the system (`C:\Program Files\LibreOffice\...`).
- **CLI plugins** (Python-based) require Python 3.8+ in PATH. Plugin installation creates a virtual environment automatically.

## Writing plugins

See the [Contributing Guide](../CONTRIBUTING.md#writing-plugins) for plugin authoring instructions.
