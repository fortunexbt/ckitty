# ckitty examples

All examples use the one supported binary, `ckitty`. Run `make` first or replace `ckitty` with `./ckitty` while working from the repository.

## Interactive

```sh
ckitty                         # plain ASCII, random seed and pose
ckitty -c                      # color when the terminal supports it
ckitty --ascii                 # explicitly disable color
NO_COLOR=1 ckitty -c           # accessibility-friendly plain output
ckitty -l -c                   # reveal the kitty piece by piece
ckitty -S -c -m "back soon"    # screensaver with a message
```

Press `q` or Escape to quit, Space to cycle poses, and `n` to create a new kitty.

## Reproducible frames

```sh
ckitty --dump --seed 42 --pose sit --frame 0
ckitty --dump --seed 42 --pose sleep --frame 20
ckitty --dump --seed 42 --pose play --frame 0
ckitty --dump --seed 42 --pose walk --frame 12
```

The same seed, pose, frame, width, and height produce the same output. This makes it easy to save a frame or compare renderer changes:

```sh
ckitty --dump --seed 123 --pose sit --frame 0 > kitty.txt
ckitty --dump --seed 123 --pose sit --frame 0 | diff -u kitty.txt -
```

## Animation tuning

```sh
ckitty -c -d 80000             # relaxed pace
ckitty -r -d 20000             # colorful, fast rainbow
ckitty -l -g 90000             # slower reveal
ckitty -S -s 2024 -p sleep     # screensaver with a fixed pose
```

Delays are microseconds and are clamped to a safe 1 ms minimum. `--dump` is the recommended mode for CI and does not require a terminal.

## Terminal behavior

The app redraws its canvas after terminal resize and clips art at the available edges. Color is optional; plain ASCII remains the fallback for terminals without color support or when `--ascii`/`NO_COLOR` is used. A terminal around 80×24 gives the most room for environmental details, but smaller sizes are supported.
