#!/bin/bash
# ckitty installation script

set -e

echo "🐱 ckitty installer"
echo "=================="

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]]; then
    OS="windows"
else
    OS="unknown"
fi

echo "Detected OS: $OS"

# Check for dependencies
check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        echo "❌ $1 is not installed"
        return 1
    else
        echo "✅ $1 is installed"
        return 0
    fi
}

echo ""
echo "Checking dependencies..."
check_dependency "cc" || check_dependency "gcc" || check_dependency "clang" || {
    echo "Error: No C compiler found. Please install gcc or clang."
    exit 1
}

# Check for ncurses
if [[ "$OS" == "macos" ]]; then
    if ! brew list ncurses &> /dev/null; then
        echo "ncurses not found. Would you like to install it with Homebrew? (y/n)"
        read -r response
        if [[ "$response" == "y" ]]; then
            brew install ncurses
        else
            echo "Please install ncurses manually: brew install ncurses"
            exit 1
        fi
    fi
elif [[ "$OS" == "linux" ]]; then
    if ! ldconfig -p | grep -q libncurses; then
        echo "ncurses not found. Please install it:"
        echo "  Ubuntu/Debian: sudo apt-get install libncurses5-dev"
        echo "  Fedora: sudo dnf install ncurses-devel"
        echo "  Arch: sudo pacman -S ncurses"
        exit 1
    fi
fi

# Build
echo ""
echo "Building ckitty..."
make clean
make all

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
else
    echo "❌ Build failed"
    exit 1
fi

# Install
echo ""
echo "Would you like to install ckitty system-wide? (requires sudo) (y/n)"
read -r response
if [[ "$response" == "y" ]]; then
    sudo make install
    echo "✅ ckitty installed to /usr/local/bin/ckitty"
    echo ""
    echo "You can now run: ckitty -c"
else
    echo "ckitty built successfully in current directory"
    echo "Run: ./ckitty_v3 -c"
fi

echo ""
echo "🐱 Installation complete!"
echo ""
echo "Quick start:"
echo "  ckitty -c           # Colorful kitty"
echo "  ckitty -l -c        # Watch it grow"
echo "  ckitty -S -c        # Screensaver mode"
echo "  ckitty -h           # Show all options"