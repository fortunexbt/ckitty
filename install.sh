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

if [ "$run_tests" -eq 1 ]; then
    make check
else
    make
fi
make install PREFIX="$prefix"
echo "ckitty installed to $prefix/bin/ckitty"
