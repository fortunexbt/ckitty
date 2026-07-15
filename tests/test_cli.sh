#!/bin/sh
set -eu

binary=$1

help_output=$("$binary" --help)
printf '%s\n' "$help_output" | grep -F 'Usage: ckitty [OPTIONS]' >/dev/null
printf '%s\n' "$help_output" | grep -F -- '--dump' >/dev/null
[ "$("$binary" --version)" = 'ckitty 1.0.0' ]

first=$("$binary" --dump --seed 123 --pose sit --frame 0)
second=$("$binary" --dump --seed 123 --pose sit --frame 0)
[ "$first" = "$second" ]
printf '%s\n' "$first" | grep -F '(_)' >/dev/null

for pose in sit sleep play walk; do
    output=$("$binary" --dump --seed 123 --pose "$pose" --frame 0)
    [ -n "$output" ]
done

frame_zero=$("$binary" --dump --seed 999 --pose sit --frame 0)
frame_later=$("$binary" --dump --seed 999 --pose sit --frame 40)
[ "$frame_zero" != "$frame_later" ]

expect_failure() {
    if "$binary" "$@" >/dev/null 2>&1; then
        echo "expected failure: $*" >&2
        exit 1
    fi
}

expect_failure --dump --seed -1
expect_failure --dump --pose nope
expect_failure --dump --width 0
expect_failure --dump --height 0
expect_failure --dump --frame nope
expect_failure --dump --width 4000 --height 4000
expect_failure --dump unexpected
expect_failure --not-an-option

echo 'CLI OK'
