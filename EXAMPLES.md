# ckitty, casually

All examples use the one supported binary, `ckitty`. Run `make` first or replace `ckitty` with `./ckitty` while working from the repository.

## Interactive

```sh
ckitty                          # warm amber, random kitty, endless animation
ckitty play                     # start in a playful pose
ckitty --theme moon sleep        # a cool evening palette
ckitty --theme forest            # a soft green palette
ckitty --quiet                   # start with the interface hidden
ckitty --ascii                   # plain, color-free output
NO_COLOR=1 ckitty                # disable color through the standard convention
ckitty -l                        # reveal the kitty piece by piece
ckitty -S -m "back soon"  # screensaver with a message
```

Press Space to cycle poses, or choose one directly: `1` for sit, `2` for sleep, `3` for play, and `4` for walk. Press `n` for a new kitty, `t` to cycle amber, moon, and forest palettes, or `h` to hide or show the interface.

`p` pauses and resumes motion, reveal progress, and screensaver movement. `?` opens help and temporarily pauses the scene. Escape closes help, or quits when help is closed; `q` always quits.

## Reproducible frames

```sh
ckitty --dump --seed 42 sit --frame 0
ckitty --dump --seed 42 sleep --frame 20
ckitty --dump --seed 42 play --frame 0
ckitty --dump --seed 42 walk --frame 12
```

The same seed, pose, frame, width, and height produce the same output. `--dump` is always free of ANSI escapes, regardless of theme, and does not require a terminal. This makes it easy to save a frame or compare renderer changes:

```sh
ckitty --dump --seed 123 sit --frame 0 > kitty.txt
ckitty --dump --seed 123 sit --frame 0 | diff -u kitty.txt -
```

## Animation tuning

```sh
ckitty -d 80000                # relaxed pace
ckitty -r -d 20000             # colorful, fast rainbow
ckitty -l -g 90000             # slower reveal
ckitty -S -s 2024 sleep         # screensaver with a fixed pose
```

Delays are microseconds and are clamped to a safe 1 ms minimum. `--dump` is the recommended mode for CI and does not require a terminal.

## Terminal behavior

The scene and interface adapt to the space available and redraw after terminal resize. Tiny terminals show a resize hint. A terminal around 80×24 gives the most room for environmental details, but smaller sizes are supported.

The default theme is `amber`; select `amber`, `moon`, or `forest` with `--theme`. Palettes use 256 colors when available, with an eight-color fallback. `--ascii` and `NO_COLOR` disable color even when a theme is selected. The app also works in terminals without color support.
