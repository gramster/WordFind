/*
 * String handling classes for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * $Id: fixstring.cc,v 1.2 2000/03/28 07:53:14 cvg Exp $
 */

#include <sys/types.h>
#include <ctype.h>
#include <syslog.h>

#include "strutil.h"
#include "fixstring.h"

#ifdef NO_INLINES
#include "fixstring.inl"
#endif

//-----------------------------------------------------------------------

String::String()
    : text(0), len(0), sz(0)
{
}

String::String(int size)
    : len(0), sz(size)
{
    text = new char[sz];
    MemAssert(text, text);
    text[0] = 0;
}

String::String(const char *s)
{
    assert(s);
    len = strlen((char*)s);
    sz = len+1;
    text = new char[sz];
    MemAssert(text, text);
    STRLCPY(text, (char*)s, sz);
}

String::String(const String &s)
{
    len = s.Length();
    sz = len+1;
    text = new char[sz];
    MemAssert(text, text);
    STRNLCPY(text, (char*)s.Text(), len, sz);
}

// This is a bit of a cheat as far as OOD goes. Rather than duplicating lots
// of code with minor differences between the static and dynamic strings, we
// treat static strings much like dynamic strings. 

void String::Resize(int newsize, int keep)
{
    (void)keep;
    if (newsize > (int)sz)
    {
        char *tmp = new char[newsize]; 
        assert(tmp);
        if (text)
        {
            if (keep) STRNLCPY(tmp, text, len, newsize);
            delete [] text;
        }
	else tmp[0] = 0;
        text = tmp;
        sz = newsize;
    } 
}

int String::IsNumeric(int *val)
{
    int pos = 0, v = 0, sgn = 1;
    while (pos < (int)len && isspace(text[pos]))
	pos++;
    if (pos < (int)len)
    {
	if (text[pos]=='-')
	{
	    pos++;
	    sgn = -1;
	}
    }
    if (isdigit(text[pos]))
    {
	while (pos < (int)len && isdigit(text[pos]))
	{
	    v = v * 10 + text[pos] - '0';
	    pos++;
	}
	if (pos == (int)len)
	{
	    *val = sgn * v;
	    return 1;
	}
    }
    return 0;
}

int String::IsNumeric(long *val)
{
    int pos = 0;
    long v = 0, sgn = 1l;
    while (pos < (int)len && isspace(text[pos]))
	pos++;
    if (pos < (int)len)
    {
	if (text[pos]=='-')
	{
	    pos++;
	    sgn = -1l;
	}
    }
    if (isdigit(text[pos]))
    {
	while (pos < (int)len && isdigit(text[pos]))
	{
	    v = v * 10l + text[pos] - '0';
	    pos++;
	}
	if (pos == (int)len)
	{
	    *val = sgn * v;
	    return 1;
	}
    }
    return 0;
}

void String::Insert(int pos, char ch)
{
    assert(len < (sz-1));
    memmove(text+pos+1, text+pos, len - pos + 1); // includes NUL
    text[pos] = ch;
    len++;
}

void String::Insert(int pos, char *str)
{
    assert(len < (sz-strlen(str)));
    int slen = strlen(str);
    char *p = text+pos;
    memmove (p+slen, p, len - pos + 1);
    memcpy (p, str, slen);
    len += strlen(str);
}

void String::Insert(int pos, String &str)
{
    assert(len < (sz-str.Length()));
    char *p = text+pos;
    memmove (p+str.len, p, len - pos + 1);
    memcpy (p, str.text, str.len);
    len += str.Length();
}

void String::Remove(int pos, int length)
{
    int oldlen = len;
    memcpy (text+pos, text+pos+length, oldlen-pos-length+1);
    len = strlen(text);
}

char * const String::TrimLeft(char *remove)
{
    String tmp(&text[strspn(text, remove)]);
    *this = tmp;
    return text;
}

char * const String::TrimRight(char *remove)
{
    Trunc(strrspn(text, remove));
    return text;
}

char * const String::Trim(char *remove)
{
    TrimRight(remove);
    TrimLeft(remove);
    return text;
}

char * const String::ToUpper()
{
    return strupr(text);
}

char * const String::ToLower()
{
    return strlwr(text);
}

String::~String()
{
    delete [] text;
}

//------------------------------------------------------------------------

String operator+(const String &s1, const String &s2)
{
    String rtn(s1.Length()+s2.Length()+1);
    rtn = s1;
    rtn += s2;
    return rtn;
}

String operator+(const String &s1, const char* s2)
{
    String rtn(s1.Length()+strlen(s2)+1);
    rtn = s1;
    rtn += s2;
    return rtn;
}

String operator+(const String &s1, const char c2)
{
    String rtn(s1.Length()+2);
    rtn = s1;
    rtn += c2;
    return rtn;
}

String operator+(const char *s1, String &s2)
{
    String rtn(strlen((char*)s1)+s2.Length()+1);
    rtn = s1;
    rtn += s2;
    return rtn;
}

String operator+(const char c1, String &s2)
{
    String rtn(s2.Length()+2);
    rtn += c1;
    rtn += s2;
    return rtn;
}

//----------------------------------------------------------------

int Compare(const String &s1, const String &s2)
{
    return strcmp((char *)(String &)s1, (char *)(String &)s2);
}

int operator==(const String &a, const String &b)
{
    return (a.Length()==b.Length() && Compare(a, b) == 0);
}

int operator!=(const String &a, const String &b)
{
    return (a.Length()!=b.Length() || Compare(a, b) == 0);
}

int operator>(const String &a, const String &b)
{
    return Compare(a, b) > 0;
}

int operator>=(const String &a, const String &b)
{
    return Compare(a, b) >= 0;
}

int operator<(const String &a, const String &b)
{
    return Compare(a, b) < 0;
}

int operator<=(const String &a, const String &b)
{
    return Compare(a, b) <= 0;
}

//----------------------------------------------------------------


