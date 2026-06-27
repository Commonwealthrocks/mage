##!/bin/bash
## deploy_deps.sh
## last updated: 27/06/2026
EXE_PATH="$1"
OUT_DIR="$2"
ldd "$EXE_PATH" | grep -iE '/ucrt64/bin/|/mingw64/bin/|/usr/bin/' | awk '{print $3}' | while read -r dll_path; do
    if [ -f "$dll_path" ]; then
        filename=$(basename "$dll_path")
        if [ ! -f "$OUT_DIR/$filename" ]; then
            cp "$dll_path" "$OUT_DIR/"
        fi
    fi
done

## end