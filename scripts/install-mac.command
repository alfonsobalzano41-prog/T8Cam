#!/bin/bash
# Installa T8Cam (AU, VST3 e app) e toglie il blocco "file scaricato da internet".
# Doppio clic su questo file dal Finder.
set -e
cd "$(dirname "$0")"

COMP="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$COMP" "$VST3"

rm -rf "$COMP/T8Cam.component" "$VST3/T8Cam.vst3"
cp -R T8Cam.component "$COMP/"
cp -R T8Cam.vst3 "$VST3/"
xattr -dr com.apple.quarantine "$COMP/T8Cam.component" "$VST3/T8Cam.vst3" 2>/dev/null || true

if [ -w /Applications ]; then APPS=/Applications; else APPS="$HOME/Applications"; mkdir -p "$APPS"; fi
rm -rf "$APPS/T8Cam.app"
cp -R T8Cam.app "$APPS/"
xattr -dr com.apple.quarantine "$APPS/T8Cam.app" 2>/dev/null || true

# Fa rileggere gli Audio Unit a macOS (Logic li ri-scansiona al prossimo avvio).
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "T8Cam installato:"
echo "  AU   -> $COMP/T8Cam.component"
echo "  VST3 -> $VST3/T8Cam.vst3"
echo "  App  -> $APPS/T8Cam.app"
echo
echo "Riavvia il DAW. Puoi chiudere questa finestra."
