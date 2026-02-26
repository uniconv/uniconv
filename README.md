<p align="center">
  <img src="uniconv-logo.png" alt="uniconv logo" width="160" />
</p>

<h1 align="center">uniconv</h1>

<p align="center">
  <strong>A universal file converter with a plugin-based pipeline architecture.</strong>
</p>

---

```bash
# Convert formats
uniconv photo.heic "jpg --quality 85"

# Chain operations
uniconv video.mp4 "wav | aac"

# Fan out
uniconv shot.png "tee | jpg --width 640, webp --width 320, tiff --width 800"

# Pipe from stdin and copy result to clipboard
echo "https://example.com" | uniconv - "qr | clipboard"

# Generate content without an input file
uniconv - "random-noise --width 512 | png"
```

## Install

**macOS / Linux:**

```bash
curl -fsSL https://raw.githubusercontent.com/uniconv/uniconv/main/install.sh | bash
```

**Windows (PowerShell):**

```powershell
irm https://raw.githubusercontent.com/uniconv/uniconv/main/install.ps1 | iex
```

Or from cmd: `powershell -c "irm https://raw.githubusercontent.com/uniconv/uniconv/main/install.ps1 | iex"`

See [Building from Source](docs/building.md) for build-from-source instructions.

## Get started

```bash
# Install the essentials plugin collection
uniconv plugin install +essentials

# Convert
uniconv photo.heic "jpg --quality 85"
uniconv video.mp4 "gif --width 320 --fps 15"
uniconv document.docx "pdf"

# Get file info
uniconv info photo.heic

# Update
uniconv update
```

## How it works

Pipelines chain transformations with `|`, branch with `tee`, and integrate with the system via builtins like `clipboard`.

```
uniconv [options] <source> "<target> [options] | <target> [options] | ..."
```

Functionality comes from **plugins** — installable converters written in any language. Native (C/C++) plugins for performance, CLI plugins (Python, Go, Rust, ...) for everything else.

```bash
uniconv plugin install +essentials  # Install a collection
uniconv plugin install ascii        # Install a plugin
uniconv plugin search image         # Search the registry
uniconv plugin update               # Update plugins installed
```

## Documentation

|                                            |                                                  |
| ------------------------------------------ | ------------------------------------------------ |
| [Pipeline Syntax](docs/pipeline-syntax.md) | Chaining, branching, presets, stdin & generators |
| [Builtin Stages](docs/builtins.md)         | `tee`, `clipboard`, passthrough(`_`)             |
| [Plugins](docs/plugins.md)                 | Installing, managing, and writing plugins        |
| [CLI Reference](docs/cli-reference.md)     | All commands and options                         |
| [Building from Source](docs/building.md)   | Build instructions and requirements (macOS, Linux, Windows) |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT
