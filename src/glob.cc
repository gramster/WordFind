/*
 * glob-style pattern matcher.
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * TODO: Precompile pat to a sequence of bitmasks. Use 0 for *.
 */

#include "glob.h"

const char *GlobPattern::Compile(const char *p)
{
    delete [] pat;
    plen = 0;
    for (const char *s = p; *s; s++)
    {
        if (*s == '[')
        {
	    while (*s && *s != ']') ++s;
        }
	++plen;
    }
    pat = new unsigned long[plen];
    int pos = 0;
    for (const char *s = p; *s; s++, pos++)
    {
	if (*s == '*')
	    pat[pos] = 0;
	else if (*s == '?')
	    pat[pos] = 0x3FFFFFF;
	else if (*s == '[')
	{
	    int invert = 0;
	    pat[pos] = 0;
	    ++s;
	    if (*s == '^') { invert = 1; ++s; }
	    while (*s && *s != ']')
	    {
		if (s[1] == '-')
		{
		    if (!IsAlpha(s[2]))
			return "Unterminated subrange";
		    else if (LetIdx(s[2]) < LetIdx(s[0]))
			return "Invalid subrange";
		    for (int i = LetIdx(s[0]); i <= LetIdx(s[2]); i++)
			pat[pos] |= 1ul << i;
		    s += 2;
		}
		else pat[pos] |= LetMask(*s);
		++s;
	    }
	    if (*s != ']')
		return "Unterminated range";
	    if (invert) pat[pos] ^= 0x3FFFFFF;
	}
	else if (IsAlpha(*s))
	{
	    pat[pos] = LetMask(*s);
	}
	else return "Illegal character";
    }
    return 0;
}

int GlobPattern::Match(const char *str)
{
    int wild = 0, ppos = 0;
    if (plen==0)
    	return (*str==0);
    while (*str)
    {
    	if (pat[ppos] == 0)
	{
	    wild = 1;
	    ++ppos;
	}
    	else
	{
	    // we must match everything up to the next `*' or the 
	    // end. If not, and wild is 1, we just skip this char,
	    // else we fail
	    int old_ppos = ppos;
	    const char *oldstr = str;
	    while (*str)
	    {
		if ((pat[ppos] & LetMask(*str)) != 0)
		{
		    ++str;
		    ++ppos;
		}
		else break;
	    }
	    if (ppos == plen) break;
	    else if (pat[ppos] != 0)
	    {
		ppos = old_ppos;
		str = oldstr;
		if (wild) ++str;
		else break;
	    }
	}
    }
    while (ppos < plen && pat[ppos] == 0) ++ppos;
    return (*str == 0 && ppos == plen);
}

#ifdef TEST

#include <stdio.h>

main(int argc, char **argv)
{
    GlobPattern p;
    const char *err = p.Compile(argv[1]);
    if (err) puts(err);
    else puts(p.Match(argv[2]) ? "Match!" : "No Match");
    return 0;
}

#endif


