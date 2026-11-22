// PokeL-ARecomp: what the FidelityFX SDK (written for MSVC) uses from the Microsoft C runtime and its headers, for
// building it with GCC / Clang on Linux (the Makefile includes this before every FSR 3 source there; MinGW on Windows
// has these itself). Release builds only reach these - the _DEBUG-only wcstombs_s and the _WIN32 branches are not.
#pragma once
#ifndef _WIN32
#include <codecvt>
#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <locale>
#include <string>

// (ffx_message.cpp uses it outside Windows without including ffx_util.h: the same definition, which C++ allows twice)
#define FFX_UNUSED(x)               ((void)(x))

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


inline int sprintf_s(char *dst, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, n, fmt, ap);
    va_end(ap);
    return r < 0 ? -1 : (size_t)r >= n ? (int)n - 1 : r;  // (the length written: the breadcrumbs advance by it)
}
template <size_t N> inline int sprintf_s(char (&dst)[N], const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, N, fmt, ap);
    va_end(ap);
    return r < 0 ? -1 : (size_t)r >= N ? (int)N - 1 : r;
}
#endif
