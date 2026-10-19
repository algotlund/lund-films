#!/usr/bin/env bash
# Crop a transparent PNG render to its visible product, add even padding,
# and save a web-sized WebP (alpha kept).
#   prep-render.sh <in.png> <out.webp> [max-width=1600] [pad%=10]
set -euo pipefail
IN="$1"; OUT="$2"; MAXW="${3:-1600}"; PAD="${4:-10}"
W=$(sips -g pixelWidth "$IN" | awk '/pixelWidth/ {print $2}')
H=$(sips -g pixelHeight "$IN" | awk '/pixelHeight/ {print $2}')
# Bounding box of pixels with alpha > 8
BB=$(ffmpeg -hide_banner -i "$IN" -vf "alphaextract,bbox=min_val=8" -f null - 2>&1 | grep -o 'x1:[0-9]* x2:[0-9]* y1:[0-9]* y2:[0-9]*' | tail -1)
x1=$(echo "$BB" | sed 's/.*x1:\([0-9]*\).*/\1/'); x2=$(echo "$BB" | sed 's/.*x2:\([0-9]*\).*/\1/')
y1=$(echo "$BB" | sed 's/.*y1:\([0-9]*\).*/\1/'); y2=$(echo "$BB" | sed 's/.*y2:\([0-9]*\).*/\1/')
bw=$((x2 - x1 + 1)); bh=$((y2 - y1 + 1))
m=$(( (bw > bh ? bw : bh) * PAD / 100 ))
cx=$((x1 - m)); cy=$((y1 - m)); cw=$((bw + 2*m)); ch=$((bh + 2*m))
[ $cx -lt 0 ] && cx=0; [ $cy -lt 0 ] && cy=0
[ $((cx + cw)) -gt $W ] && cw=$((W - cx)); [ $((cy + ch)) -gt $H ] && ch=$((H - cy))
RW=$(( cw < MAXW ? cw : MAXW ))
cwebp -quiet -q 86 -alpha_q 95 -crop $cx $cy $cw $ch -resize $RW 0 "$IN" -o "$OUT"
echo "$(basename "$OUT"): ${cw}x${ch} → ${RW}w"
