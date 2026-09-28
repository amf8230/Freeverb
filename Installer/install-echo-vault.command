#!/bin/bash
# Echo Vault installer: copies the plugins into your user plug-in folders
# and clears the macOS "downloaded from the internet" flag so Studio One can load them.
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

echo "Installing Echo Vault..."
rm -rf "$VST3_DIR/Echo Vault.vst3" "$AU_DIR/Echo Vault.component"
cp -R "Echo Vault.vst3" "$VST3_DIR/" && echo "  VST3 -> $VST3_DIR"
cp -R "Echo Vault.component" "$AU_DIR/" && echo "  AU   -> $AU_DIR"

xattr -dr com.apple.quarantine "$VST3_DIR/Echo Vault.vst3" 2>/dev/null
xattr -dr com.apple.quarantine "$AU_DIR/Echo Vault.component" 2>/dev/null

# Refresh the Audio Unit cache
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Done! Restart Studio One and look for 'Echo Vault' under Effects > Vault Audio."
read -r -p "Press Return to close this window."
