# ckitty — terminal kitty generator

`ckitty` is a small GPL-3.0 terminal companion inspired by [cbonsai](https://gitlab.com/jallbrit/cbonsai). It procedurally generates a kitty with a stable seed, then animates its tail, face, whiskers, pose, and surroundings.

## What it does

- One canonical `ckitty` binary, with a dependency-free rendering core.
- Sitting, sleeping, playing, and walking poses.
- Deterministic seeds and headless `--dump` output for scripts and CI.
- Live piece-by-piece reveal, screensaver spawning, optional color, and rainbow mode.
- Plain ASCII by default; `--ascii` or `NO_COLOR=1` disables terminal colors.
- Safe clipping and redraw on small or resized terminals.

## A frame

```text
        /\_/\
   -- ( o^O ) --
    -- > 3 < --
     /~~~~~~~~~\
     |o:oooo   |       ~~~...
     \_________/~~~~~~~
       (_)   (_)
```

The exact fur, pose, tail motion, and small companions vary by seed while the silhouette stays readable.

## Build and test

Requirements: a C11 compiler, ncurses, and libm.

```sh
make
make check              # build, core tests, CLI/error tests
make sanitize           # AddressSanitizer + UndefinedBehaviorSanitizer
```

On macOS, Homebrew ncurses is detected automatically when installed:

```sh
brew install ncurses
make check
```

On Debian/Ubuntu, install `libncurses-dev`; Fedora uses `ncurses-devel`.

## Run

```sh
./ckitty -c
./ckitty -l -c                  # live reveal
./ckitty -S -c                  # screensaver
./ckitty -s 42 -p play -c       # reproducible playing kitty
./ckitty --dump --seed 123 --pose sit --frame 0
```

Use `ckitty -h` for the authoritative option list. Interactive controls are `q`/Escape to quit, Space to cycle poses, and `n` to create a new kitty.

`--dump` never initializes ncurses and writes only the minimal non-blank ASCII bounding box, making it useful in pipes and automated tests. `--width` and `--height` select its canvas; the renderer rejects unreasonably large canvases rather than risking an overflow.

## Install

```sh
./install.sh --prefix "$HOME/.local"
# or, for a system prefix:
sudo make install PREFIX=/usr/local
```

The installer runs `make check` and never invokes `sudo` itself. Use `--skip-tests` only when a package build system has already verified the source. Remove an installed binary with `make uninstall PREFIX=...`.

## Release checklist

1. Update `CKITTY_VERSION` in `src/ckitty.c` and the matching documentation if the release changes.
2. Run `make clean`, `make check`, and `make sanitize` on a supported host.
3. Verify `./ckitty --dump --seed 123 --pose sit --frame 0` and `./ckitty --help`.
4. Build with the release compiler on Linux and macOS, then package the source tree (including `src/`, `tests/`, `Makefile`, and `LICENSE`).
5. Install into a temporary prefix and verify `bin/ckitty`, then run `make uninstall` for that prefix.

There are no generated or versioned v1/v2/v3 binaries; `ckitty` is the supported product name.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [EXAMPLES.md](EXAMPLES.md).

## License

GPL-3.0. See [LICENSE](LICENSE).
