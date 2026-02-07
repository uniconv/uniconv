# Builtin Stages

uniconv includes built-in stages for pipeline control and system integration.

## tee — Branching

Split a pipeline into multiple parallel branches:

```bash
# Convert to multiple formats at once
uniconv photo.heic "tee | jpg, png, webp"

# Different quality settings for each branch
uniconv photo.jpg "tee | jpg --quality 90, jpg --quality 50 --width 200"
```

## clipboard — Copy to System Clipboard

Copy output directly to the system clipboard:

```bash
# Convert and copy to clipboard (no file saved)
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

## Passthrough — No-op Stage

Pass input through unchanged. Useful for matching `tee` branches:

```bash
# Convert to PNG and copy ASCII to clipboard, pass through original to save
uniconv photo.heic "tee | png, ascii | _, clipboard"

# Skip one branch entirely
uniconv video.mp4 "tee | gif --width 320, _ | clipboard, _"
```

**Aliases:** `_`, `echo`, `bypass`, `pass`, `noop`
