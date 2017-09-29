/*
 * String handling classes for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "strutil.h"

INLINE void String::Set(const char *s)
{
    if (s == 0) s = "";
    len = strlen(s);
    (void)Resize(len+1, 0);
    STRNLCPY(text, s, len, sz);
}

INLINE void String::Set(const char *s, int length)
{
    len = length;
    (void)Resize(len+1, 0);
    STRNLCPY(text, s, len, sz);
}

INLINE void String::Set(int val)
{
    char v[16];
    snprintf(v, sizeof(v), "%d", val);
    Set(v);
}

#if __MSDOS__
INLINE void String::Set(long val)
{
    char v[16];
    snprintf(v, sizeof(v), "%ld", val);
    Set(v);
}
#endif

INLINE void String::Append(const char *string, int length)
{
    len += length;
    (void)Resize(len+1, 1);
    STRNLCAT(text, string, length, sz);
}

INLINE void String::Append(const char *string)
{
    Append(string, strlen(string));
}

INLINE void String::Append(char ch)
{
    (void)Resize(len+2, 1);
    text[len] = ch;
    text[++len] = 0;
}

INLINE void String::Append(int val)
{
    char v[16];
    snprintf(v, sizeof(v), "%d", val);
    Append(v, strlen(v));
}

INLINE String &String::operator=(const char *s_in)
{
    Set(s_in, strlen((char *)s_in));
    return *this;
}

INLINE String &String::operator=(const String &s_in)
{
    Set(s_in.text, s_in.len);
    return *this;
}

INLINE String &String::operator=(const char ch)
{
    char str[2];
    str[0] = ch;
    str[1] = 0;
    Set(str, 1);
    return *this;
}

INLINE String &String::operator=(const int val)
{
    Set(val);
    return *this;
}

#if __MSDOS__

INLINE String &String::operator=(const long val)
{
    Set(val);
    return *this;
}

#endif

INLINE char &String::operator[](unsigned i)
{
    return text[i];
}

INLINE void String::operator+=(const char *text_in)
{
    Append(text_in, strlen(text_in));
}

INLINE void String::operator+=(const String &s_in)
{
    Append(s_in.text, s_in.len);
}

INLINE void String::operator+=(const char ch)
{
    Append(ch);
}

INLINE void String::operator+=(int val)
{
    Append(val);
}

INLINE const char * const String::Text() const
{
    return text;
}

INLINE String::operator const char*()
{
    return text;
}

INLINE String::operator char*()
{
    return text;
}

INLINE void String::Trunc(int newlen)
{
    assert(newlen <= (int)len);
    text[len = newlen] = 0;
}

INLINE const int String::Length() const
{
    return len;
}


