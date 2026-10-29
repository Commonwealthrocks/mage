#!/bin/bash
## deploy_deps.sh
## last updated: 29/10/2026
EXE_PATH="$1"
OUT_DIR="$2"
QT_BIN_DIR="$3"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "[ MAGE ] Linux detected, packing portable AppImage..."
    APPDIR="${OUT_DIR}/AppDir"
    mkdir -p "$APPDIR/usr/bin"
    mkdir -p "$APPDIR/usr/share/applications"
    mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
    cp "$EXE_PATH" "$APPDIR/usr/bin/"
    cp -r "$(dirname "$0")/assets" "$APPDIR/usr/bin/assets"
    cat > "$APPDIR/usr/share/applications/mage.desktop" << 'EOF'
[Desktop Entry]
Name=MAGE
Exec=mage
Icon=mage
Type=Application
Categories=Utility;Security;
EOF
    cp "$(dirname "$0")/assets/imgs/s_icons/ink/mage_256.svg" "$APPDIR/usr/share/icons/hicolor/256x256/apps/mage.svg"
    if [ ! -f "${OUT_DIR}/linuxdeployqt" ]; then
        echo "[ MAGE ] Downloading linuxdeployqt..."
        wget -q -O "${OUT_DIR}/linuxdeployqt" "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
        chmod a+x "${OUT_DIR}/linuxdeployqt"
    fi
    echo "[ MAGE ] Running linuxdeployqt..."
    export VERSION="0.4"
    QMAKE_PATH=$(which qmake6)
    "${OUT_DIR}/linuxdeployqt" "$APPDIR/usr/share/applications/mage.desktop" -appimage -unsupported-allow-new-glibc -qmake="$QMAKE_PATH" -extra-plugins=iconengines,imageformats || true
else
    WINDEPLOYQT=""
    if [ -n "$QT_BIN_DIR" ] && [ -f "$QT_BIN_DIR/windeployqt.exe" ]; then
        WINDEPLOYQT="$QT_BIN_DIR/windeployqt.exe"
    elif command -v windeployqt >/dev/null 2>&1; then
        WINDEPLOYQT="windeployqt"
    elif [ -f "/c/Tools/MYSYS2/ucrt64/bin/windeployqt.exe" ]; then
        WINDEPLOYQT="/c/Tools/MYSYS2/ucrt64/bin/windeployqt.exe"
    fi
    if [ -n "$WINDEPLOYQT" ]; then
        "$WINDEPLOYQT" --verbose 0 --no-translations --no-system-d3d-compiler "$EXE_PATH" 2>&1 | grep -v "catalogs.json" | grep -v "Translations will not be available" || true
    fi
    ldd "$EXE_PATH" | grep -iE '/ucrt64/bin/|/mingw64/bin/|/usr/bin/' | awk '{print $3}' | while read -r dll_path; do
        if [ -f "$dll_path" ]; then
            filename=$(basename "$dll_path")
            if [ ! -f "$OUT_DIR/$filename" ]; then
                cp "$dll_path" "$OUT_DIR/"
            fi
        fi
    done
    echo "[ MAGE ] Qt6 runtime and dependencies deployed."
fi

## end
