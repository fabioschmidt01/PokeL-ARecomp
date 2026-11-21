#!/bin/bash
# make deps-windows (MSYS2 UCRT64 shell): what the shipped Windows build links against instead of MSYS2's packages,
# built from pinned sources into build/deps-win. MSYS2's FFmpeg is a GPLv3 build (x264, x265, xvid, ...); the
# release uses an LGPL-only FFmpeg with just what the game's movies need - WebM: VP9 video + Vorbis audio, plus
# VP8 / Opus / H.264 / AAC in WebM or MP4 (the same configuration as tools/build_deps_macos.sh) - and swscale.
# SDL3 (zlib licence) and the compiler runtime DLLs still come from MSYS2.
# Needs: the packages from tools/setup_windows.ps1 (clang, make, nasm). Skipped when its output exists
# (delete build/deps-win to redo).
# `tools/build_deps_windows.sh aarch64` (tools/za/dist-arm64.sh): the same FFmpeg for Windows ARM64 into
# build/deps-win-arm64, cross-compiled with llvm-mingw (github.com/mstorsjo/llvm-mingw: aarch64-w64-mingw32-clang and its
# tools on PATH, or LLVM_MINGW=<its folder>) and with FFmpeg's NEON assembly (clang assembles it, no nasm): the tablets
# and Windows on ARM machines it is for decode the movies' VP9 on the CPU.
set -euo pipefail
cd "$(dirname "$0")/.."
P=$PWD/build/deps-win
S=$P/src
FF=ffmpeg
ARCHCFG=(--cc=gcc --cxx=g++ --arch=x86_64 --disable-asm --disable-x86asm --disable-inline-asm)
if [ "${1:-}" = aarch64 ]; then
    P=$PWD/build/deps-win-arm64 FF=ffmpeg-arm64  # (the source tarball stays in build/deps-win/src)
    [ -n "${LLVM_MINGW:-}" ] && export PATH="$LLVM_MINGW/bin:$PATH"
    command -v aarch64-w64-mingw32-clang >/dev/null ||
        { echo "deps-win: aarch64 needs llvm-mingw (aarch64-w64-mingw32-clang on PATH, or LLVM_MINGW=<its folder>)"; exit 1; }
    ARCHCFG=(--enable-cross-compile --cross-prefix=aarch64-w64-mingw32- --cc=aarch64-w64-mingw32-clang
             --cxx=aarch64-w64-mingw32-clang++ --arch=aarch64)
fi
mkdir -p "$S"
JOBS=$(nproc)

fetch() {  # url sha256 file
    if [ ! -f "$S/$3" ]; then
        for u in "$1" "https://www.ffmpeg.org/releases/$3"; do
            if curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 --retry-connrefused --http1.1 -o "$S/$3.part" "$u"; then
                mv "$S/$3.part" "$S/$3"
                break
            elif command -v /c/Windows/System32/curl.exe >/dev/null 2>&1 && /c/Windows/System32/curl.exe -fsSL --retry 5 --retry-all-errors --retry-delay 2 --retry-connrefused --http1.1 -o "$S/$3.part" "$u" 2>/dev/null; then
                mv "$S/$3.part" "$S/$3"
                break
            fi
        done
    fi
    echo "$2  $S/$3" | sha256sum -c --quiet
}
unpack() { rm -rf "$S/$2" && mkdir -p "$S/$2" && tar -xf "$S/$1" -C "$S/$2" --strip-components 1; }

fetch https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz \
      8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e ffmpeg-9.0.2.tar.xz

if [ ! -f "$P/lib/libavcodec.dll.a" ]; then
    unpack ffmpeg-9.0.2.tar.xz $FF
    (cd "$S/$FF" && ./configure --prefix="$P" "${ARCHCFG[@]}" --target-os=mingw32 \
        --enable-shared --disable-static \
        --disable-autodetect --disable-programs --disable-doc --disable-network --disable-avdevice --disable-avfilter \
        --disable-swresample --disable-everything \
        --enable-protocol=file --enable-demuxer=matroska,mov \
        --enable-decoder=vp9,vp8,vorbis,opus,h264,aac --enable-parser=vp9,vp8,vorbis,opus,h264,aac \
        && make -j"$JOBS" && make install)
fi
grep -q 'GPL version 3\|--enable-gpl' "$P/lib/pkgconfig/libavcodec.pc" && { echo "deps-win: FFmpeg is not LGPL"; exit 1; }
echo "deps-win: $(ls "$P/bin"/*.dll | xargs -n1 basename | tr '\n' ' ')"
