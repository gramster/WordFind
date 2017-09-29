/*******************************************************************
 *
 * String handling classes for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 ********************************************************************/

#ifndef DYNSTRING_H
#define DYNSTRING_H

//#include <string.h>
#include "debug.h"
#include "fixstring.h"

//-----------------------------------------------------------------------------
// DynamicString : strings of dynamic size

#define SMALLSTRING	16 	// Must be a power of 2!!
#define STRINGMASK	(~15) 	// high bits of size

class DynamicString : public String
{
    TRACE(DynamicString)
protected:
    char	data[SMALLSTRING];

    void Expand(int newsz, int keep);
    virtual void Resize(int length, int keep = 1);
public:
    DynamicString(const char *s = 0);
    DynamicString(const String &s);
    DynamicString(const DynamicString &s);
    virtual void Insert(int pos, String &str);
    virtual void Insert(int pos, char *str);
    virtual void Insert(int pos, char ch);
    virtual ~DynamicString();
};

DynamicString operator+(const DynamicString &s1, const DynamicString &s2);
DynamicString operator+(const DynamicString &s1, const char* s2);
DynamicString operator+(const DynamicString &s1, const char c2);
DynamicString operator+(const char *s1, DynamicString &s2);
DynamicString operator+(const char c1, DynamicString &s2);

//----------------------------------------------------------------

#endif

    

