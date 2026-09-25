#!/usr/bin/env bash
# Turn a folder of rendered PNG frames into a web-ready sequence for the
# review page. Frames are resized and converted to WebP (alpha preserved),
# numbered frame-0000.webp, frame-0001.webp … and described by meta.json.
#
#   review/add-sequence.sh <png-folder> <id> [options]
#
#   --title "Name"    Title shown on the page         (default: id)
#   --width 1400      Output width in px               (default: 1400)
#   --step 1          Keep every Nth frame             (default: 1)
#   --quality 82      WebP quality 0–100               (default: 82)
#   --bg "#ffffff"    Starting background, black or white (default: #ffffff)
#   --scroll 400      Scroll length, in % of screen    (default: 400)
#   --mobile 720      Width of the lighter phone set   (default: 720)
#   --cut "Label:66"  Add a tab playing from frame 66 to the end, or a
#                     range with "Label:66-200". Repeatable. Frame numbers
#                     are output frames (after --step). With any --cut, a
#                     "Full animation" tab is added first.
#
# Phones get the smaller set in m/ — full-size frames for a long sequence
# use more memory than iPhone Safari allows and the tab reloads.
#
# Share it as:  https://lundfilms.fi/review/?s=<id>
set -euo pipefail

if [ $# -lt 2 ]; then sed -n '2,/^# Share/p' "$0" | sed 's/^# \{0,1\}//'; exit 1; fi

SRC="$1"; ID="$2"; shift 2
TITLE="$ID"; WIDTH=1400; STEP=1; QUALITY=82; BG="#ffffff"; SCROLL=400; MOBILE=720; CUTS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --title) TITLE="$2"; shift 2 ;;
    --width) WIDTH="$2"; shift 2 ;;
    --step) STEP="$2"; shift 2 ;;
    --quality) QUALITY="$2"; shift 2 ;;
    --bg) BG="$2"; shift 2 ;;
    --scroll) SCROLL="$2"; shift 2 ;;
    --mobile) MOBILE="$2"; shift 2 ;;
    --cut)
      label="${2%:*}"; range="${2##*:}"; start="${range%-*}"
      case "$range" in *-*) end=", \"end\": ${range#*-}" ;; *) end="" ;; esac
      CUTS="$CUTS, { \"label\": \"$label\", \"start\": $start$end }"
      shift 2 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

command -v cwebp >/dev/null || { echo "cwebp not found — brew install webp" >&2; exit 1; }
[ -d "$SRC" ] || { echo "No such folder: $SRC" >&2; exit 1; }
case "$ID" in *[!a-z0-9-]*|"") echo "id must be lowercase letters, digits and dashes" >&2; exit 1 ;; esac

OUT="$(cd "$(dirname "$0")" && pwd)/seq/$ID"
rm -rf "$OUT"; mkdir -p "$OUT/m"

# Sorted list of PNGs, thinned by --step
LIST="$(mktemp)"
find "$SRC" -maxdepth 1 -type f -iname '*.png' | sort | awk -v s="$STEP" '(NR-1)%s==0' > "$LIST"
COUNT=$(wc -l < "$LIST" | tr -d ' ')
[ "$COUNT" -gt 0 ] || { echo "No PNG files in $SRC" >&2; exit 1; }
echo "Converting $COUNT frames → $OUT"

# Convert in parallel, 8 at a time: full size + phone size
i=0
while IFS= read -r src; do
  n=$(printf %04d $i)
  cwebp -quiet -q "$QUALITY" -alpha_q 90 -resize "$WIDTH" 0 "$src" -o "$OUT/frame-$n.webp" &
  cwebp -quiet -q "$QUALITY" -alpha_q 90 -resize "$MOBILE" 0 "$src" -o "$OUT/m/frame-$n.webp" &
  i=$((i + 1))
  if [ $((i % 8)) -eq 0 ]; then wait; printf "\r  %d / %d" "$i" "$COUNT"; fi
done < "$LIST"
wait; printf "\r  %d / %d\n" "$COUNT" "$COUNT"
rm -f "$LIST"

# Read back actual output dimensions
DIMS=$(sips -g pixelWidth -g pixelHeight "$OUT/frame-0000.webp" 2>/dev/null | awk '/pixel/ {print $2}' | tr '\n' ' ')
W=$(echo $DIMS | cut -d' ' -f1); H=$(echo $DIMS | cut -d' ' -f2)
MDIMS=$(sips -g pixelWidth -g pixelHeight "$OUT/m/frame-0000.webp" 2>/dev/null | awk '/pixel/ {print $2}' | tr '\n' ' ')
MW=$(echo $MDIMS | cut -d' ' -f1); MH=$(echo $MDIMS | cut -d' ' -f2)

cat > "$OUT/meta.json" <<JSON
{
  "title": "$TITLE",
  "frames": $COUNT,
  "width": $W,
  "height": $H,
  "mobile": { "width": $MW, "height": $MH },
  "ext": "webp",
  "bg": "$BG",
  "scroll": $SCROLL,
$([ -n "$CUTS" ] && echo "  \"cuts\": [ { \"label\": \"Full animation\", \"start\": 0 }$CUTS ],")
  "updated": "$(date +%Y-%m-%d)"
}
JSON

SIZE=$(du -sh "$OUT" | cut -f1)
echo "Done: $COUNT frames, ${W}×${H}, $SIZE total"
echo "Preview locally:  http://localhost:8123/review/?s=$ID"
echo "Client link:      https://lundfilms.fi/review/?s=$ID"
