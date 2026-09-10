# GTA: San Andreas Linux port — asset preparation

This port does **not** include Rockstar game data. The user supplies the Android
package files, and the Linux launcher loads them from the `gtasa/` directory.

The tested native payload is the **arm64-v8a v2.11.264** `libGame.so` from the
supplied GSG Android package. v2.11.311 is a different native build and is not
interchangeable without version-specific validation.

## What the Linux port loads

The loaded Android library still uses the Android game's file model. The Linux
compatibility layer supplies an `AAssetManager`-like interface and redirects it
to ordinary files below the current game directory:

```text
Android asset path                 Linux lookup
assets/data/foo                    data/foo
assets/texdb/gta3/gta3.etc.dat    texdb/gta3/gta3.etc.dat
assets/audio/sfx/genrl.osw        audio/sfx/genrl.osw
```

The shim first tries the path as given, then retries after removing an
`assets/` prefix. It indexes `data/` and `es2/` for the Android asset-existence
checks; the `181` files reported by the startup log are only the `data/` tree,
not the complete game installation. Other paths are checked directly on disk.

The native file layer also opens regular files and the large packed game files
used by San Andreas, including `.ras` and `.msf` files. Data is loaded on demand,
so a successful title-screen boot does not prove that every map, mission,
vehicle, language, texture, or radio file is present.

The v2.11.264 package contains these relevant kinds of data:

- `data/`: game configuration, maps, collision/path data, scripts, decisions,
  and other text/binary tables. Examples include `data/gta.dat`,
  `data/script/main.scm`, `data/script/script.img`, and `data/maps/`.
- `models/`: model and effect resources.
- `texdb/`: texture databases and their companion `.dat`, `.tmb`, `.toc`,
  `.unc`, `.txt`, and, where supplied, `.sz` files. Examples include the
  `gta3`, `gta_int`, `menu`, `player`, `playerhi`, and `txd` trees.
- `audio/`: audio configuration, sound banks/streams, `.osw` sound files and
  their `.idx` indexes, and streamed radio/cutscene audio.
- `text/`: language `.gxt` files such as `american.gxt`, `french.gxt`, and
  `japanese.gxt`.
- `textures/`: loose texture/font resources used by the Android build.
- `anim/`: animation resources.
- `es2/`, if present in the package: GLES2 shader/renderer resources.
- Package-specific loose files such as `config.txt`, `stream.ini`, and shader
  cache files. `config.txt` is a **game data file**, not this port's wrapper
  configuration.

The exact package also contains Android/application material such as
`flutter_assets`, `dexopt`, `rockstar`, `socialclub`, `xml`, and `json`. The
safe preparation rule is to copy the complete `assets/` tree, preserving its
relative paths and case, instead of trying to guess which of those files will
be needed by a later code path.

For the tested v2.11.264 package, `assets/` contains **479 files** totalling
**2,628,343,969 bytes**. The useful top-level counts are `data/` 181,
`texdb/` 100, `audio/` 59, `textures/` 64, `text/` 14, `models/` 22,
`anim/` 3, and `rockstar/` 14, alongside the smaller package/application
trees. These numbers are a sanity check for that package, not a universal
manifest for every GSG release.

The port also vendors `assetfile.txt` as a **120-entry required-file
manifest**. Its first line is `120` and every later line is an
`assets/`-relative path. The entries that exist in this extracted package use
its exact directory and filename casing. The manifest intentionally selects
120 files rather than enumerating the complete 479-file asset tree. It is
copied to the game root, beside `config.txt`; it is not a replacement for any
of the listed data files.

## Required non-asset files

The clean Linux installation needs these files under `gtasa/`:

```text
gtasa/
├── gtasa_linux
├── libGame.so
├── libc++_shared.so             # vendored Android NDK runtime
├── assetfile.txt                # vendored 120-entry package manifest
├── libSDL3.so.0                 # bundled Linux SDL3 build
├── data/                         # extracted Android assets
├── models/
├── texdb/
├── audio/
├── text/
├── textures/
└── ...                           # the rest of assets/ preserved verbatim
```

### `libGame.so`

Use the **arm64-v8a** library from the same Android package as the data. Do not
substitute a different game build or an ARM32 library. The tested v2.11.264
library has SHA-256:

```text
3e6c3b843909cc4ff9a3721487c7f95cf01390fb42ae11db0e02adb9c1a54e24
```

This hash is a provenance check for the tested package, not a replacement for
obtaining the game lawfully.

### `libc++_shared.so`

The current loader uses this as the Android C++ runtime donor for the C++ and
`__cxa` imports that `libGame.so` expects. This is a redistributable Android NDK
runtime vendored by the Linux port; it is **not** another proprietary game
asset that users need to obtain from a second game distribution. Keep the
vendored file beside `libGame.so` under the exact name `libc++_shared.so`.

An arbitrary host C++ library is not an equivalent replacement. A different
Android game version may require a different compatible NDK runtime.

### Linux libraries, not Android libraries

`libSDL3.so.0` is built for Linux/AArch64 and is not taken from the Android
package. The Linux executable also uses the target's Linux EGL/GLES, OpenAL
Soft, and mpg123 libraries. The Android package's `libopenal.so` and
`libVendor_mpg123.so` are not the normal Linux playback/decoder dependencies;
do not treat copying those Android libraries as part of a clean Linux install.

The Android `libGame.so` advertises Android dependencies such as
`libVendor_mpg123.so`, `libandroid.so`, `liblog.so`, `libEGL.so`,
`libGLESv2.so`, and `libopenal.so`. The Linux import table translates those
interfaces to the Linux SDL/EGL/GLES/OpenAL/mpg123 implementations. They are
ABI inputs to the loader, not a request to ship the Android copies beside the
Linux executable.

The Linux audio path is:

```text
OpenAL Soft loopback mixer → SDL3 audio stream → target PipeWire/SDL backend
```

## Preparing a clean directory from the Android package

The following extracts the complete APK asset tree without flattening it. Run
it outside the source repository; proprietary files must not be added to Git.
Use an OBB only when the Android package actually supplies one for the same
version.

```sh
set -eu
set -o pipefail

APK=/path/to/the/matching/GSG-Android.apk
PORTS=/userdata/roms/ports
GAME="$PORTS/gtasa"
STAGE="${TMPDIR:-/tmp}/gtasa-assets-prepare"

rm -rf "$STAGE"
mkdir -p "$STAGE/assets" "$GAME"

# Preserve every assets/<path> member, including case and subdirectories.
unzip -q "$APK" 'assets/*' -d "$STAGE"
cp -a "$STAGE/assets/." "$GAME/"
install -m 0644 /path/to/gtasa_linux/assetfile.txt "$GAME/assetfile.txt"

# Extract only the matching ARM64 game library from the APK.
unzip -p "$APK" 'lib/arm64-v8a/libGame.so' > "$GAME/libGame.so"

# Copy the port's vendored Android NDK runtime. In this repository it is the
# tracked top-level libc++_shared.so, not a file extracted from the game APK.
install -m 0644 /path/to/gtasa_linux/libc++_shared.so \
    "$GAME/libc++_shared.so"

chmod 0755 "$GAME/gtasa_linux"
```

If the Android distribution uses a matching `main.*.obb`, extract the OBB's
contents into the same `gtasa/` directory while preserving paths. Do not merge
an OBB from another version into this APK. The tested v2.11.264 upload already
contains its large game data in the APK's `assets/` tree, so an additional OBB
is not automatically required for that package.

The launcher itself remains one directory above this tree:

```text
ports/
├── Grand Theft Auto San Andreas.sh
└── gtasa/
    ├── gtasa_linux
    ├── libGame.so
    ├── libc++_shared.so
    └── extracted assets...
```

## Verify before launching

Check that the library is the expected architecture and that the APK library
was not silently replaced by a different game build:

```sh
file "$GAME/libGame.so" "$GAME/libc++_shared.so"
sha256sum "$GAME/libGame.so"
test -f "$GAME/config.txt"
test "$(head -n 1 "$GAME/assetfile.txt")" = 120
test -f "$GAME/data/gta.dat"
test -f "$GAME/data/script/main.scm"
test -f "$GAME/texdb/gta3/gta3.img"
test -f "$GAME/text/american.gxt"
```

Do not use only a file count as a completeness check. Texture databases are
large and are split across companion files; audio files have indexes; and many
resources are opened only after starting a new game, entering a map, changing
language, or playing radio/cutscene audio.

A clean extraction still needs a full play test before it can be called
complete. Startup/import resolution, rendering, gamepad input, audio, map
loading, saves, radio, and exit are separate verification gates.
