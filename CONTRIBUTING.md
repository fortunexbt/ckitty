# Contributing to ckitty

Keep ckitty small, deterministic, and pleasant to run. Changes should fit the single-binary architecture: terminal concerns belong in `src/ckitty.c`; procedural generation and canvas operations belong in `src/ckitty_core.c`/`.h` and must not depend on ncurses.

## Development

Install a compiler and ncurses, then run:

```sh
make check
make sanitize
```

`make check` includes the dependency-free rendering tests and CLI/error tests. Headless rendering is available with `./ckitty --dump --seed 123 --pose sit --frame 0`, so terminal interaction is not required for most changes.

## Code and rendering guidelines

- Use the existing C11 style and compile without warnings.
- Keep seeded output deterministic. Any random choice must come from `ckitty_rng`, not `rand()` or process-global state.
- Clip through the canvas API; never write directly outside a canvas.
- Preserve a useful plain-ASCII fallback and test small canvases.
- Keep animation state stable per kitty so environmental details do not flicker.
- Add or update tests when changing rendering, parsing, or lifecycle behavior.

New poses should remain recognizable at roughly 80 columns and should have sensible behavior on a small terminal. Avoid adding dependencies for a visual effect that can be expressed with the current canvas primitives.

## Pull requests

Describe the user-visible behavior, include the seed/pose/frame for rendering changes, and report the commands used for verification. Do not commit generated binaries or local terminal captures. Update `README.md` or `EXAMPLES.md` when adding a supported option.

## Reporting bugs

Include the operating system, compiler, ncurses version if relevant, terminal size, command line, seed, and whether `--dump` reproduces the issue. For memory-safety issues, include the output of `make sanitize`.
