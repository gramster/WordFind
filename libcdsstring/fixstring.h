/*******************************************************************
 *
 * String handling classes for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * fixstring.h: strings of fixed length
 *
 ********************************************************************/

#ifndef FIXSTRING_H
#define FIXSTRING_H

#include "portability.h"
#include <string.h>
#include "debug.h"

//--------------------------------------------------------------------
// String : string of predetermined maximum size. There are two
//	reasons for having such a class - we may wish to (or have to)
//	enforce a maximum length on a string, and/or we may wish a string
//	to always return the same value when cast to a (char*).
//
// Note that the implementation always resizes the string contents by
// calling the virtual Resize method before any assignments to the contents
// of the string. This allows us to derive a dynamic string class in a
// trivial fashion by redefining Resize..

class String 
{
    TRACE(String)

protected:

    char	*text;
    unsigned	len;    // length excluding NUL byte
    unsigned	sz;	// allocated space
    virtual void Resize(int length, int keep = 1);

public:

    String();
    String(int len);
    String(const char *s);
    String(const String &s);
    virtual ~String();

    static int Compare(const String &s1, const String &s2);

    void Trunc(int newlen);
    const int Length() const;
    void Set(const char *s);
    void Set(const char *s, int length);
    void Set(int val);
#if __MSDOS__
    void Set(long val);
#endif
    void Append(const char *string);
    void Append(const char *string, int length);
    void Append(char ch);
    void Append(int val);
    String &operator=(const char *s_in);
    String &operator=(const String &s_in);
    String &operator=(const char ch);
    String &operator=(const int val);
#if __MSDOS__
    String &operator=(const long val);
#endif
    char &operator[](unsigned i);
    void operator+=(const char *text_in);
    void operator+=(const String &s_in);
    void operator+=(const char ch);
    void operator+=(int val);
    const char * const Text() const;
    operator const char*();
    operator char*();
    virtual void Insert(int pos, String &str);
    virtual void Insert(int pos, char *str);
    virtual void Insert(int pos, char ch);
    void Remove(int pos, int len = 1);
    int IsNumeric(int *val);
    int IsNumeric(long *val);
    char * const TrimLeft(char *remove = "\r\n\t ");
    char * const TrimRight(char *remove = "\r\n\t ");
    char * const Trim(char *remove = "\r\n\t ");
    char * const ToUpper();
    char * const ToLower();
};

// Comparisons


int operator==(const String &a, const String &b);
int operator!=(const String &a, const String &b);
int operator>(const String &a, const String &b);
int operator>=(const String &a, const String &b);
int operator<(const String &a, const String &b);
int operator<=(const String &a, const String &b);

// Concatenation operations. 

String operator+(const String &s1, const String &s2);
String operator+(const String &s1, const char* s2);
String operator+(const String &s1, const char c2);
String operator+(const char *s1, String &s2);
String operator+(const char c1, String &s2);

//----------------------------------------------------------------
#ifndef INLINE
#ifdef NO_INLINES
#define INLINE
#else
#define INLINE inline
#endif
#endif

#ifndef NO_INLINES
#include "fixstring.inl"
#endif

#endif

    

