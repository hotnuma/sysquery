#include "str_ext.h"

bool cstr_ellipsize(CString *cstr, const char *str, int length)
{
    int inlen = strlen(str);
    if (inlen == length)
    {
        cstr_copy(cstr, str);
        return true;
    }

    int partlen = 1;
    int len = length - partlen;

    if (inlen < 1 || len < 1)
        return false;

    cstr_copy_len(cstr, str, MIN(inlen, len));

    if (inlen >= length)
        cstr_append(cstr, "+");

    int diff = length - cstr_size(cstr);

    for (int i = 0; i < diff; ++i)
    {
        cstr_append_c(cstr, ' ');
    }

    return true;
}

bool cstr_long(CString *cstr, long val, int pad)
{
    CStringAuto *temp = cstr_new_size(16);
    cstr_fmt(temp, "%ld", val);

    cstr_clear(cstr);

    int diff = pad - cstr_size(temp);

    for (int i = 0; i < diff; ++i)
    {
        cstr_append_c(cstr, ' ');
    }

    cstr_append(cstr, c_str(temp));

    return true;
}


