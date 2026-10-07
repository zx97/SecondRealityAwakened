#!/usr/bin/env bash
#
# Build a single-file Flatpak bundle for "Second Reality: Reawakened".
#
# The demo is compiled *inside* the Freedesktop SDK that matches the runtime,
# so the executable links against the runtime's own glibc/SDL2/GLES and runs on
# any Flatpak host. The package contains only what it needs to launch: the
# binary, its desktop entry + icon, and the credits/license files.
#
# This uses the native `flatpak build-*` commands, so `flatpak-builder` is not
# required (handy on immutable distros such as Bazzite/Silverblue).
#
set -euo pipefail

APP_ID="io.github.zx97.SecondRealityAwakened"
ARCH="x86_64"
BRANCH="${FREEDESKTOP_BRANCH:-25.08}"
RUNTIME="org.freedesktop.Platform"
SDK="org.freedesktop.Sdk"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$ROOT/_Builds/flatpak"
# Keep the work tree OUTSIDE the source directory. That directory is
# bind-mounted into the build sandbox, and a build dir living inside it would
# expose var/run/host (the host root) back into the sources, sending make's
# recursive wildcard into an infinite loop.
WORK="${XDG_CACHE_HOME:-$HOME/.cache}/secondreality-flatpak"
BUILD="$WORK/build"
REPO="$WORK/repo"
BUNDLE="$OUT/SecondRealityReawakened-${BRANCH}-${ARCH}.flatpak"

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }

# --- 1. Make sure the matching runtime and SDK are installed ----------------
ensure() {
    flatpak info "$1//$BRANCH" >/dev/null 2>&1 && return 0
    log "installing $1//$BRANCH (user)"
    flatpak remote-add --if-not-exists --user flathub \
        https://dl.flathub.org/repo/flathub.flatpakrepo
    flatpak install -y --user flathub "$1//$BRANCH"
}
ensure "$RUNTIME"
ensure "$SDK"

# --- 2. Fresh build tree ----------------------------------------------------
log "preparing build tree"
rm -rf "$BUILD" "$REPO"
mkdir -p "$WORK" "$OUT"
flatpak build-init --arch="$ARCH" "$BUILD" "$APP_ID" "$SDK" "$RUNTIME" "$BRANCH"

# Keep the committed embedded-asset sources newer than their bitmaps so make
# never tries to rewrite the bind-mounted source tree.
find "$ROOT/Parts" -name '*Data.cpp' -exec touch {} +

# --- 3. Compile inside the SDK and assemble /app ----------------------------
log "compiling inside $SDK//$BRANCH (a few minutes)..."
flatpak build \
    --bind-mount=/src="$ROOT" \
    --build-dir=/src \
    --env=APP_ID="$APP_ID" \
    "$BUILD" \
    bash -c '
        set -e
        # Build straight into /app/bin; object files stay in /tmp and are
        # discarded, so they never reach the package.
        make -j"$(nproc)" all \
            CXX=g++ CC=gcc \
            SRC_DIR=/src \
            BUILD_DIR=/tmp/obj \
            OUTPUT_DIR=/app/bin \
            OUT=secondreality \
            EMBED_PY=python3

        # Launcher entry, icon and credits/license.
        install -Dm644 "/src/flatpak/${APP_ID}.desktop" \
            "/app/share/applications/${APP_ID}.desktop"
        install -Dm644 "/src/flatpak/${APP_ID}.png" \
            "/app/share/icons/hicolor/256x256/apps/${APP_ID}.png"
        install -Dm644 /src/flatpak/CREDITS.md \
            /app/share/doc/secondreality/CREDITS.md
        install -Dm644 /src/LICENSE.md \
            "/app/share/licenses/${APP_ID}/LICENSE.md"
        # xBRZ is GPL-3.0 and is linked into the binary, so ship its license.
        install -Dm644 /src/ThirdParty/xbrz/License.txt \
            "/app/share/licenses/${APP_ID}/xbrz-GPL-3.0.txt"
    '

# --- 4. Runtime permissions: GPU, display, audio ----------------------------
log "finalizing $APP_ID"
flatpak build-finish "$BUILD" \
    --command=secondreality \
    --share=ipc \
    --socket=wayland --socket=fallback-x11 --socket=pulseaudio \
    --device=dri \
    --filesystem=xdg-run/pipewire-0

# --- 5. Export and bundle ---------------------------------------------------
log "exporting bundle"
flatpak build-export --arch="$ARCH" "$REPO" "$BUILD" "$BRANCH"
flatpak build-bundle --arch="$ARCH" "$REPO" "$BUNDLE" "$APP_ID" "$BRANCH"

# Drop the intermediate tree unless explicitly kept for inspection.
[ "${KEEP_WORK:-0}" = 1 ] || rm -rf "$WORK"

log "done: $BUNDLE"
echo "Install with:  flatpak install --user \"$BUNDLE\""
echo "Run with:      flatpak run $APP_ID"
