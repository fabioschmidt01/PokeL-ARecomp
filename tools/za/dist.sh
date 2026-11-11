#!/bin/bash
# ZA Recomp release for Windows: build/dist-za/ZA Recomp/ (ZIP=1: + ZA-Recomp-windows.zip). No game code or game files:
# the start screen reads the player's own .xci / .nsp with their prod.keys (zaimport/) and recompiles the game on their
# machine (lasgen/ = tools/gen.py frozen by PyInstaller, zig/ = the compiler, gamelib-src/ = the headers it includes).
# Run from Git Bash with MSYS2 (UCRT64) installed, Python 3.12 (py launcher) and the .NET 8 SDK. Downloads once: zig
# (ziglang.org, hash checked), the generator's Python packages (PyPI), LibHac (NuGet).
# FFmpeg: the LGPL build from tools/build_deps_windows.sh (build/deps-win, made here once; MSYS2's is GPL).
set -euo pipefail
cd "$(dirname "$0")/../.."
unset INCLUDE LIB LIBPATH
UCRT_DIR=""
for candidate in \
  "/d/a/_temp/setup-msys2/msys64/ucrt64/bin" \
  "/d/a/_temp/setup-msys2/ucrt64/bin" \
  "/c/msys64/ucrt64/bin" \
  "/c/hostedtoolcache/windows/msys64/ucrt64/bin"; do
  if [ -x "$candidate/clang.exe" ]; then
    UCRT_DIR="$candidate"
    break
  fi
done
if [ -z "$UCRT_DIR" ]; then
  UCRT_CLANG=$(find /d/a/_temp /c -maxdepth 5 -name "clang.exe" 2>/dev/null | grep -i "ucrt64/bin/clang.exe" | head -n 1 || true)
  [ -n "$UCRT_CLANG" ] && UCRT_DIR=$(dirname "$UCRT_CLANG")
fi
if [ -n "$UCRT_DIR" ]; then
  CLEAN_PATH=$(echo "$PATH" | tr ':' '\n' | grep -v -i "Program Files/LLVM" | grep -v -i "Windows Kits" | tr '\n' ':')
  export PATH="$UCRT_DIR:$(dirname "$UCRT_DIR")/usr/bin:$CLEAN_PATH"
  echo "Using UCRT64 toolchain at: $UCRT_DIR"
  echo "Clang binary: $(which clang)"
fi
CURL_BIN="curl"
command -v /c/Windows/System32/curl.exe >/dev/null 2>&1 && CURL_BIN="/c/Windows/System32/curl.exe"
TAR_BIN="tar"
command -v /c/Windows/System32/tar.exe >/dev/null 2>&1 && TAR_BIN="/c/Windows/System32/tar.exe"
GENOPT="-O1 -DGUEST_VOLATILE" GENARCH=-march=x86-64-v3  # (the game code's flags: part of the library id)
B=build/za-dist D="build/dist-za/PokeL-ARecomp"

# 1. the compiler (Windows' curl: MSYS2's one rejects TLS-inspecting proxies' certificates)
ZIP_ZIG=build/zig-dl/zig-x86_64-windows-0.17.0.zip
if [ ! -x build/zig/zig.exe ]; then
    mkdir -p build/zig-dl
    [ -f $ZIP_ZIG ] || $CURL_BIN -fsSL --retry 5 --retry-all-errors --retry-delay 2 --retry-connrefused --http1.1 -o $ZIP_ZIG https://ziglang.org/download/0.17.0/zig-x86_64-windows-0.17.0.zip
    bash tools/fetch_zig.sh
fi
# 1b. Intel XeSS SDK (Intel Arc AI upscaling runtime)
if [ ! -f third_party/xess/bin/libxess.dll ]; then
    mkdir -p build/xess-dl
    ZIP_XESS=build/xess-dl/XeSS_SDK_3.0.2.zip
    [ -f "$ZIP_XESS" ] || $CURL_BIN -fsSL --retry 5 --retry-all-errors --retry-delay 2 --retry-connrefused --http1.1 -o "$ZIP_XESS" https://github.com/intel/xess/releases/download/v3.0.2/XeSS_SDK_3.0.2.zip
    $TAR_BIN -xf "$ZIP_XESS" -C third_party/xess
fi

# 2. the generator helper (its own venv: only what gen.py needs, not the randomizer's packages)
PY_CMD="python"
if command -v py >/dev/null 2>&1 && py -3.12 --version >/dev/null 2>&1; then
    PY_CMD="py -3.12"
fi
if [ ! -f build/gen-venv/.ok ]; then
    $PY_CMD -m venv build/gen-venv
    build/gen-venv/Scripts/python.exe -m pip install -q capstone==5.0.9 unicorn==2.1.4 lz4==4.4.5 pyinstaller==6.22.3
    touch build/gen-venv/.ok
fi
USERPROFILE="$(cygpath -w ~)" build/gen-venv/Scripts/python.exe -m PyInstaller --noconfirm --clean --log-level WARN \
    --distpath build/helper --workpath build/pyinstaller tools/lasgen.spec
build/gen-venv/Scripts/python.exe tools/pe_no_cfg.py build/helper/lasgen/lasgen.exe
# 2b. the button icons + opt-in battle layout preparer (zaprompts + ooz; all resources come from the player's game)
mkdir -p build/ooz
(cd third_party/ooz && g++ -O2 -shared -static -include sys/stat.h -Dmain=ooz_main kraken.cpp bitknit.cpp lzna.cpp \
    ooz_lib.cpp -o ../../build/ooz/ooz.dll -Wl,-u,__mingw_fe_dfl_env)  # (-u: newer MinGW libmingwex needs CRT_fp10 from libmingw32)
if [ ! -f build/prompts-venv/.ok2 ]; then  # (.ok2: texture2ddecoder, the team icons' BC7)
    $PY_CMD -m venv build/prompts-venv
    build/prompts-venv/Scripts/python.exe -m pip install -q pillow==13.0.0 numpy==2.5.4 fonttools==4.67.0 cffsubr==0.4.0 texture2ddecoder==1.0.6 \
        pyinstaller==6.22.3
    touch build/prompts-venv/.ok2
fi
USERPROFILE="$(cygpath -w ~)" build/prompts-venv/Scripts/python.exe -m PyInstaller --noconfirm --clean --log-level WARN \
    --distpath build/helper --workpath build/pyinstaller-prompts tools/zaprompts.spec
build/helper/zaprompts/zaprompts.exe --xbox-attack-layout-version  # refuse a ZIP with an older/incomplete helper
# 3. the LGPL FFmpeg, the exe (release: no console, log in lasrecomp.log) and zig with its runtime built ahead of time
SRC_FF=build/deps-win/src/ffmpeg-9.0.2.tar.xz
if [ ! -f "$SRC_FF" ]; then
    mkdir -p build/deps-win/src
    for u in "https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz" "https://www.ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz"; do
        if curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 --retry-connrefused --http1.1 -o "$SRC_FF.part" "$u"; then
            mv "$SRC_FF.part" "$SRC_FF"
            break
        elif command -v /c/Windows/System32/curl.exe >/dev/null 2>&1 && /c/Windows/System32/curl.exe -fsSL --retry 5 --retry-all-errors --retry-delay 2 --retry-connrefused --http1.1 -o "$SRC_FF.part" "$u" 2>/dev/null; then
            mv "$SRC_FF.part" "$SRC_FF"
            break
        fi
    done
fi
bash tools/build_deps_windows.sh
make -j"$(nproc)" B=$B DEPS="$PWD/build/deps-win" RELEASE=1 GENARCH="$GENARCH" GENOPT="$GENOPT" $B/PokeL-ARecomp.exe
! nm $B/PokeL-ARecomp.exe | grep -q ' f_70'  # (no game code inside)
FLAGS=$(make -s --eval='pv: ; @echo $(strip $(filter-out -Iruntime,$(GENFLAGS)))' B=$B GENARCH="$GENARCH" GENOPT="$GENOPT" pv)
bash tools/zig_slim.sh $FLAGS
# 4. the game file importer (self-contained: no .NET needed on the player's machine)
dotnet publish tools/zaimport -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true \
    -p:EnableCompressionInSingleFile=true -p:DebugType=none -o build/zaimport > /dev/null

# 5. the folder
rm -rf build/dist-za && mkdir -p "$D/gamelib-src" "$D/zaimport"
cp $B/PokeL-ARecomp.exe $B/PokeL-ARecomp.dll.a "$D/"
cp -R build/helper/lasgen "$D/"
cp -R build/helper/zaprompts "$D/"
cp third_party/ooz/COPYING "$D/zaprompts/COPYING-GPLv3.txt"
cp third_party/ooz/README.ZARecomp.md "$D/zaprompts/README-ooz.md"
cp -R build/zig-slim "$D/zig"
cp build/zaimport/zaimport.exe "$D/zaimport/"
cp runtime/cpu.h runtime/simd.h runtime/simd_fp.h runtime/gamelib.h tools/gamelib_entry.c runtime/hle/hle_symbols.txt \
    "$D/gamelib-src/"
cp docs/icon/icon_1024.png "$D/icon.png"
cp third_party/fonts/Baloo2/Baloo2-SemiBold.ttf "$D/"
cp third_party/fonts/Baloo2/OFL.txt "$D/Baloo2-OFL.txt"
cp README.md LICENSE "$D/"
mkdir -p "$D/docs"
cp docs/XBOX-ATTACK-LAYOUT.md "$D/docs/"
[ -f README.pt-BR.md ] && cp README.pt-BR.md "$D/"
[ -f third_party/xess/bin/libxess.dll ] && cp third_party/xess/bin/libxess.dll "$D/"
[ -f third_party/xess/bin/libxell.dll ] && cp third_party/xess/bin/libxell.dll "$D/"
cp tools/za/za-default.ini "$D/za-default.ini"  # (not larecomp.ini: the first run copies it, runtime/paths.c - a new zip unzipped over an install keeps the player's own)
# (every DLL but Windows' own and the Vulkan loader: also when copies sit next to the exe in $B, a dev setup)
PATH="$PWD/build/deps-win/bin:$PATH" ldd $B/PokeL-ARecomp.exe | awk 'tolower($3) !~ /^\/c\/windows\// && !/vulkan-1/ {print $3}' | sort -u |
    while read -r f; do cp "$f" "$D/"; done
! ls "$D" | grep -iE 'x264|x265|xvid|postproc'  # (no GPL codec libraries in the release)
du -sh "$D"
if [ "${ZIP:-0}" = 1 ]; then
    (cd build/dist-za && $TAR_BIN -a -cf PokeL-ARecomp-windows-x64.zip "PokeL-ARecomp")
    ls -la build/dist-za/PokeL-ARecomp-windows-x64.zip
fi
