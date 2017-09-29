#ifndef TEST
#include "mypilot.h"
#endif

#if defined(USE_REGEXP) || defined(USE_GLOB)

#ifdef IS_FOR_PALM
#define isspace(c)		((c)==' ' || (c)=='\t' || (c)=='\n')
#define isupper(c)		((c)>='A' && (c)<='Z')
#define islower(c)		((c)>='a' && (c)<='z')
#define toupper(c)		((char)(islower(c) ? ((c)-'a'+'A') : (c)))
#define tolower(c)		((char)(isupper(c) ? ((c)+'a'-'A') : (c)))
#define isdigit(c)		((c)>='0' && (c)<='9')
#else
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#endif


#ifndef IS_FOR_PALM

void dump(const char *pattern, int j)
{
    for (int i = 0; i < j; i++)
    {
	if (isupper(pattern[i]))
	    putchar(pattern[i]);
	else if (pattern[i] & 0x80)
	{
	    putchar('[');
	    if (pattern[i]&0x40) putchar(' ');
	    for (int c = 0; c < 26; c++)
	    {
		int idx = c+2;
		if (pattern[i+idx/8] & (0x80>>(idx%8)))
		    putchar(c+'A');
	    }
	    putchar(']');
	    i+=3;
	}
	// else closures; not done yet
    }
    putchar('\n');
}

#endif

// These are stripped down for WordFind's use - no escaped
// chars, no numeric ranges, only upper case letters, etc.

#include "regex.h"

MatchPat::~MatchPat()
{
}

#ifdef USE_REGEXP

unsigned long WFRegExp::addset(char c, char *set, int j) const
{
    // most sig bit of first byte indicates a set; next bit indicates 
    // ' ' is in the set; remaining bits are used for A-Z
    if (c == ' ')
    {
        set[j-4] |= 0x40;
        return (1ul<<26);
    }
    else
    {
	int idx = LetIdx(c)+2;
	int byt = j-4+(idx/8);
        set[byt] |= (char)(0x80 >>  (idx%8));
        return LetMask(c);
    }
}

unsigned long WFRegExp::dodash(const char *array, int &i, char *set, int j) const
{
    int start = LetIdx(array[i]);
    i+=2;
    unsigned long rtn = 0ul;
    int end = LetIdx(array[i]);
    while (start <= end)
        rtn |= addset((char)('A'+(start++)), set, j);
    return rtn;
}

unsigned long WFRegExp::filset(char delim, const char *array, int &i, char *set, int &j) const
{
    unsigned long rtn = 0ul;
    for (; array[i] != delim && array[i] != EOS; i++)
    {
	if (array[i+1]==DASH)
	    rtn |= dodash(array, i, set, j);
	else if (IsAlphabetic(array[i]))
	    rtn |= addset(array[i], set, j);
	else
	    return 0ul;
    }
    return rtn;
}

// expand char class at arg[i] into pat[j]

const char *WFRegExp::getrange(const char *arg, int &i, char *pat, int &j,
					unsigned long &mask) const
{
    int must_invert = 0;
    mask = 0ul;
    ++i; // skip [
    if (arg[i]==NOT)
    {
	must_invert = 1;
	++i;
    }
    // In WordFind we substitute a fixed 4-byte
    // bitmap for these variable closures.
    // First we make space, then we fill in the bits
    if (addchar((char)0x80, pat, j, MAXPAT)<0 ||
        addchar((char)0, pat, j, MAXPAT)<0 ||
        addchar((char)0, pat, j, MAXPAT)<0 ||
        addchar((char)0, pat, j, MAXPAT)<0)
	return "Insufficient space";
    if ((mask = filset(CCLEND, arg, i, pat, j))==0ul)
	return "Syntax error in range";
    if (arg[i] != CCLEND)
	return "Unterminated range";
    if (must_invert)
    {
	unsigned char *p = (unsigned char *)(pat+j-4);
	*p++ ^= 0x7F;
	*p++ ^= 0xFF;
	*p++ ^= 0xFF;
	*p ^= 0xFF;
	mask ^= 0x3FFFFFFul;
    }
    return 0;
}

// insert closure or optional entry at pat[j]
        
int WFRegExp::stclos(char closure, char *pat, int &j, int &lastj, int &lastcl) const
{
    for (int jp = j-1; jp>=lastj; --jp) // make a hole by moving up last elt
    {
        int jt = jp + (int)CLOSIZE;;
        if (addchar(pat[jp], pat, jt, MAXPAT)<0)
	    return -1;
    }
    j += CLOSIZE;
    int rtn = lastj;
    if (addchar((char)closure, pat, lastj, MAXPAT)<0 ||	// closure type
        addchar((char)0, pat, lastj, MAXPAT)<0 ||	// COUNT
	addchar((char)lastcl, pat, lastj, MAXPAT)<0 ||	// PREVCL
	addchar((char)0, pat, lastj, MAXPAT)<0)		// START
	    return -1;
    return rtn;
}

// compile pattern from pat_in
    
const char *WFRegExp::makepat(const char *pat_in, char *pattern,
				unsigned long &initmask,
				unsigned long &minreq,
				int &minlen, int &maxlen) const
{
    int j = 0; // pattern index
    int lastj = 0;
    int lastcl = -1;
    int i = 0;
    enum { YES, NO, MAYBE } is_first = NO;
    unsigned long m;
    const char *err;
    
    initmask = 0x3FFFFFFul;
    minlen = 0;
    maxlen = (pat_in[0] == '^') ? 0 : 32;
    minreq = 0ul;

    for (;; ++i)
    {
        int lj = j;
	char ch = pat_in[i];
	if (islower(ch)) ch = toupper(ch);
	if (isupper(ch))
	{
	    if (addchar(ch, pattern, j, MAXPAT) < 0)
		goto nospace;
	    if (is_first == YES)
	    {
	        initmask |= LetMask(ch);  
	        is_first = MAYBE;
	    }
	    else if (is_first == MAYBE)
	        is_first = NO;
	    if (pat_in[i] != '?')
	        minreq |= LetMask(ch);
	    ++maxlen;
	    ++minlen;
	}
	else switch(ch)
	{
	    case EOS:
	        if (addchar(EOS, pattern, j, MAXPAT)<0)
		    goto nospace;
#ifndef IS_FOR_PALM
		dump(pattern, j);
#endif
		if (is_first == YES) initmask = 0x3FFFFFFul;
		if (i && pat_in[i-1] != '$')
		    maxlen = 32;
		return 0; // no error
	    case ANY:
	        if (addchar(ANY, pattern, j, MAXPAT)<0)
		    goto nospace;
	        if (is_first == YES)
	        {
	            initmask = 0x3FFFFFFul;
	            is_first = MAYBE;
	        }
	        else is_first = NO;
	        ++maxlen;
	        ++minlen;
	        break;
	    case BOL:
	        if (i)
		    return "^ only allowed at start or pattern or range";
		else if (addchar(BOL, pattern, j, MAXPAT)<0)
		    goto nospace;
	        is_first = YES;
	        initmask = 0ul;
		break;
	    case EOL:
	        if (pat_in[i+1])
		    return "$ only allowed at end of pattern";
		else if (addchar(EOL, pattern, j, MAXPAT)<0)
		    goto nospace;
	        if (is_first == YES)
	            initmask = 0x3FFFFFFul;
	        is_first = NO;
	        break;
	    case CCL:
	        err = getrange(pat_in, i, pattern, j, m);
		if (err) return err;
	        if (is_first == YES)
	        {
	            initmask |= m;
	            is_first = MAYBE;
	        }
	        else is_first = NO;
	        ++maxlen;
	        ++minlen;
	        break;
	    case OPTIONAL:
	    case CLOSURE:
	        if (is_first == MAYBE) is_first = YES;
	        // fall thru
	    case CLOSUR1:
	        if (i)
	        {
	            lj = lastj;
		    if (pattern[lj]==BOL || pattern[lj]==EOL || 
		        pattern[lj]==CLOSURE ||	pattern[lj]==CLOSUR1 ||
			pattern[lj]==OPTIONAL)
			    return "Cannot apply an operator to an operator";
	            if (ch == CLOSURE || ch == CLOSUR1)
	                maxlen = 32;
	            if (ch == CLOSURE || ch == OPTIONAL)
	                --minlen;
		    lastcl = stclos(pat_in[i], pattern, j, lastj, lastcl);
		    if (lastcl>=0) break;
	        }
		else return "Cannot start with an operator";
	        // fall thru
	    default:
	        return "Invalid character";
	}
	lastj = lj;
    }
    // should be unreachable
    //return "Unknown error";
  nospace:
    return "Insufficient space";
}

//------------------------------------------------------------

// match a single pattern at pat[j]

int WFRegExp::omatch(const char *lin, int &i, const char *pat, int &j) const
{
    switch (pat[j])
    {
	case BOL:
	    return (i == 0);
	case EOL:
	    return (lin[i]==0);
	case ANY:
	    if (lin[i]==0)
	        return 0;
	    break;
	default:
	    if (pat[j]&0x80)
	    {
		if (lin[i]==0)
		    return 0;
		else if (lin[i] == ' ')
		{
		    if ((pat[j]&0x40) == 0)
			return 0;
		}
		else
		{
		    int idx = LetIdx(lin[i])+2;
		    int byt = j+(idx/8);
        	    int in_class = ((pat[byt] & (0x80 >> (idx%8))) != 0);
		    if (!in_class) return 0;
		}
	    }
	    else if (lin[i]!=pat[j])
	        return 0;
	    break;
    }
    ++i;
    return 1;
}
    
// return size of pattern entry at pat[n]
    
int WFRegExp::patsiz(const char *pat, int n) const
{
    switch (pat[n])
    {
	case OPTIONAL:
	case CLOSURE:
	case CLOSUR1:		return CLOSIZE;
	default: 		break;
    }
    return (pat[n]&0x80) ? 4 : 1;
}
    
// Look for match starting at lin[from]
    
int WFRegExp::amatch(const char *lin, int from, char *pat)
{
    int stack = -1;
    int offset = from; // next unexamined input char
    for (int j = 0; pat[j]; j += patsiz(pat, j))
    {
        if (pat[j] == OPTIONAL || pat[j] == CLOSURE || pat[j] == CLOSUR1)
	{
	    int i, cnt=0;
	    stack = j;
	    j = j + (int)CLOSIZE; // step over closure
	    for (i = offset; lin[i] != EOS; ) // match as many as possible
	    {
	        if (omatch(lin, i, pat, j))
		{
		    ++cnt;
		    if (pat[stack] == OPTIONAL)
			break;
		}
		else break;
	    }
	    pat[stack+COUNT] = (char)(i-offset);
	    if (pat[stack]==CLOSUR1 && cnt == 0)
		return -1; // must match at least one
	    pat[stack+START] = (char)offset;
	    offset = i; // character that made us fail
	}
	else if (!omatch(lin, offset, pat, j)) // non-closure
	{
	    // XXX check the inequalities here!
	    for (; stack >= 0; stack = pat[stack+PREVCL])
	        if (pat[stack+COUNT]>0)
		    break;
	    if (stack<0) // stack empty?
	        return -1;
	    pat[stack+COUNT] = (char)(pat[stack+COUNT]-1);
	    j = stack + (int)CLOSIZE;
	    offset = pat[stack+START] + pat[stack+COUNT];
	}
	// else omatch succeeded
    }
    return offset;
}
    
// Find a match anywhere in the line, given the compiled pattern
    
int WFRegExp::match(const char *lin, char *pat)
{
    for (int i = 0; lin[i]; ++i)
        if (amatch(lin, i, pat)>=0)
	    return 1;
    return 0;
}

//------------------------------------------------------------
// Public methods for regexps

const char *WFRegExp::Compile(const char *re,
				unsigned long &initmask,
    				unsigned long &minreq,
    				int &minlen, int &maxlen)
{
    if (pat == 0) pat = new char[MAXPAT];
    return makepat(re, pat, initmask, minreq, minlen, maxlen);
}

int WFRegExp::Match(const char *line)
{
    return match(line, pat);
}

WFRegExp::~WFRegExp()
{
    delete [] pat;
}

#endif USE_REGEXP
//-----------------------------------------------------------------

#ifdef USE_GLOB

const char *GlobPattern::Compile(const char *p,
				unsigned long &initmask,
				unsigned long &minreq,
				int &minlen, int &maxlen)
{
    const char *s;
    delete [] pat;
    plen = minlen = maxlen = 0;
    minreq = 0;
    for (s = p; *s; s++)
    {
        if (*s == '[')
        {
	    while (*s && *s != ']') ++s;
        }
	++plen;
	if (*s != '*') ++minlen;
	else maxlen = 0xFF;
    }
    if (maxlen == 0) maxlen = minlen;
    pat = new unsigned long[plen+1];
    pat[plen] = 0;
    int pos = 0;
    for (s = p; *s; s++, pos++)
    {
	if (*s == '*')
	    pat[pos] = 0;
	else if (*s == '?')
	    pat[pos] = 0x3FFFFFFul;
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
		    if (!IsAlphabetic(s[2]))
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
	    if (invert) pat[pos] ^= 0x3FFFFFFul;
	}
	else if (IsAlphabetic(*s))
	{
	    pat[pos] = LetMask(*s);
	    minreq |= pat[pos];
	}
	else return "Illegal character";
    }
    initmask = pat[0];
    if (initmask == 0) initmask = 0x3FFFFFFul;
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
	    if (++ppos == plen) return 1;
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

GlobPattern::~GlobPattern()
{
    delete [] pat;
}

#endif USE_GLOB

#ifdef TEST

main(int argc, char **argv)
{
#if 0
    WFRegExp re;
    const char *err = re.Compile(argv[1]);
    if (err) puts(err);
    else
    {
	while (!feof(stdin))
	{
	    char buff[128];
	    if (fgets(buff, sizeof(buff), stdin) == 0) break;
	    buff[strlen(buff)-1] = 0;
	    if (re.Match(buff)) puts(buff);
	}
    }
#else
    GlobPattern p;
    unsigned long initmask, minreq;
    int minlen, maxlen;
    const char *err = p.Compile(argv[1], initmask, minreq, minlen, maxlen);
    if (err) puts(err);
    else puts(p.Match(argv[2]) ? "Match!" : "No match");
#endif
}

#endif TEST
#endif // USE_GLOB || USE_REGEXP

