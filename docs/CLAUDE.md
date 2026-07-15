# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build
make

# Clean build artifacts
make clean

# Run all tests (core + CLI)
make test
make test-cli
make check

# Run ASan/UBSan checks
make sanitize

# Install
sudo make install
```

## Testing and Running

```bash
# Basic run (after building)
./ckitty -c

# Live generation mode (watch it grow like cbonsai)
./ckitty -l -c

# Test specific features
./ckitty -c -s 123     # Specific seed for reproducible kitty
./ckitty -S -c         # Screensaver mode
./ckitty -h            # Help to verify options parsing

# Render one frame without ncurses (useful for debugging/CI)
./ckitty --dump --seed 123 --pose sit --frame 0
```

## Architecture Overview

The project builds a single unified `ckitty` binary with a small core + ncurses front-end.

### Modules

- `src/ckitty_core.c` / `src/ckitty_core.h`: deterministic procedural generation and a simple canvas.
- `src/ckitty.c`: CLI parsing + ncurses loop (input, timing, rendering, screensaver/live mode).

### Rendering Pipeline (ncurses)

1. Clear screen
2. (Optional) draw ground
3. If `--live` and the kitty isn't "grown" yet: reveal a deterministic shuffled draw order.
4. Otherwise: call `ckitty_render_frame()` for the current frame.
5. Render canvas cells with optional colors/rainbow.
6. Handle user input.

### Critical Dependencies

- **ncurses**: All terminal manipulation, requires proper initialization/cleanup
- **math.h**: For sine-based tail sway and curved sleeping pose
- **Color pairs**: Initialized only when requested and supported by the terminal

### Tests

- Core tests live in `tests/test_ckitty_core.c` and run via `make test`. They avoid ncurses entirely.

The codebase follows cbonsai's philosophy of deterministic procedural generation with a fun live reveal. `ckitty` is the only supported binary; the historical v1/v2/v3 source files are not part of the build.
