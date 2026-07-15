#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
out="$root/assets/ckitty-demo.gif"
if [ -n "${CKITTY_DEMO_FONT:-}" ]; then
    font=$CKITTY_DEMO_FONT
elif [ "$(uname -s)" = "Darwin" ] && [ -f /System/Library/Fonts/SFNSMono.ttf ]; then
    font=/System/Library/Fonts/SFNSMono.ttf
else
    font=DejaVu-Sans-Mono
fi

if ! command -v magick >/dev/null 2>&1; then
    echo "ckitty: make-demo.sh needs ImageMagick (magick)" >&2
    exit 1
fi
if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ckitty: make-demo.sh needs FFmpeg (ffmpeg) to assemble the GIF" >&2
    exit 1
fi
if [ ! -f "$root/ckitty" ]; then
    make -C "$root" all
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM
mkdir -p "$root/assets"

make_frame() {
    number=$1
    pose=$2
    frame=$3
    art="$tmp/art-$number.txt"
    png="$tmp/frame-$number.png"

    "$root/ckitty" --dump --seed 123 --pose "$pose" --frame "$frame" > "$art"
    magick -size 1200x650 xc:'#080c11' \
        -fill '#0d131b' -draw 'roundrectangle 28,28 1172,622 18,18' \
        -fill '#182331' -draw 'roundrectangle 28,28 1172,92 18,18' \
        -fill '#182331' -draw 'rectangle 28,74 1172,92' \
        -fill '#ff5f57' -draw 'circle 60,60 68,60' \
        -fill '#febc2e' -draw 'circle 86,60 94,60' \
        -fill '#28c840' -draw 'circle 112,60 120,60' \
        -font "$font" -pointsize 17 -fill '#8d9bad' \
        -gravity north -annotate +0+50 'ckitty — terminal kitty' \
        -font "$font" -pointsize 18 -fill '#79d4c6' \
        -gravity northwest -annotate +72+120 "~ \\$ ckitty $pose" \
        -font "$font" -pointsize 24 -fill '#f5c96b' \
        -gravity center -annotate +0+30 "@$art" \
        -font "$font" -pointsize 16 -fill '#617286' \
        -gravity southwest -annotate +72+30 "seed 123  ·  frame $frame" \
        -font "$font" -pointsize 16 -fill '#79d4c6' \
        -gravity southeast -annotate +72+30 'q quit  ·  space pose  ·  n new kitty' \
        "$png"
}

make_frame 01 sit 0
make_frame 02 sit 16
make_frame 03 sleep 0
make_frame 04 play 0
make_frame 05 play 16
make_frame 06 walk 8

# Assemble with FFmpeg's palette pipeline so every frame remains a complete
# terminal window instead of becoming a viewer-dependent transparent delta.
ffmpeg -loglevel error -y -framerate 2 -i "$tmp/frame-%02d.png" \
    -vf 'fps=2,split[s0][s1];[s0]palettegen=max_colors=256:reserve_transparent=0[p];[s1][p]paletteuse=dither=sierra2_4a' \
    -loop 0 "$out"
echo "wrote $out"
