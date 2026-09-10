#!/usr/bin/env bash
# GTA: San Andreas — generic Linux/AArch64 launcher
#
# The launcher contains no game data.  Place the user's Android ARM64 files
# beside the executable, or under the detected gtasa/ directory:
#   libGame.so
#   libc++_shared.so
#   data/, models/, texdb/, audio/, ...

set -u

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"

# Accept an explicitly supplied game directory; otherwise the game payload is
# always the gtasa/ directory beside this launcher.  This is the PortMaster
# layout: /userdata/roms/ports/Grand Theft Auto San Andreas.sh plus gtasa/.
if [[ -n "${GTASA_GAME_DIR:-}" ]]; then
    GAME_DIR="$(CDPATH= cd -- "$GTASA_GAME_DIR" 2>/dev/null && pwd -P)" || {
        printf 'GTA SA: invalid GTASA_GAME_DIR: %s\n' "$GTASA_GAME_DIR" >&2
        exit 1
    }
else
    GAME_DIR="$SCRIPT_DIR/gtasa"
fi

BINARY="${GTASA_BINARY:-$GAME_DIR/gtasa_linux}"
LOG_FILE="${GTASA_LOG:-$GAME_DIR/gtasa.log}"

# SDL3 chooses Wayland/X11/KMSDRM according to the current Linux session.
# Users can override any of these without editing the launcher.
export SDL_VIDEO_ALLOW_SCREENSAVER=0
export SDL_NO_SIGNAL_HANDLERS=1
if [[ -z "${SDL_KMSDRM_DEVICE:-}" && -e /dev/dri/card0 ]]; then
    export SDL_KMSDRM_DEVICE=/dev/dri/card0
fi

# Knulli runs system PipeWire under /var/run, including for SSH tests. Do not
# override a desktop/user session or an explicitly selected SDL audio backend.
if [[ -z "${XDG_RUNTIME_DIR:-}" && -S /var/run/pipewire-0 ]]; then
    export XDG_RUNTIME_DIR=/var/run
fi
if [[ -z "${SDL_AUDIO_DRIVER:-}" && -S "${XDG_RUNTIME_DIR:-/nonexistent}/pipewire-0" ]]; then
    export SDL_AUDIO_DRIVER=pipewire
fi

# Prefer libraries shipped with the port, while retaining system paths for
# SDL3, EGL/GLES, OpenAL Soft, and mpg123.
if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
    export LD_LIBRARY_PATH="$GAME_DIR:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="$GAME_DIR"
fi

# Optional PortMaster controls integration.  The core launcher does not require
# PortMaster and remains usable from a normal desktop/session.
CONTROL_DIR=""
for candidate in \
    /opt/system/Tools/PortMaster \
    /opt/tools/PortMaster \
    "$XDG_DATA_HOME/PortMaster" \
    "$SCRIPT_DIR/PortMaster" \
    "$(dirname -- "$GAME_DIR")/PortMaster"; do
    if [[ -f "$candidate/control.txt" ]]; then
        CONTROL_DIR="$candidate"
        break
    fi
done

if [[ -n "$CONTROL_DIR" ]]; then
    # shellcheck disable=SC1090
    controlfolder="$CONTROL_DIR"
    set +u
    source "$CONTROL_DIR/control.txt"
    set -u
    if declare -F get_controls >/dev/null 2>&1; then
        get_controls
    fi
    export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
fi

if [[ ! -x "$BINARY" ]]; then
    printf 'GTA SA: executable not found or not executable:\n  %s\n' "$BINARY" >&2
    printf 'Set GTASA_BINARY or build/install gtasa_linux there.\n' >&2
    exit 1
fi

# Knulli/Batocera runs ports from a frontend process rather than a controlling
# VT. KMSDRM needs the active VT on stdin to become the DRM master and present
# to the panel; the original handheld launcher explicitly used tty1 here.
if [[ -c /dev/tty1 && -r /dev/tty1 ]]; then
    exec < /dev/tty1
fi

for required in libGame.so; do
    if [[ ! -f "$GAME_DIR/$required" ]]; then
        printf 'GTA SA: missing required Android library:\n  %s\n' "$GAME_DIR/$required" >&2
        printf 'Extract it from the arm64-v8a APK into the game directory.\n' >&2
        exit 1
    fi
done

# The native game payload is AArch64.  Permit an override for QEMU/binfmt or
# other environments where uname does not report the payload architecture.
HOST_ARCH="$(uname -m 2>/dev/null || true)"
if [[ "$HOST_ARCH" != "aarch64" && "$HOST_ARCH" != "arm64" && "${GTASA_ALLOW_FOREIGN_ARCH:-0}" != 1 ]]; then
    printf 'GTA SA: this build targets AArch64; host reports %s.\n' "${HOST_ARCH:-unknown}" >&2
    printf 'Use an AArch64 Linux target or set GTASA_ALLOW_FOREIGN_ARCH=1 for an emulator/binfmt setup.\n' >&2
    exit 1
fi

mkdir -p "$(dirname -- "$LOG_FILE")" 2>/dev/null || true
cd "$GAME_DIR" || exit 1

# Keep stdout/stderr available when launched from a terminal, while retaining
# a log for frontend launches.  GTASA_NO_LOG=1 disables redirection.
if [[ "${GTASA_NO_LOG:-0}" == 1 ]]; then
    exec "$BINARY" "$@"
else
    # Start each run with a fresh log; do not append indefinitely across runs.
    exec >"$LOG_FILE" 2>&1
    printf '[%s] launcher: starting %s\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')" "$BINARY"
    printf '[launcher] game_dir=%s\n' "$GAME_DIR"
    exec "$BINARY" "$@"
fi
