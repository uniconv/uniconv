# Uniconv Pipeline Format Convention

## Problem

In uniconv's pipeline execution, intermediate files are named using the pattern `s{stage}_e{element}.{target}`, where the target name is used as the file extension. This conflates two independent concepts:

- **Target**: what the plugin does (operation name)
- **Extension**: what format the plugin outputs (file format)

They overlap for converters by coincidence (`fgb` → `.fgb`), but break for operation-type targets (`extract` → `.extract`, `save` → `.save`).

Additionally, the final output extension is determined by `get_output_format()`, which walks back to the original source file extension when the last stage's target is not a recognized format. This produces incorrect results — e.g., a file containing FGB binary data with a `.geojson` extension.

## Identifier Convention

```
[scope:]target[.extension]
```

| Part | Required | Description |
|------|----------|-------------|
| `scope` | Optional | Namespace, typically the plugin name |
| `target` | Required | Operation name |
| `extension` | Optional | Output format; if omitted, defaults to target |

**Parsing rules:**

- Has `.` → extension is the part after the last dot
- No `.` → extension equals the target
- Has `:` → scope is the part before the colon

No format registry. No "known format" checks. The convention is purely syntactic.

## Manifest

```json
{
  "name": "<plugin-name>",
  "targets": ["<identifier>", ...],
  "accepts": ["<identifier>", ...],
  "sink": false
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `targets` | `string[]` | — | Identifiers this plugin can produce |
| `accepts` | `string[]` | — | Identifiers this plugin can consume |
| `sink` | `bool` | `false` | If `true`, plugin consumes input but produces no output file |

A **sink** plugin is a terminal stage that performs a side-effect (save, upload, notify) without producing output for downstream stages. When the last stage is a sink:

- uniconv does not expect an output file
- No final output extension resolution is needed
- The pipeline result is the sink plugin's success/failure status

## DAG Validation

At composition time (before execution), validate each stage transition:

```
extension(prev_stage.target) ∈ accepts(next_stage)
```

If not matched → error. No guessing, no fallback.

## Temp Path Generation

```
extension = parse(target).extension
path = /tmp/uniconv/run_{pid}_{ts}/s{stage}_e{element}.{extension}
```

## Final Output Extension Resolution

The final output extension depends on whether the last stage is a sink.

```
Last stage is sink?
  yes → no output file; pipeline result = success/failure
  no  → resolve extension from last plugin's output
```

**Resolution order (non-sink last stage):**

| Priority | Condition | Extension source |
|----------|-----------|------------------|
| 1 | Last plugin returns `output` in JSON | Extension from the reported path |
| 2 | Last plugin returns no `output` | Extension from the generated temp path (convention-parsed) |
| 3 | Neither resolves | Error |

```
Case 1: Last stage is sink (save, upload, notify)
  manifest: sink = true
  → no final output file
  → pipeline result: { "success": true }

Case 2: Last stage is non-sink, plugin reports output
  plugin returns:   { "output": "/tmp/.../s2_e0.fgb" }
  final extension:  .fgb  ← from plugin's reported path

Case 3: Last stage is non-sink, plugin reports no output
  generated temp:   s2_e0.fgb  (from convention)
  final extension:  .fgb  ← from generated temp path

Case 4: Last stage is non-sink, no output, meaningless extension
  → error: cannot determine output format
```

**Key principle:** uniconv never walks back to the original input extension. The last stage owns the output format. Sinks explicitly declare they produce nothing.

## Examples

### Converter plugin (ogr)

```json
{
  "name": "ogr",
  "targets": ["fgb", "geojson", "csv"],
  "accepts": ["geojson", "fgb", "shp"]
}
```

- `ogr:fgb` → parse: no dot, extension = `fgb`
- `ogr:geojson` → parse: no dot, extension = `geojson`

### Operation plugin (postgis)

```json
{
  "name": "postgis",
  "targets": ["extract.geojson", "extract.csv"],
  "accepts": ["postgis"]
}
```

- `postgis:extract.geojson` → parse: has dot, extension = `geojson`
- `postgis:extract.csv` → parse: has dot, extension = `csv`

### Sink plugin (save)

`save` is a sink plugin. It copies the file somewhere but produces no output for the pipeline.

```json
{
  "name": "save",
  "targets": ["save"],
  "accepts": ["fgb", "geojson", "csv", "..."],
  "sink": true
}
```

Pipeline: `input.geojson | ogr:fgb | save`

```
Stage 1 (ogr:fgb):
  extension = fgb
  temp path = s1_e0.fgb
  plugin output: { "output": "/tmp/.../s1_e0.fgb" }

Stage 2 (save):
  sink = true
  plugin receives s1_e0.fgb as input, copies it to save directory
  plugin output: { "success": true }
  → no final output file (sink)
```

No extension guessing, no walk-back. The pipeline simply ends.

### Other sink examples

- `upload` — pushes data to a remote service
- `notify` — sends a notification
- `clipboard` — copies content to clipboard

All declared with `"sink": true` in their manifest.

## Full Pipeline Example

```
postgis:extract.geojson | ogr:fgb | save

Stage 0: postgis:extract.geojson
  parse → target=extract, extension=geojson
  temp path → s0_e0.geojson
  --output /tmp/.../s0_e0.geojson
  DAG check: N/A (first stage)

Stage 1: ogr:fgb
  parse → target=fgb, extension=fgb
  DAG check: prev produces "geojson", ogr accepts "geojson" ✓
  temp path → s1_e0.fgb
  --output /tmp/.../s1_e0.fgb

Stage 2: save
  sink = true
  DAG check: prev produces "fgb", save accepts "fgb" ✓
  plugin receives s1_e0.fgb, copies to save directory
  plugin output: { "success": true }

Final output: none (sink terminal)
Pipeline result: success
```

## Changes Required

| Component | Change |
|-----------|--------|
| Manifest spec | Add `accepts` field, support identifier convention in `targets` |
| `target_to_extension()` | Replace with convention parser |
| `generate_temp_path()` | Use parsed extension |
| DAG composition | Validate using `accepts` + parsed extension |
| `finalize_outputs()` | Skip output for sink stages; use last plugin's output extension for non-sink; no walk-back |
| `get_output_format()` | Remove walk-back logic; check sink flag first, then follow resolution order |
| Plugin manifests | Update all `plugin.json` to new format |
| Backward compat | If no dot in target, extension = target (existing behavior preserved) |

## Summary of Principles

1. **Target ≠ extension.** They are independent concepts that happen to overlap for converters.
2. **Convention is the contract.** `[scope:]target[.extension]` — purely syntactic, no format registry.
3. **Last plugin owns the output.** Final extension comes from the last plugin's reported output, not from walking back to the source.
4. **No guessing.** If DAG can't validate, error. If final extension can't resolve, error.
5. **Sinks are explicit.** Plugins that consume input without producing output declare `"sink": true`. No ambiguity about missing output.
