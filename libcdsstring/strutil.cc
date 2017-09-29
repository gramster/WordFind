/*
 * String utility functions for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 * $Id: strutil.cc,v 1.7 2000/02/11 08:20:34 gram Exp $
 */

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>

#include "strutil.h"

void CEQURUX_LogOverrun(const char *fname, int linenum, int want, int got)
{
    if (want>0)
        syslog(LOG_ERR, "Buffer overrun (%d>%d) at %s:%d resulted in truncation",
		want, got, fname, linenum);
    else
        syslog(LOG_ERR, "Buffer overrun at %s:%d resulted in truncation",
		fname, linenum);
}

// missing string functions; for now we just hack 'em pretty
// inefficiently (assuming a search from the back would be
// faster in typical use of "string last")

char *strlwr(char *str)
{
    for (char *s = str ; *s ; s++)
	if (isupper(*s))
	    *s = tolower(*s);
    return str;
}

char *strupr(char *str)
{
    for (char *s = str ; *s ; s++)
	if (islower(*s))
	    *s = toupper(*s);
    return str;
}

char *strrstr(const char *s1, const char *s2)
{
    char *rtn = 0, *tmp = strstr(s1, s2);
    while (tmp)
    {
	rtn = tmp;
	tmp = strstr(tmp+1, s2);
    }
    return rtn;
}

size_t strrspn(const char *s1, const char *s2)
{
    int l = strlen(s1);
    while (--l >= 0)
    {
	if (strchr(s2, s1[l])==0)
	    break;
    }
    return l+1;
}

size_t strrcspn(const char *s1, const char *s2)
{
    int l = strlen(s1);
    while (--l >= 0)
    {
	if (strchr(s2, s1[l])!=0)
	    break;
    }
    return l+1;
}

char *strcasestr(const char *str, const char *substr)
{
    int l = strlen(str), l2 = strlen(substr);
    if (l < 256 && l2 < 256)
    {
        char b1[256], b2[256];
        STRLCPY(b1, str, sizeof(b1)); strlwr(b1);
        STRLCPY(b2, substr, sizeof(b2)); strlwr(b2);
        char *x = strstr(b1, b2);
        if (x) return (char*)(str+(x-b1));
        else return 0;
    }
    else // dynamically allocate space
    {
	char *rtn = 0;
        char *b1 = new char[l+1];
	if (b1 == 0) return 0;
        char *b2 = new char[l2+1];
	if (b2 == 0) { delete [] b1; return 0; }
        STRLCPY(b1, str, l+1); strlwr(b1);
        STRLCPY(b2, substr, l2+1); strlwr(b2);
        char *x = strstr(b1, b2);
        if (x) rtn = (char*)(str+(x-b1));
	delete [] b2;
	delete [] b1;
        return rtn;
    }
}

/*
 * The idea for the next two routines comes from OpenBSD.
 * These are similar to strncpy/strncat but have some more
 * sensible semantics. In particular:
 * 
 * - the size parameter specifies the total size of the destination
 *	buffer, not the available space
 * - NUL termination always occurs
 * - the return value is what the length should be. If this is
 *	greater than the size parameter truncation has occured.
 */

size_t CEQURUX_strlcat(char *dst, const char *src,
			int cnt, size_t siz,
			const char *fname, int linenum)
{
    register size_t sl = strlen(src);
    size_t dl = strlen(dst); // already used space in dest
    size_t space = siz-dl-1; // space left in dest
    if (cnt>= 0 && sl > (unsigned)cnt) sl = cnt; // # of bytes of src to copy

    // the new length, assuming there is space

    size_t rtn = dl+sl+1; 

    // check if there is space for sl plus NUL byte

    if (rtn > siz)
    {
	CEQURUX_LogOverrun(fname, linenum, sl, space);
	sl = space;
    }

    // do the copy

    while (sl-->0)
	dst[dl++] = *src++;
    dst[dl]=0;

    // return the desired length

    return rtn;
}

size_t CEQURUX_strlcpy(char *dst, const char *src, int cnt, size_t siz,
			const char *fname, int linenum)
{
    dst[0] = 0;
    return CEQURUX_strlcat(dst, src, cnt, siz, fname, linenum);
}


