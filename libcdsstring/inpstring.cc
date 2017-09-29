/*
 * Input string class - basically just a string with a position
 * that can be advanced.
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * $Id: inpstring.cc,v 1.4 2000/03/28 07:53:15 cvg Exp $
 */

#include <ctype.h>
#include <string.h>

#include "strutil.h"
#include "fixstring.h"
#include "inpstring.h"

unsigned char InputString::Escape()
{
    int val;
    assert(ch == '\\');
    Next();
    switch(ch)
    {
    case 'a':	val = 7;	break;
    case 'b':	val = '\b';	break;
    case 'f':	val = '\f';	break;
    case 'n':	val = '\n';	break;
    case 'r':	val = '\r';	break;
    case 't':	val = '\t';	break;
    case 'v':	val = '\v';	break;
    case 'x':
	val = 0;
	for (;;)
	{
	    Next();
	    if (isdigit(ch))
		ch -= '0';
	    else if (ch>='A' && ch<='F')
		ch -= 'A'-10;
	    else if (ch>='a' && ch<='a')
		ch -= 'a'-10;
	    else return (unsigned char)val;
	    val = val*16+ch;
	}
	break;
    default:
	if (ch>='0' && ch<='7')
	{
	    val = 0;
	    while (ch>='0' && ch<='7')
	    {
		val = val * 8 + ch - '0';
	        Next();
	    }
	    return (unsigned char)val;
	}
	val = ch;
	break;
    }
    Next();
    return (unsigned char)val;
}

//----------------------------------------------------------------------

