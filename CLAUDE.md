# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**uniconv** is a universal file converter and content intelligence tool written in C++20. It uses an ETL-based pipeline syntax (Transform, Extract, Load) with a plugin system supporting both native (C/C++) and CLI-based plugins.

## Build Commands

```bash
# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run tests
ctest           # All tests
ctest -V        # Verbose output

# Single executable
./build/uniconv
```

**CMake Options:**
- `UNICONV_BUILD_TESTS` (ON) - Enable unit tests

## Architecture

### ETL Pattern

All operations follow ETL philosophy (determined automatically from target):
- **Transform**: Format conversion (jpg, png, mp4)
- **Extract**: Content extraction (faces, audio, text)
- **Load**: Upload/save (gdrive, s3, notion)

### Pipeline Syntax

```bash
# Pipeline must be quoted to prevent shell interpretation of |
uniconv "source | stage1 | stage2 | stage3"
uniconv "photo.heic | jpg --quality 90 | tee | gdrive, s3"
uniconv -o output.png "photo.jpg | png"
```
- `|` separates stages (sequential execution)
- `,` separates elements within stage (parallel/branching)
- `tee` builtin replicates output for branching
- Plugin-specific options (--quality, --width, --height) go with the target

### CLI Options

**Core options** (before the pipeline):
- `-o, --output` - Output path
- `-f, --force` - Overwrite existing files
- `--json` - JSON output
- `--quiet` / `--verbose` - Output control
- `--dry-run` - Show what would be done
- `-r, --recursive` - Process directories recursively
- `-p, --preset` - Use preset
- `--watch` - Watch directory for changes

**Subcommands:**
- `info <file>` - Show file information
- `formats` - List supported formats
- `preset list|create|delete|show|export|import` - Manage presets
- `plugin list|install|remove|info|search|update` - Manage plugins (supports `+<collection>` install)
- `config get|set|list` - Manage configuration

### Plugin System

**Two plugin types:**
1. **Native Plugins (C/C++)** - Shared libraries implementing C ABI from `include/uniconv/plugin_api.h`
2. **CLI Plugins** - External executables with JSON-based I/O protocol

**Plugin identification:** `<scope>.<etl_type>` (e.g., `ffmpeg.transform`, `vips.transform`)

**Discovery paths:** `~/.uniconv/plugins/`, `/usr/local/lib/uniconv/`, `./plugins/`

### Core Components

| Component | Location | Purpose |
|-----------|----------|---------|
| Engine | `src/core/engine.*` | ETL request orchestrator |
| PluginManager | `src/core/plugin_manager.*` | Plugin registry & resolution |
| PipelineExecutor | `src/core/pipeline_executor.*` | Multi-stage execution |
| PipelineParser | `src/cli/pipeline_parser.*` | Pipeline syntax parsing |
| CliParser | `src/cli/parser.*` | CLI11-based argument parsing |

### Data Flow

1. **CliParser** → Routes command to handler
2. **PipelineParser** → Builds pipeline AST from syntax
3. **Engine** → Creates ETL requests, resolves plugins
4. **PluginManager** → Finds and executes correct plugin
5. **PipelineExecutor** → Orchestrates stages, manages temp files

### Output Path Resolution

For final stage output (in order of priority):
1. Explicit output in pipeline element: `jpg --output foo.jpg`
2. CLI `-o` option:
   - No extension: append target extension (`-o ./temp/output` → `./temp/output.png`)
   - Has extension: use as-is (`-o ./temp/output.png` → `./temp/output.png`)
   - Note: multiple outputs (tee) with explicit extension will overwrite the same file
3. Default: current directory with input stem + target extension

## Code Conventions

- Files: `snake_case.cpp/.h`
- Classes/Structs: `PascalCase`
- Functions: `snake_case()`
- Variables: `snake_case`
- Member variables: `snake_case_`
- Constants: `kPascalCase`
- Namespaces: `uniconv::core`, `uniconv::cli`, etc.

## Key Dependencies

- **CLI11** v2.4.2 - Command-line parsing
- **nlohmann/json** v3.11.3 - JSON serialization
- **Google Test** v1.14.0 - Unit testing

Note: Image conversion (libvips) is now provided by the external `image-convert` plugin in `plugins/image-convert/`.
