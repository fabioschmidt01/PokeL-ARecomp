// PokeL-ARecomp: what the FidelityFX SDK (written for MSVC) uses from the Microsoft C runtime and its headers, for
// building it with GCC / Clang on Linux (the Makefile includes this before every FSR 3 source there; MinGW on Windows
// has these itself). Release builds only reach these - the _DEBUG-only wcstombs_s and the _WIN32 branches are not.
#pragma once
#ifndef _WIN32
#include <codecvt>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <locale>
#include <string>

#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif

inline int wcscpy_s(wchar_t *dst, size_t n, const wchar_t *src) {
    if (!dst || !n) return 22;  // (EINVAL)
    wcsncpy(dst, src ? src : L"", n);
    dst[n - 1] = 0;
    return 0;
}
template <size_t N> inline int wcscpy_s(wchar_t (&dst)[N], const wchar_t *src) { return wcscpy_s(dst, N, src); }

inline int strcpy_s(char *dst, size_t n, const char *src) {
    if (!dst || !n) return 22;
    strncpy(dst, src ? src : "", n);
    dst[n - 1] = 0;
    return 0;
}
template <size_t N> inline int strcpy_s(char (&dst)[N], const char *src) { return strcpy_s(dst, N, src); }
#endif
