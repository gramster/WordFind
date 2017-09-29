/*******************************************************************
 *
 * String utility functions for Gram's TCL interpreter for GC4
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 *
 ********************************************************************/

#ifndef STRUTIL_H
#define STRUTIL_H

#include <sys/types.h>
//#include <string.h>

#define LogOverrun()    CEQURUX_LogOverrun(__FILE__, __LINE__)

void CEQURUX_LogOverrun(const char *fname, int linenum,
				int want = 0, int got = 0);

char *strlwr(char *s);
char *strupr(char *s);
char *strrstr(const char *s1, const char *s2);
size_t strrspn(const char *s1, const char *s2);
size_t strrcspn(const char *s1, const char *s2);
char *strcasestr(const char *str, const char *substr);

size_t CEQURUX_strlcat(char *dst, const char *src, int cnt, size_t siz,
			const char *fname, int linenum);
size_t CEQURUX_strlcpy(char *dst, const char *src, int cnt, size_t siz,
			const char *fname, int linenum);

#define STRLCPY(d, s, m)	CEQURUX_strlcpy(d, s, m, m, __FILE__, __LINE__)
#define STRLCAT(d, s, m)	CEQURUX_strlcat(d, s, m, m, __FILE__, __LINE__)
#define STRNLCPY(d, s, n, m)	CEQURUX_strlcpy(d, s, n, m, __FILE__, __LINE__)
#define STRNLCAT(d, s, n, m)	CEQURUX_strlcat(d, s, n, m, __FILE__, __LINE__)

#endif

