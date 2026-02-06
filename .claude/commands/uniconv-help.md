You are a uniconv CLI expert. Answer the user's question about uniconv usage.

## User Question

$ARGUMENTS

## uniconv Quick Reference

**Core syntax:** `uniconv [OPTIONS] <source> "<pipeline>"`

### Pipeline Operators
| Operator | Meaning | Example |
|----------|---------|---------|
| `\|` | Sequential stage | `"pdf \| docx"` |
| `,` | Parallel elements | `"jpg, png, webp"` |
| `@` | Plugin selection | `"mp4@ffmpeg"` |
| `--key val` | Plugin option | `"jpg --quality 90"` |

### Global Options
`-o <path>` output, `-f` force, `-r` recursive, `-p <preset>` preset, `--json`, `--verbose`, `--quiet`, `--dry-run`

### Builtins
- **tee**: Branch — `"tee | jpg, png, webp"` (cannot be first/last stage)
- **clipboard**: Copy to clipboard — `"jpg | clipboard"` (no file by default; `--save` to keep file)
- **_ / pass / noop**: Passthrough

### Clipboard Behavior
| Format | Copies | File saved? |
|--------|--------|-------------|
| Image (jpg, png, gif, bmp, tiff, webp) | Content | No (use `--save`/`-o`) |
| Text (txt, md, json, xml, csv, html, yaml) | Content | No (use `--save`/`-o`) |
| Other (mp4, avi, etc.) | Path | Error without `-o`/`--save` |

### Output Resolution (priority)
1. `--output` in pipeline element
2. `-o` flag (auto-appends extension if missing)
3. Default: `<stem>.<target>` in current directory

### Subcommands
- `info <file>` — file metadata
- `plugin list|install|remove|info|search|update` — plugin management
- `preset list|create|show|delete|export|import` — saved pipelines
- `config list|get|set` — settings
- `watch <dir> "<pipeline>"` — auto-convert on changes
- `update [--check]` — self-update

### Common Patterns
```bash
uniconv photo.heic "jpg --quality 90"              # Basic conversion
uniconv photo.heic "tee | jpg, png, webp"          # Multi-format
uniconv photo.jpg "pdf | docx"                     # Chain stages
uniconv -r ./photos "jpg --quality 85"             # Batch convert
uniconv photo.heic "png | clipboard"               # Copy to clipboard
uniconv -o ./out -f photo.heic "webp"              # Output path + force
uniconv plugin install +essentials                  # Install plugins
uniconv -p web-ready photo.heic                    # Use preset
```

## Instructions

Answer the user's question concisely using the reference above. If they ask about a specific feature, provide examples. If they ask about plugin options, suggest `uniconv plugin info <name>` to check available options.
