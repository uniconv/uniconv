# Contributing to uniconv

Thanks for your interest in contributing to uniconv. This guide covers everything you need to get started.

## Ways to Contribute

- **Report bugs** — Open an issue with steps to reproduce and expected vs. actual behavior.
- **Write a plugin** — Build a converter for a format you care about and publish it to the registry.
- **Improve the core** — Pick up an open issue or propose an enhancement.
- **Improve docs** — Fix typos, add examples, clarify explanations.

## Development Setup

### Prerequisites

- CMake 3.20+
- C++20 compiler (Clang 14+, GCC 12+, MSVC 17+)
- Git

### Building (macOS / Linux)

```bash
git clone https://github.com/uniconv/uniconv.git
cd uniconv/uniconv
mkdir build && cd build
cmake .. -DUNICONV_BUILD_TESTS=ON
cmake --build .
```

### Building (Windows)

Requires Visual Studio 2022 with "Desktop development with C++" workload.

```powershell
git clone https://github.com/uniconv/uniconv.git
cd uniconv\uniconv
mkdir build && cd build
cmake .. -DUNICONV_BUILD_TESTS=ON
cmake --build . --config Debug
```

### Running Tests

```bash
# macOS / Linux
cd build
ctest          # Run all tests
ctest -V       # Verbose output
ctest -R name  # Run specific test
```

```powershell
# Windows
cd build
ctest -C Debug
ctest -C Debug -V
ctest -C Debug -R name
```

## Code Conventions

| Element | Style | Example |
|---------|-------|---------|
| Files | `snake_case` | `plugin_manager.cpp` |
| Classes / Structs | `PascalCase` | `PluginManager` |
| Functions | `snake_case` | `find_plugin()` |
| Variables | `snake_case` | `plugin_count` |
| Member variables | `snake_case_` (trailing underscore) | `name_` |
| Constants | `kPascalCase` | `kMaxRetries` |
| Namespaces | lowercase | `uniconv::core` |

### Namespaces

- `uniconv::core` — Engine, plugin manager, pipeline executor
- `uniconv::cli` — Command-line parsing and commands
- `uniconv::plugins` — Plugin loader interfaces
- `uniconv::utils` — File, string, HTTP, version utilities

## Project Structure

```
uniconv/
├── uniconv/               # Core CLI application (C++20)
│   ├── src/
│   │   ├── core/          # Engine, plugin manager, pipeline executor
│   │   ├── cli/           # Command-line parsing and commands
│   │   ├── plugins/       # Plugin loader interfaces
│   │   └── utils/         # File, string, HTTP, version utilities
│   ├── include/uniconv/   # Public headers (plugin_api.h)
│   └── tests/             # Google Test unit tests
├── plugins/               # Example plugins
│   ├── ascii/             # Python CLI plugin
│   ├── image-filter/      # Python CLI plugin
│   ├── image-convert/     # Native C++ plugin
│   └── video-convert/     # Native C++ plugin
└── registry/              # Plugin registry (index, manifests, collections)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `UNICONV_BUILD_TESTS` | `ON` | Build unit tests |
| `UNICONV_ENABLE_WARNINGS` | `ON` | Enable compiler warnings |
| `UNICONV_WITH_FFMPEG` | `ON` | FFmpeg log suppression (if available) |

## Architecture

### Key Components

| Component | Files | Purpose |
|-----------|-------|---------|
| Engine | `core/engine.*` | ETL request orchestration |
| PluginManager | `core/plugin_manager.*` | Plugin registry and resolution |
| PipelineExecutor | `core/pipeline_executor.*` | Multi-stage pipeline execution |
| PipelineParser | `cli/pipeline_parser.*` | Pipeline syntax parsing |
| CliParser | `cli/parser.*` | CLI11-based argument parsing |
| RegistryClient | `core/registry_client.*` | Plugin registry interaction |

### Data Flow

```
CLI Input → CliParser → PipelineParser → Engine → PluginManager → PipelineExecutor → Output
```

1. **CliParser** routes the command to the appropriate handler.
2. **PipelineParser** builds a pipeline AST from the `"source | target | target"` syntax.
3. **Engine** creates ETL requests and resolves plugins.
4. **PluginManager** finds and executes the correct plugin (native or CLI).
5. **PipelineExecutor** orchestrates stages and manages intermediate files.

## Writing Plugins

Plugins are the primary way to extend uniconv. There are two plugin types.

### CLI Plugins

CLI plugins are the easiest way to add support for a new format. They can be written in any language. The plugin is invoked as a subprocess and communicates via command-line arguments and JSON on stdout.

**Minimal example (Python):**

```python
#!/usr/bin/env python3
import argparse, json

parser = argparse.ArgumentParser()
parser.add_argument("--input", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--target", required=True)
args = parser.parse_args()

# ... perform the conversion ...

print(json.dumps({
    "success": True,
    "output_path": args.output,
    "metadata": {}
}))
```

**Required manifest (`plugin.json`):**

```json
{
  "name": "my-plugin",
  "scope": "my-plugin",
  "version": "1.0.0",
  "description": "My custom converter",
  "interface": "cli",
  "executable": "my_plugin.py",
  "targets": ["my-format"],
  "accepts": ["jpg", "png"]
}
```

**Manifest fields:**

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `targets` | `map` or `array` | — | Targets this plugin can produce. Array shortcut: `["jpg"]` → `{"jpg": []}`. Map for non-trivial: `{"extract": ["geojson", "csv"]}` |
| `accepts` | `string[]` (optional) | omit = accept all | Input formats. Omitted = accept any input, `[]` = accept nothing |
| `sink` | `bool` | `false` | If `true`, plugin owns output (upload, save) — uniconv skips finalization |

Place the plugin directory in `~/.uniconv/plugins/` for automatic discovery.

### Native Plugins

Native plugins are shared libraries (`.so`, `.dylib`, `.dll`) implementing the C ABI defined in [`include/uniconv/plugin_api.h`](include/uniconv/plugin_api.h).

**Required exports:**

```c
UNICONV_EXPORT UniconvPluginInfo* uniconv_plugin_info(void);
UNICONV_EXPORT UniconvResult*     uniconv_plugin_execute(const UniconvRequest* req);
UNICONV_EXPORT void               uniconv_plugin_free(void* ptr);
```

See [`plugins/image-convert`](https://github.com/uniconv/plugins/tree/main/image-convert) and [`plugins/video-convert`](https://github.com/uniconv/plugins/tree/main/video-convert) for complete working examples.

### Publishing a Plugin

For full details on packaging, releasing, and submitting plugins to the registry, see the [Plugin Development Guide](https://github.com/uniconv/registry/blob/main/plugins/DEVELOPMENT.md) and [Publishing Guide](https://github.com/uniconv/plugins/blob/main/PUBLISHING.md).

### Windows notes for plugin development

- **Native plugins:** Bundled dependency DLLs must be placed in the same directory as the plugin `.dll`. The uniconv core uses `LoadLibraryEx` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` to locate them.
- **CLI plugins:** Work cross-platform. For Python plugins, ensure `python3` / `python` is in PATH.
- **Release scripts:** Use `python release.py` (cross-platform) instead of `./release.sh` on Windows. Both scripts have identical behavior.

## Submitting Changes

1. Fork the repository and create a feature branch from `main`.
2. Make your changes, following the code conventions above.
3. Add or update tests for any new or changed behavior.
4. Run the full test suite and confirm it passes.
5. Write a clear commit message describing the change and its motivation.
6. Open a pull request against `main`.

## Reporting Issues

When filing a bug report, include:

- uniconv version (`uniconv --version`)
- Operating system and compiler version
- Steps to reproduce
- Expected behavior vs. actual behavior
- Relevant error output or logs (`--verbose` flag)
