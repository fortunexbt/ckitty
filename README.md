# ckitty

<p align="center">
  <img src="https://raw.githubusercontent.com/fortunexbt/ckitty/main/assets/ckitty-banner.svg" alt="ckitty, a tiny terminal cat" width="900">
</p>

<p align="center">
  A tiny, animated terminal cat for the moments when your terminal needs a friend.
</p>

<p align="center">
  <a href="https://github.com/fortunexbt/ckitty/blob/main/LICENSE">GPL-3.0</a>
  · C11 · ncurses · macOS + Linux
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/fortunexbt/ckitty/main/assets/ckitty-demo.gif" alt="ckitty running in a terminal" width="900">
</p>

ckitty is a small native program that draws procedural ASCII cats directly in your terminal. It has a few good poses, a gently animated tail, tiny surprises, and enough randomness to feel alive without becoming noisy.

No account. No runtime. No Electron. Just a cat.

## Start here

If you have a macOS or Linux machine, this is the whole installation:

```sh
curl -fsSL https://raw.githubusercontent.com/fortunexbt/ckitty/main/install.sh \
  | sh -s -- --prefix "$HOME/.local"
```

Then:

```sh
ckitty
```

The installer builds and tests the one canonical `ckitty` binary, installs it under `$HOME/.local/bin`, and never calls `sudo`. If that directory is not on your `PATH` yet:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Want to try it without installing anything?

```sh
git clone https://github.com/fortunexbt/ckitty.git
cd ckitty
make
./ckitty
```

## The nice bits

- `ckitty` — a cozy default kitty
- `ckitty play` — a little more energy
- `ckitty --ascii` — plain, color-free output
- `ckitty -l` — a piece-by-piece reveal
- `ckitty -S -m "back soon"` — a quiet screensaver

While it is running:

```text
space   change pose
n       summon a new kitty
q / esc go home
```

The layout redraws safely when the terminal changes size. Very small windows get a friendly resize message instead of mangled art. Color is automatic when supported; `--ascii` and `NO_COLOR=1` keep things readable everywhere.

## A deterministic kitty

For scripts, screenshots, tests, or just getting the same cat twice:

```sh
./ckitty --dump --seed 123 sit
```

`--dump` does not initialize ncurses. It writes one clean frame to standard output, and it accepts `--frame`, `--width`, and `--height` when you need a particular canvas.

Most people never need the rest of the options. If you do, `ckitty --help` has the complete list.

## Build it yourself

Requirements: a C11 compiler, ncurses, and libm.

```sh
make
make check
make sanitize
```

On macOS, Homebrew’s ncurses is detected automatically:

```sh
brew install ncurses
```

On Debian or Ubuntu, install `libncurses-dev`. Fedora calls the package `ncurses-devel`.

To install from a checkout:

```sh
./install.sh --prefix "$HOME/.local"
```

For a system prefix, use `sudo make install PREFIX=/usr/local` explicitly. To remove an installation, run `make uninstall PREFIX=...`.

## For contributors

The renderer is deliberately small and deterministic. The useful checks are:

```sh
make check       # build, renderer tests, CLI tests, resize tests
make sanitize    # AddressSanitizer + UndefinedBehaviorSanitizer
make demo        # rebuild the README GIF (ImageMagick + FFmpeg)
```

`assets/ckitty-demo.gif` is made from real `--dump` frames, so the gallery stays honest. `tools/make-demo.sh` keeps the capture reproducible and the README animation intentionally slow enough to look at.

More examples live in [EXAMPLES.md](EXAMPLES.md). The project layout is in [docs/PROJECT_STRUCTURE.md](docs/PROJECT_STRUCTURE.md), and contribution notes are in [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-3.0 — see [LICENSE](LICENSE).
