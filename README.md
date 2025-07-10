# 🐱 ckitty - Terminal Kitty Generator

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![ncurses](https://img.shields.io/badge/library-ncurses-green.svg)](https://invisible-island.net/ncurses/)

A procedurally generated terminal kitty, inspired by [cbonsai](https://gitlab.com/jallbrit/cbonsai)'s organic growth algorithms. Watch as ASCII art kitties come to life with different poses, animations, and personalities!

```
      /\_/\  
     ( o.o )      ckitty - Terminal kitties that
      > ^ <         grow, play, and sleep!
     /     \    
    (_)   (_)   
```

## Features

### v3 (Latest) - Procedural Generation
- **Multiple Poses**: Sitting, sleeping, playing, and walking kitties
- **Organic Animations**: Tail swaying, eye blinking, whisker twitching
- **Interactive Environment**: Yarn balls, mice, and birds
- **Advanced ASCII Art**: Curved lines, detailed features
- **Live Generation**: Watch the kitty being drawn piece by piece
- **Screensaver Mode**: Continuous generation of new kitties

### v2 - Growth System
- Step-by-step kitty construction
- Customizable tail types (straight, curved, question mark, excited)
- Variable body dimensions and fluffiness
- Different ear styles and whisker lengths

### v1 - Basic Animation
- Simple animated kitty with movement
- Basic color support

## Installation

### Requirements
- C compiler (gcc, clang)
- ncurses library
- math library (for v3)

### Building from source

```bash
# Build all versions
make

# Install latest version
sudo make install

# Build specific version
make ckitty_v3
```

### macOS (Homebrew)
```bash
brew install ncurses
make
```

### Linux
```bash
# Debian/Ubuntu
sudo apt-get install libncurses5-dev libncursesw5-dev

# Fedora
sudo dnf install ncurses-devel

# Arch
sudo pacman -S ncurses

make
```

## Usage

### Basic Commands
```bash
# Generate a random kitty
ckitty

# Watch kitty grow live (like cbonsai)
ckitty -l -c

# Specific pose with colors
ckitty -c

# Rainbow mode
ckitty -r

# Screensaver mode
ckitty -S -c

# Generate same kitty with seed
ckitty -s 42 -c

# Custom message
ckitty -m "Hello Kitty!" -c
```

### Controls
- `q` or `ESC` - Quit
- `Space` - Change pose (v3)
- `n` - New kitty in screensaver mode (v3)

### Options
```
-h, --help          Show help message
-l, --live          Live generation mode (watch it grow)
-d, --delay <ms>    Animation delay (default: 40000)
-c, --colors        Enable colors
-r, --rainbow       Rainbow mode
-s, --seed <num>    Set random seed for reproducible kitties
-i, --infinite      Run indefinitely
-S, --screensaver   Screensaver mode (continuous generation)
-m, --message <msg> Display custom message
```

## Examples

```bash
# Playful kitty with yarn ball
ckitty -c -s 123

# Sleeping kitty
ckitty -c -s 456

# Live growing rainbow kitty
ckitty -l -r

# Screensaver with custom delay
ckitty -S -c -d 20000
```

## Demo

### Quick Start
```bash
# See a random kitty
./ckitty_v3 -c

# Watch a kitty being "grown" (like cbonsai)
./ckitty_v3 -l -c

# Screensaver mode - endless kitties!
./ckitty_v3 -S -c
```

### Gallery of Kitties

Different seeds generate different kitties:

```bash
# Playful kitty with yarn ball (seed: 123)
./ckitty_v3 -c -s 123

# Sleepy kitty (seed: 456)  
./ckitty_v3 -c -s 456

# Alert kitty (seed: 789)
./ckitty_v3 -c -s 789
```

## Algorithm

Like cbonsai, ckitty uses procedural generation:

1. **Pose Selection**: Randomly chooses kitty's pose
2. **Part Generation**: Builds kitty parts in sequence
   - Tail (with curve algorithms)
   - Body (with fluffiness factor)
   - Head and facial features
   - Environmental elements
3. **Animation System**: Continuous updates for lifelike movement
4. **Physics**: Tail sway uses sine waves, whiskers have twitch probability

## Comparison with cbonsai

| Feature | cbonsai | ckitty |
|---------|---------|---------|
| Procedural generation | ✓ Trees grow organically | ✓ Kitties built step-by-step |
| Live mode | ✓ Watch branches grow | ✓ Watch kitty parts appear |
| Randomization | ✓ Unique trees | ✓ Unique kitties |
| Animation | ✓ Swaying leaves | ✓ Blinking, tail swaying |
| Customization | ✓ Tree types, sizes | ✓ Poses, colors, features |
| Screensaver | ✓ | ✓ Endless kitties |
| Seeds | ✓ Reproducible trees | ✓ Reproducible kitties |

## Contributing

Feel free to submit issues and enhancement requests!

## Credits

Inspired by [cbonsai](https://gitlab.com/jallbrit/cbonsai) by John Allbritten

## License

GPL-3.0