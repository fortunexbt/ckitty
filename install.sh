#!/bin/sh
set -eu

prefix=${PREFIX:-/usr/local}
run_tests=1

usage() {
    cat <<EOF
Usage: $0 [--prefix DIR] [--skip-tests]

Build and install the canonical ckitty binary. The script never invokes sudo;
use sudo explicitly when the selected prefix requires it.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix)
            [ "$#" -ge 2 ] || { echo "missing value for --prefix" >&2; exit 2; }
            prefix=$2
            shift 2
            ;;
        --prefix=*)
            prefix=$(printf '%s\n' "$1" | sed 's/^--prefix=//')
            shift
            ;;
        --skip-tests)
            run_tests=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

# The same script works from a checkout and from a one-line curl install.
# Keep the download path deliberately boring: git is preferred, with a GitHub
# archive fallback for small machines that do not have git installed.
if [ ! -f Makefile ] || [ ! -d src ]; then
    repo_url=${CKITTY_REPO_URL:-https://github.com/fortunexbt/ckitty.git}
    repo_ref=${CKITTY_REF:-main}
    tmp_dir=$(mktemp -d 2>/dev/null || mktemp -d -t ckitty)
    source_dir=""
    cleanup() {
        rm -rf "$tmp_dir"
    }
    trap cleanup EXIT INT TERM

    if command -v git >/dev/null 2>&1; then
        git clone --quiet --depth 1 --branch "$repo_ref" "$repo_url" "$tmp_dir/ckitty"
        source_dir="$tmp_dir/ckitty"
    elif command -v curl >/dev/null 2>&1 && command -v tar >/dev/null 2>&1; then
        archive="$tmp_dir/ckitty.tar.gz"
        curl -fsSL "$repo_url/archive/refs/heads/$repo_ref.tar.gz" -o "$archive"
        mkdir "$tmp_dir/source"
        tar -xzf "$archive" -C "$tmp_dir/source" --strip-components=1
        source_dir="$tmp_dir/source"
    else
        echo "ckitty: install needs git, or curl and tar" >&2
        exit 1
    fi

    if [ "$run_tests" -eq 1 ]; then
        (cd "$source_dir" && ./install.sh --prefix "$prefix")
    else
        (cd "$source_dir" && ./install.sh --prefix "$prefix" --skip-tests)
    fi
    exit 0
fi

if [ "$run_tests" -eq 1 ]; then
    make check
else
    make
fi
make install PREFIX="$prefix"
echo "ckitty installed to $prefix/bin/ckitty"
