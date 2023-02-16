#ifndef STR_EXT_H
#define STR_EXT_H

#include <cstring.h>

bool cstr_ellipsize(CString *cstr, const char *str, int length);
bool cstr_long(CString *cstr, long val, int pad);

#endif // STR_EXT_H


