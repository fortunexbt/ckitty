# ckitty

<p align="center">
  <img src="https://raw.githubusercontent.com/fortunexbt/ckitty/main/assets/ckitty-banner.svg" alt="ckitty, a tiny terminal cat" width="900">
</p>

<p align="center">
  A tiny, animated terminal cat for the moments when your terminal needs a friend.
</p>

<p align="center">
  <a href="https://github.com/fortunexbt/ckitty/blob/main/LICENSE">GPL-3.0</a>
  · <a href="https://github.com/fortunexbt/ckitty/releases">releases</a>
  · C11 · ncurses · macOS + Linux
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/fortunexbt/ckitty/main/assets/ckitty-demo.gif" alt="A gallery of ckitty's deterministic ASCII frames" width="900">
</p>

<p align="center"><em>A gallery of <code>--dump</code> frames; the interactive app adds color and controls.</em></p>

ckitty is a small native program that draws procedural ASCII cats directly in your terminal. It has four poses, a gently animated tail, soft ambient twinkles, and enough randomness to feel alive without becoming noisy. A warm amber palette and an unobtrusive interface make it a cozy place to leave your terminal.

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

Tagged releases also include checksumed Linux x86_64 and Apple silicon archives.

## The nice bits

- `ckitty` — a cozy kitty in warm amber
- `ckitty play` — a little more energy
- `ckitty --theme moon` — a cool evening palette (`forest` is available too)
- `ckitty --quiet` — start with the interface hidden
- `ckitty --ascii` — plain, color-free output
- `ckitty -l` — a piece-by-piece reveal
- `ckitty -S -m "back soon"` — a quiet screensaver

While it is running:

```text
space         cycle poses
1 / 2 / 3 / 4  sit / sleep / play / walk
n             summon a new kitty
p             pause or resume
t             cycle amber / moon / forest
h             hide or show the interface
?             open help (temporarily pauses the scene)
esc           close help, or quit when help is closed
q             quit from anywhere
```

Pause freezes motion, the piece-by-piece reveal, and screensaver movement. The layout adapts to smaller terminals and redraws safely on resize; very small windows get a friendly resize message. Palettes use 256 colors when available and fall back to eight colors. `--ascii` and `NO_COLOR=1` disable color, and terminals without color support stay readable.

## A deterministic kitty

For scripts, screenshots, tests, or just getting the same cat twice:

```sh
./ckitty --dump --seed 123 sit
```

`--dump` does not initialize ncurses. It writes one clean frame to standard output with no ANSI escapes, and it accepts `--frame`, `--width`, and `--height` when you need a particular canvas. Themes and interactive controls do not change its deterministic output.

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
