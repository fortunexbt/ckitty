# Contributing to ckitty

Thank you for your interest in contributing to ckitty! This document provides guidelines and instructions for contributing.

## Code of Conduct

- Be respectful and inclusive
- Welcome newcomers and help them get started
- Focus on constructive criticism

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in [Issues](https://github.com/yourusername/ckitty/issues)
2. If not, create a new issue with:
   - Clear title and description
   - Steps to reproduce
   - Expected vs actual behavior
   - System information (OS, terminal, ncurses version)

### Suggesting Features

1. Check existing issues for similar suggestions
2. Create a new issue with the `enhancement` label
3. Describe the feature and its use case
4. Include ASCII art mockups if relevant!

### Code Contributions

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-kitty`)
3. Make your changes
4. Run tests and ensure compilation works
5. Commit with clear messages
6. Push to your fork
7. Create a Pull Request

### Coding Standards

- Follow the existing C style in the codebase
- Use meaningful variable names
- Comment complex algorithms
- Keep functions focused and small
- Test on multiple terminals if possible

### ASCII Art Guidelines

When creating new kitty poses or animations:
- Keep within 80 column width
- Test with and without colors
- Ensure it looks good in different terminal sizes
- Make it recognizable as a cat!

## Development Setup

```bash
# Clone the repo
git clone https://github.com/yourusername/ckitty.git
cd ckitty

# Install dependencies (macOS)
brew install ncurses

# Install dependencies (Ubuntu/Debian)
sudo apt-get install libncurses5-dev

# Build
make

# Test
./ckitty_v3 -c
```

## Testing

Before submitting:
1. Test all command line options
2. Test in different terminal emulators
3. Test with/without color support
4. Verify no memory leaks with valgrind (if available)

## Pull Request Process

1. Update README.md if adding new features
2. Add comments for new algorithms
3. Ensure all tests pass
4. Update version numbers if applicable
5. PR will be merged after review

## Adding New Features

Popular requests:
- New kitty poses (stretching, grooming, hunting)
- More environmental elements
- Different cat breeds
- Sound effects (purring via terminal bell?)
- Network multiplayer kitties

## Questions?

Feel free to open an issue for any questions about contributing!