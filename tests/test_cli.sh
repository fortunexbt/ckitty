#!/bin/sh
set -eu

binary=$1

help_output=$("$binary" --help)
if printf '%s\n' "$help_output" | grep -F -- '--infinite' >/dev/null; then
    echo "compatibility flags should stay out of the friendly help" >&2
    exit 1
fi
printf '%s\n' "$help_output" | grep -F 'Usage: ckitty [POSE] [OPTIONS]' >/dev/null
printf '%s\n' "$help_output" | grep -F -- '--dump' >/dev/null
printf '%s\n' "$help_output" | grep -F -- '--theme' >/dev/null
printf '%s\n' "$help_output" | grep -F -- '--quiet' >/dev/null
[ "$("$binary" --version)" = 'ckitty 1.1.0' ]

first=$("$binary" --dump --seed 123 --pose sit --frame 0)
second=$("$binary" --dump --seed 123 --pose sit --frame 0)
[ "$first" = "$second" ]
for infinite in -i --infinite; do
    compatibility=$("$binary" "$infinite" --dump --seed 123 --pose sit --frame 0)
    [ "$compatibility" = "$first" ]
done
printf '%s\n' "$first" | grep -F '/\___/\' >/dev/null
printf '%s\n' "$first" | grep -F '(__)(__)' >/dev/null

for pose in sit sleep play walk; do
    output=$("$binary" --dump --seed 123 --pose "$pose" --frame 0)
    [ -n "$output" ]
    printf '%s\n' "$output" | grep -F '/\___/\' >/dev/null
    positional=$("$binary" --dump --seed 123 "$pose" --frame 0)
    [ "$output" = "$positional" ]
done

frame_zero=$("$binary" --dump --seed 999 --pose sit --frame 0)
frame_later=$("$binary" --dump --seed 999 --pose sit --frame 40)
[ "$frame_zero" != "$frame_later" ]

# Presentation options must never change the deterministic, plain-text export,
# including its whitespace and final newline (which shell variables discard).
scratch=$(mktemp -d "${TMPDIR:-/tmp}/ckitty-cli.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
for pose in sit sleep play walk; do
    for frame in 0 40; do
        "$binary" --dump --seed 123 "$pose" --frame "$frame" >"$scratch/plain"
        for theme in amber moon forest; do
            "$binary" --dump --seed 123 "$pose" --frame "$frame" \
                --theme "$theme" --quiet --rainbow >"$scratch/styled"
            cmp "$scratch/plain" "$scratch/styled"
        done
    done
done

expect_failure() {
    if "$binary" "$@" >/dev/null 2>&1; then
        echo "expected failure: $*" >&2
        exit 1
    fi
}

expect_usage_error() {
    code=0
    "$binary" "$@" >"$scratch/stdout" 2>"$scratch/stderr" || code=$?
    if [ "$code" -ne 2 ]; then
        echo "expected usage exit 2, got $code: $*" >&2
        exit 1
    fi
    [ ! -s "$scratch/stdout" ]
    [ -s "$scratch/stderr" ]
}

expect_usage_error --dump --theme neon
expect_usage_error --theme neon
expect_usage_error --dump --theme ''
expect_usage_error --dump --theme

expect_failure --dump --seed -1
expect_failure --dump --pose nope
expect_failure --dump --width 0
expect_failure --dump --height 0
expect_failure --dump --frame nope
expect_failure --dump --width 4000 --height 4000
expect_failure --dump unexpected
expect_failure --dump sit walk
expect_failure --not-an-option

echo 'CLI OK'
