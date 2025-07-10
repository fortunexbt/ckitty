# ckitty Examples Gallery

A collection of example commands and their expected outputs.

## Basic Examples

### Simple Kitty
```bash
./ckitty_v3
```
Generates a random kitty without colors in a random pose.

### Colorful Kitty
```bash
./ckitty_v3 -c
```
Same as above but with colors enabled.

### Live Generation Mode
```bash
./ckitty_v3 -l -c
```
Watch the kitty being drawn piece by piece, similar to cbonsai's branch growth.

## Advanced Examples

### Specific Poses

#### Sitting Kitty
```
     /\_/\
    ( o.o )     A content kitty sitting upright
     > ^ <      with alert whiskers
    (  *  )
    |  *  |
    (_)(_)
```

#### Sleeping Kitty
```
    zzZ
   (^.^)      Curled up and dreaming
  (     )     with occasional z's floating up
  (_____) 
```

#### Playing Kitty
```
      /\_/\
     ( o.o )  \     Pouncing position with
      > ^ </   \    raised paws and excited tail
    =======     @   Often appears with yarn ball
    /     \    @@@
```

## Seed Examples

### Memorable Seeds

```bash
# Fat fluffy kitty
./ckitty_v3 -c -s 42

# Thin elegant kitty
./ckitty_v3 -c -s 1337

# Hyperactive playing kitty
./ckitty_v3 -c -s 9999

# Always sleeping kitty
./ckitty_v3 -c -s 2024
```

## Animation Examples

### Rainbow Mode
```bash
./ckitty_v3 -r
```
Kitty cycles through all available colors.

### Slow Motion
```bash
./ckitty_v3 -c -d 100000
```
Slower animations for a relaxed viewing experience.

### Fast Motion
```bash
./ckitty_v3 -c -d 10000
```
Hyperactive kitty with rapid animations.

## Screensaver Mode

### Basic Screensaver
```bash
./ckitty_v3 -S -c
```
Generates new kitties continuously at random positions.

### Screensaver with Custom Message
```bash
./ckitty_v3 -S -c -m "Gone fishing, back soon!"
```

## Environmental Elements

Kitties can appear with various objects:

- **Yarn Ball**: Pink ball of yarn with trailing string
- **Mouse**: Small gray mouse (appears during play)
- **Birds**: Flying overhead (random chance)

## Terminal Compatibility

### For Best Results

1. **Terminal Size**: Minimum 80x24 recommended
2. **Color Support**: 256 colors preferred
3. **Font**: Monospace font required

### Tested Terminals

- **iTerm2** (macOS) - Excellent
- **Terminal.app** (macOS) - Good
- **gnome-terminal** (Linux) - Excellent
- **xterm** (Linux) - Good
- **Windows Terminal** (Windows) - Good
- **PuTTY** (Windows) - Basic

## Fun Combinations

### Party Mode
```bash
./ckitty_v3 -r -d 20000 -S
```

### Zen Mode
```bash
./ckitty_v3 -c -s 2024 -d 80000
```

### Debug Mode
```bash
./ckitty_v3 -c -s 12345
```
Always generates the same kitty for testing.