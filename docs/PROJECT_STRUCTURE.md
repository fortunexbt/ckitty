# Project Structure

```
ckitty/
├── README.md              # Main documentation
├── LICENSE                # GPL-3.0 license
├── CONTRIBUTING.md        # Contribution guidelines
├── EXAMPLES.md           # Usage examples and gallery
├── Makefile              # Build configuration
├── install.sh            # Installation script
├── .gitignore            # Git ignore rules
├── .github/
│   └── workflows/
│       └── build.yml     # CI/CD pipeline
├── docs/
│   └── PROJECT_STRUCTURE.md  # This file
├── src/                  # Source files
│   ├── ckitty.c         # v1: Basic animated kitty
│   ├── ckitty_v2.c      # v2: Procedural generation
│   └── ckitty_v3.c      # v3: Advanced features
└── examples/            # Example scripts (future)
```

## File Descriptions

### Core Source Files

- **ckitty.c**: Original version with basic animation
- **ckitty_v2.c**: Introduced procedural generation and growth system
- **ckitty_v3.c**: Current version with multiple poses, environments, and advanced animations

### Build System

- **Makefile**: Handles compilation of all versions, installation, and cleanup
- **install.sh**: Automated installation script with dependency checking

### Documentation

- **README.md**: Main project documentation with features, installation, and usage
- **CONTRIBUTING.md**: Guidelines for contributors
- **EXAMPLES.md**: Comprehensive examples and use cases
- **LICENSE**: GPL-3.0 license text

### CI/CD

- **.github/workflows/build.yml**: GitHub Actions workflow for building and testing on multiple platforms

## Version History

### v1.0 - Basic Animation
- Simple kitty with walking animation
- Basic color support
- Command-line options

### v2.0 - Procedural Generation
- Step-by-step kitty construction
- Multiple tail types
- Customizable body features
- Live generation mode

### v3.0 - Advanced Features
- Multiple poses (sitting, sleeping, playing)
- Environmental elements (yarn, mice, birds)
- Advanced animations (blinking, whisker twitching)
- Screensaver mode
- Improved ASCII art with curves

## Future Structure (Planned)

```
ckitty/
├── src/
│   ├── core/           # Core functionality
│   ├── poses/          # Different kitty poses
│   ├── animations/     # Animation logic
│   └── environments/   # Environmental elements
├── assets/            # ASCII art templates
├── tests/             # Unit tests
└── examples/          # Example scripts and demos
```