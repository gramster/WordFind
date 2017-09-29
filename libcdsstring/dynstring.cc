/*
 * String handling classes for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * $Id: dynstring.cc,v 1.3 2000/02/10 11:02:26 gram Exp $
 */

#include <string.h>

#include "strutil.h"
#include "dynstring.h"

//---------------------------------------------------------------------------
// Dynamically resizeable strings

DynamicString::DynamicString(const char *s)
    : String()
{
    if (s==0)
    {
        text = data;
        text[0] = 0;
        sz = SMALLSTRING;
    }
    else
    {
	len = strlen((char*)s);
        Resize(len+1);
        STRNLCPY(text, (char *)s, len, sz);
    }
}

DynamicString::DynamicString(const String &s)
    : String()
{
    len = s.Length();
    Resize(len+1);
    STRNLCPY(text, (char *)s.Text(), len, sz);
}

DynamicString::DynamicString(const DynamicString &s)
    : String()
{
    len = s.Length();
    Resize(len+1);
    STRNLCPY(text, (char *)s.Text(), len, sz);
}

void DynamicString::Expand(int newsz, int keep)
{
    char *newtext = (newsz > SMALLSTRING) ? (new char [newsz]) : data;
    MemAssert(newtext, text);
    if (keep && text)
    {
	if (sz > SMALLSTRING || newsz > SMALLSTRING)
            STRNLCPY(newtext, text, strlen(text), newsz);
    }
    else newtext[0] = 0;
    if (sz > SMALLSTRING)
        delete [] text;
    text = newtext;
    sz = newsz;
}

void DynamicString::Resize(int length, int keep)
{
    // round up to next smallstring chunk
    int newsz = SMALLSTRING + (length & STRINGMASK);
    assert(newsz > length);
    if (newsz > (int)sz || (newsz<=(int)(sz>>1)))
        Expand(newsz, keep);
}

void DynamicString::Insert(int pos, char ch)
{
    Resize(len+2);
    String::Insert(pos, ch);
}

void DynamicString::Insert(int pos, char *str)
{
    int l = strlen(str);
    Resize(len+l+1);
    String::Insert(pos, str);
}

void DynamicString::Insert(int pos, String &str)
{
    int l = str.Length();
    Resize(len+l+1);
    String::Insert(pos, str);
}

DynamicString::~DynamicString()
{
    if (sz <= SMALLSTRING)
	text = 0; // make sure base class doesn't try to delete static area
}

DynamicString operator+(const DynamicString &s1, const DynamicString &s2)
{   
    DynamicString rtn(s1.Length()+s2.Length()+1);
    rtn = s1;
    rtn += s2;
    return rtn;
}   
    
DynamicString operator+(const DynamicString &s1, const char* s2)
{   
    DynamicString rtn(s1.Length()+strlen(s2)+1);
    rtn = s1;
    rtn += s2;
    return rtn;
}   

DynamicString operator+(const DynamicString &s1, const char c2)
{
    DynamicString rtn(s1.Length()+2);
    rtn = s1;
    rtn += c2;
    return rtn;
}

DynamicString operator+(const char *s1, DynamicString &s2)
{
    DynamicString rtn(strlen((char*)s1)+s2.Length()+1);
    rtn = s1;
    rtn += s2;
    return rtn;
}

DynamicString operator+(const char c1, DynamicString &s2)
{   
    DynamicString rtn(s2.Length()+2);
    rtn += c1;
    rtn += s2;
    return rtn;
}

//----------------------------------------------------------------



