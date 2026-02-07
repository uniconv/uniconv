# CLI Reference

```
uniconv [options] <source> "<pipeline>"
uniconv <command> [args]
```

## Global options

| Flag | Description |
|------|-------------|
| `-o, --output <path>` | Output file path |
| `-f, --force` | Overwrite existing files |
| `-r, --recursive` | Process directories recursively |
| `--input-format <fmt>` | Override auto-detected input format for stdin |
| `--timeout <seconds>` | Plugin timeout (0 = no timeout) |
| `--json` | Output results as JSON |
| `--dry-run` | Show what would happen without executing |
| `--quiet` | Suppress output |
| `--verbose` | Detailed output |
| `-p, --preset <name>` | Use a saved preset |
| `-v, --version` | Show version |
| `-h, --help` | Show help |

## Source

| Source | Meaning |
|--------|---------|
| `photo.heic` | File path |
| `-` (with piped data) | Read from stdin (format auto-detected) |
| `-` (no piped data) | Generator mode (no input file) |

## Commands

### File operations

| Command | Description |
|---------|-------------|
| `info <file>` | Show file details (format, size, dimensions) |
| `watch <dir> "<pipeline>"` | Watch directory and process new files |

### Plugin management

| Command | Description |
|---------|-------------|
| `plugin list` | List installed plugins |
| `plugin install <name>` | Install a plugin or collection (`+name`) |
| `plugin remove <name>` | Uninstall a plugin |
| `plugin update [name]` | Update one or all plugins |
| `plugin search <query>` | Search the registry |
| `plugin info <name>` | Show plugin details |

### Presets

| Command | Description |
|---------|-------------|
| `preset list` | List saved presets |
| `preset create <name> "<pipeline>"` | Save a preset |
| `preset delete <name>` | Remove a preset |
| `preset show <name>` | Show preset details |

### Configuration

| Command | Description |
|---------|-------------|
| `config list` | List configuration |
| `config get <key>` | Get a config value |
| `config set <key> <value>` | Set a config value |

### System

| Command | Description |
|---------|-------------|
| `update` | Update uniconv to the latest version |
| `update --check` | Check for updates without installing |
