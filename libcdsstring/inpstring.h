/*
 * Input string class - basically just a string with a position
 * that can be advanced.
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 */

#ifndef INPSTRING_H
#define INPSTRING_H

#include "debug.h"
#include "dynstring.h"

class InputString : public DynamicString
{
    TRACE(InputString)
    int pos;
    int ch;
public:
    int Now()
    {
	return ch;
    }
    int Next()
    {
        if (pos < (int)len)
            ch = text[pos++];
        else ch = -1;
	return ch;
    }
    InputString &operator=(const String &s_in)
    {
	Set(s_in.Text(), s_in.Length());
	pos = 0;
        return *this;
    }
    void Reset()
    {
	pos = 0;
    }
    InputString(const char *text_in = 0)
	: DynamicString(text_in)
    {
	pos = 0;
	Next();
    }
    InputString(const InputString &is)
	: DynamicString(is.text)
    {
	pos = 0;
	Next();
    }
    unsigned char Escape();
};

#endif

//----------------------------------------------------------------------

