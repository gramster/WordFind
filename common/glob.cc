/*
 * Regular expression and glob-style pattern classes for Gram's Commander.
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * $Id: globpattern.cc,v 1.4 2000/02/10 11:02:26 gram Exp $
 */

#include "strutil.h"
#include "dynstring.h"
#include "globpattern.h"

// Tcl Glob style patterns

char *GlobPattern::Compile(const char *pat)
{
    pattern = pat;
    pattern.ToUpper();
    return 0;
}

// I don't think this is guaranteed to work in the way
// I want it to for PalmWord.

int GlobPattern::Match(const char *str)
{
    DynamicString s(str);
    s.ToUpper();
    int wild = 0;
    str = (char *)s;
    char *pat = (char *)pattern;
    if (pat==0)
    	return (*str==0);
    while (*str)
    {
	char *oldpat = pat;
    	if (*pat == '*')
	{
	    wild = 1;
	    pat++;
	}
    	else if (*pat!='?' && *pat != *str)
    	{
    	    if (wild)
    	    {
    	        pat = oldpat;
    	        ++str;
    	    }
    	    else return 0; // fail
    	}
	else // match
	{
	    wild = 0;
	    pat++;
	    str++;
	}
    }
    return (*pat==0); // return 0 if end of pattern
}

//----------------------------------------------------------------------

