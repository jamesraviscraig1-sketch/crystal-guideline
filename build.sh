#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
# build.sh — Compile libcrystal_guideline.so via Android NDK
#
# Requirements:
#   - Android NDK (set NDK_PATH or export ANDROID_NDK_HOME)
#   - cmake 3.22+
#
# Usage:
#   export NDK_PATH=/path/to/ndk   (or relies on ANDROID_NDK_HOME)
#   ./build.sh
#
# Output:  dist/libcrystal_guideline.so  (arm64-v8a)
# Then:    ./patch.sh game.apk
# ═══════════════════════════════════════════════════════════════

set -euo pipefail

RED='\033[0;31m'; GRN='\033[0;32m'; BLU='\033[0;34m'; RST='\033[0m'
ok()  { echo -e "${GRN}[✓]${RST} $*"; }
die() { echo -e "${RED}[✗]${RST} $*" >&2; exit 1; }
info(){ echo -e "${BLU}[i]${RST} $*"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/.build"
SO_OUT="$SCRIPT_DIR/dist"

# ─── NDK detection ────────────────────────────────────────────
NDK="${NDK_PATH:-${ANDROID_NDK_HOME:-}}"
if [[ -z "$NDK" ]]; then
    # Common locations
    for candidate in \
        "$HOME/Library/Android/sdk/ndk-bundle" \
        "$HOME/Android/Sdk/ndk-bundle" \
        "$HOME/android-ndk"*
    do
        [[ -d "$candidate" ]] && NDK="$candidate" && break
    done
fi
[[ -z "$NDK" ]] && die "NDK not found. Set NDK_PATH=/path/to/ndk-XX.X.XXXXXXX"
ok "NDK: $NDK"

TOOLCHAIN="$NDK/build/cmake/android.toolchain.cmake"
[[ ! -f "$TOOLCHAIN" ]] && die "Toolchain not found at $TOOLCHAIN"

# ─── cmake configure ─────────────────────────────────────────
info "Configuring cmake (arm64-v8a, minSdk 21)…"
mkdir -p "$BUILD_DIR" "$SO_OUT"

cmake \
    -S "$SCRIPT_DIR/jni" \
    -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DANDROID_STL=none \
    -G Ninja \
    2>&1 | tail -5

ok "Configured"

# ─── build ───────────────────────────────────────────────────
info "Building…"
cmake --build "$BUILD_DIR" --config Release -j$(nproc 2>/dev/null || echo 4)

# ─── copy output ─────────────────────────────────────────────
SO_BUILT=$(find "$BUILD_DIR" -name "libcrystal_guideline.so" | head -1)
[[ -z "$SO_BUILT" ]] && die "Build succeeded but .so not found?"

cp "$SO_BUILT" "$SO_OUT/libcrystal_guideline.so"
SIZE=$(du -sh "$SO_OUT/libcrystal_guideline.so" | cut -f1)
ok "libcrystal_guideline.so → $SO_OUT/  ($SIZE)"
echo ""
echo -e "${GRN}Build done!${RST}  Now run:  ${BLU}./patch.sh game.apk${RST}"
