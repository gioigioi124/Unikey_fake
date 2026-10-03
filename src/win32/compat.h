#ifndef __WIN32_COMPAT_H__
#define __WIN32_COMPAT_H__

// Include windows.h first for Win32 types (HHOOK, HWND, etc.)
#ifndef _WIN32
#define _WIN32
#endif
#ifndef WIN32
#define WIN32
#endif

#include <windows.h>
#include <string.h>
#include <cstring>
#include <cstdlib>

// Compatibility for legacy C++ codebase where strchr on const char* was assigned to char*
inline char* compat_strchr(const char* s, int c) {
    return const_cast<char*>(std::strchr(s, c));
}
inline char* compat_strchr(char* s, int c) {
    return std::strchr(s, c);
}
#define strchr compat_strchr

#endif // __WIN32_COMPAT_H__
