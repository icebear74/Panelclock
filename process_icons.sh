#!/usr/bin/env bash
#
# process_icons.sh
#
# Erwartet: ffmpeg, xxd, awk
# Usage:
#   ./process_icons.sh /path/to/Symbole
#
# Für jede Unterordner (Iconset) sucht es Iconset/png/*.png
# Legt png/48, png/24, png/16, png/12 an und schreibt:
#   png/<size>/<name>.png   (skaliert, rgba)
#   png/<size>/<name>.txt   (RGB888 hex Werte, kommasepariert, BREITE Werte/Zeile -> Blockform)
#
set -euo pipefail

# Config: Auflösungen
SIZES=(48)

# Tools check
for cmd in ffmpeg xxd awk; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "ERROR: required tool '$cmd' not found in PATH" >&2
    exit 2
  fi
done

if [ $# -lt 1 ]; then
  echo "Usage: $0 /path/to/Symbole" >&2
  exit 1
fi

BASE_DIR="$1"
if [ ! -d "$BASE_DIR" ]; then
  echo "ERROR: base folder '$BASE_DIR' does not exist" >&2
  exit 1
fi

echo "Processing base folder: $BASE_DIR"
echo "Target sizes: ${SIZES[*]}"
echo

# Iterate direct subfolders only
find "$BASE_DIR" -mindepth 1 -maxdepth 1 -type d -print0 | while IFS= read -r -d '' ICONSET_DIR; do
  PNG_DIR="$ICONSET_DIR/png"
  if [ ! -d "$PNG_DIR" ]; then
    echo "Skipping '$ICONSET_DIR' (no png/ folder)"
    continue
  fi

  echo "Iconset: $ICONSET_DIR"
  # List PNG files (non-recursive)
  shopt -s nullglob
  png_files=("$PNG_DIR"/*.png "$PNG_DIR"/*.PNG)
  shopt -u nullglob
  if [ ${#png_files[@]} -eq 0 ]; then
    echo "  No PNG files in $PNG_DIR, skipping."
    continue
  fi

  for size in "${SIZES[@]}"; do
    OUTDIR="$PNG_DIR/$size"
    mkdir -p "$OUTDIR"
  done

  for src in "${png_files[@]}"; do
    fname="$(basename "$src")"
    name_noext="${fname%.*}"
    echo "  Processing '$fname'..."

    for size in "${SIZES[@]}"; do
      out_png="$PNG_DIR/$size/$name_noext.png"
      out_txt="$PNG_DIR/$size/$name_noext.txt"

      # 1) Scale to size x size, preserve alpha. Overwrite if exists (-y).
      ffmpeg -y -v error -i "$src" -vf "scale=${size}:${size}:flags=lanczos" -pix_fmt rgba "$out_png"
      if [ $? -ne 0 ]; then
        echo "    ERROR: ffmpeg failed scaling $src -> $out_png" >&2
        continue
      fi

      # 2) Produce txt with RGBA raw bytes -> convert to 0xRRGGBB or 0x000000 if alpha==0
      #    Output format: comma separated, WIDTH values per line (block look), no trailing comma after last value.
      total_pixels=$((size * size))
#      values_per_line=$size        # ergibt ein "Block" pro Zeile (z.B. 48 Werte pro Zeile bei 48x48)
      values_per_line=8

      ffmpeg -v error -i "$out_png" -f rawvideo -pix_fmt rgba - | \
        xxd -p -c 4 | \
        awk -v total="$total_pixels" -v perline="$values_per_line" '
        BEGIN { cnt = 0; }
        {
          s = toupper($0);            # RRGGBBAA
          r = substr(s,1,2);
          g = substr(s,3,2);
          b = substr(s,5,2);
          a = substr(s,7,2);
          if (a == "00") {
            out = "0x000000";
          } else {
            out = "0x" r g b;
          }
          cnt++;
          # print value and decide comma/newline
          if (cnt < total) {
            # not last overall value -> print value + comma
            printf "%s", out;
            if (cnt % perline == 0) {
              # end of visual row -> comma then newline
              printf ",\n";
            } else {
              printf ", ";
            }
          } else {
            # last value -> print and newline, no trailing comma
            printf "%s\n", out;
          }
        }
        ' > "$out_txt"

      if [ $? -ne 0 ]; then
        echo "    ERROR: failed generating txt for $out_png" >&2
        continue
      fi

      echo "    -> $size: $out_png , $out_txt"
    done

  done

  echo "  Done: $ICONSET_DIR"
  echo
done

echo "All done."