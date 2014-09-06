#pragma once

#include <sql.h>
#include <string>
#include <cstddef>

// If it isn't, things will be very broken when passing from ODBC to V8
static_assert(sizeof(SQLWCHAR) == 2, "SQLWCHAR should be two bytes");

inline std::wstring sqlstring(const SQLWCHAR* str) {
    std::wstring wstr;
    for (; *str; str++)
        wstr.push_back(*str);
    return wstr;
}

inline std::size_t sqlwcslen(const SQLWCHAR* str) {
    std::size_t len = 0;
    while (*str++)
        len++;
    return len;
}

template<typename U>
inline int sqlwcscmp(const SQLWCHAR* x, const U* y) {
    for (; *x || *y; x++, y++)
        if (*x != *y)
            return (int)*x - (int)*y;
    return 0;
}

inline void sqlwcsncpy(SQLWCHAR* dst, const SQLWCHAR* src, std::size_t count) {
    while (count --> 0) {
        if (src) {
            *dst++ = *src++;
        } else {
            *dst++ = 0;
            src = nullptr;
        }
    }
}
