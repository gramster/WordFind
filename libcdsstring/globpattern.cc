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

char *GlobPattern::Compile(const char *pat, int matchcase_in)
{
    int inrange = 0;
    matchcase = matchcase_in;
    pattern = pat;
    if (!matchcase) pattern.ToUpper();
    char *p = (char *)pattern;
    while (*p)
    {
	if (*p == '\\')
	{
	    if (*++p == 0)
		goto badexp;
	}	
	else if (inrange && *p == '-')
	{
	    p++;
	    if (*p == ']' || *p==0) goto badexp;
	}
	else if (*p == '[')
	    inrange = 1;
	else if (*p == ']' && inrange)
	    inrange = 0;
	p++;
    }
    if (inrange == 0)
	return 0;
badexp:
    return "Error in glob pattern";
}

int GlobPattern::Match(const char *str)
{
    DynamicString s(str);
    if (!matchcase) s.ToUpper();
    int mismatch = 0;
    str = (char *)s;
    char *pat = (char *)pattern;
    if (pat==0)
    	return (*str==0);
    while (*str)
    {
	char *oldpat = pat;
    	if (*pat == '\\')
	{
	    pat++;
    	    if (*pat!='?' && *pat != *str)
		goto fail;
	}
    	else if (*pat == '[')
	{
	    int inv = 0;
	    pat++;
	    if (*pat=='^') { pat++; inv = 1; }
	    while (*pat != ']')
	    {
		int first = *pat++, last;
		if (*pat == '-')
		{
		    pat++;
		    last = *pat++;
		}
		else last = first;
		if (*str >= first && *str<=last)
		{
		    if (!inv) break; // match
		}
		else if (inv) break; // match
	    }
	    if (*pat==']')
		goto fail;
	    while (*pat != ']')
	    {
		if (*pat=='\\') pat++;
		pat++;
	    }
	}
    	else if (*pat == '*')
	{
	    mismatch = 1;
	    pat++;
	    continue;
	}
    	else if (*pat!='?' && *pat != *str)
	    goto fail;
	mismatch = 0;
	pat++;
	str++;
	continue;
    fail:
	if (mismatch)
	{
	   pat = oldpat;
	   str++;
	}
	else return 0;
    }
    return (*pat==0); // return 0 if end of pattern
}

//----------------------------------------------------------------------

