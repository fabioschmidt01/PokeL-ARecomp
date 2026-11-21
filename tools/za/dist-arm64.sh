#!/bin/bash
# PokeL-ARecomp for Windows ARM64: build/dist-arm64/PokeL-ARecomp/ (ZIP=1 or `dist-arm64.sh zip`: + PokeL-ARecomp-windows-arm64.zip).
# For Android tablets and phones through GameHub's ARM64 Wine (proton-arm64x: the game code runs natively, only the
# x64 helpers go through FEX) and for Windows on ARM PCs. As the x64 release (tools/za/dist.sh): no game code or game
# files, the start screen imports the player's .xci / .nsp and recompiles the game on their machine - here with zig for
# ARM64 and the generator's --simd asm (runtime/gamebuild.c).
#
# What differs from the x64 build:
# - GUEST_SHIFT -0x6000000000 (runtime/rt.h): the guest window at 0x10..0x20 GB instead of 0x70..0x80 GB. Android's ARM64
#   kernels give a process a 39-bit address space (512 GB) whose top is already taken by the time Wine starts the game.
# - The GPU path for drivers without VK_EXT_external_memory_host (Turnip / Adreno): the guest heap shadow
#   (runtime/nvn/vulkan/exec.c gsh_*) and the transform feedback mirror without the import.
# - The helpers (lasgen, zaprompts, zaimport, PokeL-Extractor) are the x64 ones: Windows on ARM and GameHub run them
#   emulated. lasgen is built here (gen.py: --guest-shift); the other three are taken from the x64 release folder
#   (X64_DIST, default build/dist-za/PokeL-ARecomp from tools/za/dist.sh, or an unpacked official x64 zip of the same version).
#
# Setup (Git Bash on an x64 Windows PC with MSYS2, as for dist.sh), once:
# - MSYS2's clangarm64 packages, installed into the x64 MSYS2 (their post-install scripts fail there, harmless):
#     pacman -S mingw-w64-clang-aarch64-{crt-git,headers-git,winpthreads,compiler-rt,libc++,libunwind,sdl3,shaderc,vulkan-headers,vulkan-loader}
#   (WINSYSROOT=/c/msys64/clangarm64), and its libclang_rt.builtins-aarch64.a copied into the UCRT64 clang's resource
#   folder (ucrt64/lib/clang/<version>/lib/windows/): MSYS2's x64 clang links the ARM64 exe against that sysroot.
# - llvm-mingw (github.com/mstorsjo/llvm-mingw, the ucrt-x86_64 build): LLVM_MINGW=<its folder>, for windres and the
#   LGPL FFmpeg (tools/build_deps_windows.sh aarch64).
# - Python 3.12 (py launcher) for the generator helper. Downloads once: zig for x64 and ARM64 (ziglang.org, hash checked),
#   FFmpeg's source, PyInstaller and the generator's packages (PyPI).
# Environment: MSYS2's programs started from Git Bash's ones don't inherit exported variables (TMP: windres and gcc
# then fail on temp files), so every tool runs through MSYS2's own env.exe, which sets them.
set -euo pipefail
cd "$(dirname "$0")/../.."
unset INCLUDE LIB LIBPATH
: "${LLVM_MINGW:?set LLVM_MINGW to the llvm-mingw folder (see the header)}"
WINSYSROOT=${WINSYSROOT:-/c/msys64/clangarm64}
X64_DIST=${X64_DIST:-build/dist-za/PokeL-ARecomp}
for f in zaimport/zaimport.exe zaprompts/zaprompts.exe; do
    [ -f "$X64_DIST/$f" ] || { echo "dist-arm64: no $X64_DIST/$f (tools/za/dist.sh first, or X64_DIST=<an x64 release folder>)"; exit 1; }
done
MSYS=/c/msys64
TMPW=$(cygpath -m "${TEMP:-$HOME/AppData/Local/Temp}")
E=("$MSYS/usr/bin/env" "PATH=$MSYS/ucrt64/bin:$MSYS/usr/bin:$LLVM_MINGW/bin:$PATH" "TMP=$TMPW" "TEMP=$TMPW" "TMPDIR=$(cygpath -u "$TMPW")")
SHIFT=-0x6000000000 GENOPT="-O1 -DGUEST_VOLATILE"  # (the game code's flags: part of the library id; GENARCH armv8.2-a)
B=build/arm64-dist D="build/dist-arm64/PokeL-ARecomp"
MK=(make -j"$(nproc)" B=$B WINARCH=aarch64 WINSYSROOT=$WINSYSROOT GUEST_SHIFT=$SHIFT DEPS="$PWD/build/deps-win-arm64"
    RELEASE=1 GENOPT="$GENOPT")

# 1. zig: the x64 one runs here and builds the ARM64 one's runtime ahead of time (tools/zig_slim.sh), the ARM64 one ships
[ -x build/zig/zig.exe ] || "${E[@]}" bash tools/fetch_zig.sh
[ -x build/zig-arm64/zig.exe ] || "${E[@]}" ZIG_TARGET=aarch64-windows ZIG_OUT=build/zig-arm64 bash tools/fetch_zig.sh
# 2. the generator helper (x64, emulated there; built from this tree: --guest-shift)
if [ ! -f build/gen-venv/.ok ]; then
    py -3.12 -m venv build/gen-venv
    build/gen-venv/Scripts/python.exe -m pip install -q capstone==5.0.9 unicorn==2.1.4 lz4==4.4.5 pyinstaller==6.22.3
    touch build/gen-venv/.ok
fi
USERPROFILE="$(cygpath -w ~)" build/gen-venv/Scripts/python.exe -m PyInstaller --noconfirm --clean --log-level WARN \
    --distpath build/helper --workpath build/pyinstaller tools/lasgen.spec
build/gen-venv/Scripts/python.exe tools/pe_no_cfg.py build/helper/lasgen/lasgen.exe
build/helper/lasgen/lasgen.exe --help | grep -q -- --guest-shift
# 3. the LGPL FFmpeg for ARM64, the exe, zig with its ARM64 runtime
"${E[@]}" bash tools/build_deps_windows.sh aarch64
"${E[@]}" "${MK[@]}" $B/PokeL-ARecomp.exe
! "$LLVM_MINGW/bin/llvm-nm" $B/PokeL-ARecomp.exe | grep -qE ' f_(10|70)[0-9a-f]{8}'  # (no game code inside)
FLAGS=$("${E[@]}" make -s --eval='pv: ; @echo $(strip $(filter-out -Iruntime,$(GENFLAGS)))' B=$B WINARCH=aarch64 \
    GUEST_SHIFT=$SHIFT GENOPT="$GENOPT" pv)
"${E[@]}" ZIG_SLIM_FROM=build/zig-arm64 ZIG_SLIM_OUT=build/zig-slim-arm64 ZIG_SLIM_TARGET=aarch64-windows-gnu \
    ZIG_HOST="$PWD/build/zig/zig.exe" bash tools/zig_slim.sh $FLAGS

# 4. the folder (as dist.sh's, plus the x64 helpers)
rm -rf build/dist-arm64 && mkdir -p "$D/gamelib-src" "$D/zaimport"
cp $B/PokeL-ARecomp.exe $B/PokeL-ARecomp.dll.a "$D/"
cp -R build/helper/lasgen "$D/"
cp -R "$X64_DIST/zaprompts" "$D/"
cp "$X64_DIST/zaimport/zaimport.exe" "$D/zaimport/"
[ -f "$X64_DIST/PokeL-Extractor.exe" ] && cp "$X64_DIST/PokeL-Extractor.exe" "$D/"
cp -R build/zig-slim-arm64 "$D/zig"
# the PC route (PC-Setup.bat): the game files and game code made on an x64 Windows PC, natively - on an Android tablet
# the setup's generator runs emulated (hours). The x64 zig.exe runs there with the same lib\ and rt\ (zig_slim.sh).
cp build/zig/zig.exe "$D/zig/zig-x64.exe"
GID=$("${E[@]}" make -s --eval='pv: ; @echo $(GAMELIB_ID)' B=$B WINARCH=aarch64 GUEST_SHIFT=$SHIFT GENOPT="$GENOPT" pv)
sed -e "s|@GAMELIB_ID@|$GID|" -e "s|@GAMELIB_CFLAGS@|$FLAGS|" -e "s|@GUEST_SHIFT_DEC@|$(printf '%d' $SHIFT)|" \
    tools/za/pc-setup-arm64.ps1 > "$D/PC-Setup.ps1"
cp tools/za/PC-Setup.bat "$D/"
cp runtime/cpu.h runtime/simd.h runtime/simd_fp.h runtime/gamelib.h tools/gamelib_entry.c runtime/hle/hle_symbols.txt \
    "$D/gamelib-src/"
cp docs/icon/icon_1024.png "$D/icon.png"
cp third_party/fonts/Baloo2/Baloo2-SemiBold.ttf "$D/"
cp third_party/fonts/Baloo2/OFL.txt "$D/Baloo2-OFL.txt"
cp README.md LICENSE "$D/"
[ -f README.pt-BR.md ] && cp README.pt-BR.md "$D/"
[ -d textures/prompts ] && mkdir -p "$D/textures" && cp -R textures/prompts "$D/textures/"
mkdir -p "$D/docs" && cp docs/XBOX-ATTACK-LAYOUT.md docs/android-gamehub.md "$D/docs/"
# (the first run's settings: fullscreen - GameHub's window is the screen -, 30 fps and 1x, what a tablet's GPU keeps up with)
sed -e 's/^fullscreen = .*/fullscreen = 1/' -e 's/^fps60 = .*/fps60 = 0/' -e 's/^resolution = .*/resolution = 1x/' \
    tools/za/za-default.ini > "$D/za-default.ini"
# the DLLs, by import tables: the LGPL FFmpeg first, then MSYS2's clangarm64 (SDL3, shaderc, libc++, ...); Windows' own
# and the Vulkan loader (every driver brings it; Wine has its own) aren't in either folder
declare -A seen; queue=("$B/PokeL-ARecomp.exe")
while [ ${#queue[@]} -gt 0 ]; do
    f=${queue[0]}; queue=("${queue[@]:1}")
    for d in $("$LLVM_MINGW/bin/llvm-objdump" -p "$f" | sed -n 's/^\s*DLL Name: //p' | tr -d '\r'); do
        lc=${d,,}; [ -n "${seen[$lc]:-}" ] && continue; seen[$lc]=1
        [ "$lc" = vulkan-1.dll ] && continue
        for dir in build/deps-win-arm64/bin "$WINSYSROOT/bin"; do
            src=$(ls "$dir" | grep -ix -- "$d" | head -1 || true)
            [ -n "$src" ] && { cp "$dir/$src" "$D/"; queue+=("$dir/$src"); break; }
        done
    done
done
! ls "$D" | grep -iE 'x264|x265|xvid|postproc'  # (no GPL codec libraries in the release)
du -sh "$D"
if [ "${ZIP:-0}" = 1 ] || [ "${1:-}" = zip ]; then
    (cd build/dist-arm64 && /c/Windows/System32/tar.exe -a -cf PokeL-ARecomp-windows-arm64.zip "PokeL-ARecomp")
    ls -la build/dist-arm64/PokeL-ARecomp-windows-arm64.zip
fi
