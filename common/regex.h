#ifndef WFREGEX_H
#define WFREGEX_H

// Simple Regular Expressions
//
// To help speed up the matching in WordFind, we build up some extra info.
// My first idea was to have 26-elt arrays of minimum required and max
// available counts for each letter. However, the max avail is a waste of
// time, as most regexps will either not be anchored on at least one end,
// or will contain a + or * closure applied to a wild, which means no limit
// on any char. Also, the minimum avail is complicated to compute, and 
// expensive to check. 
//
// Instead I create two bitmasks. One represent the set of possible initial
// letters. If the regexp is not anchored at the front, this will be the
// full set, and buys us nothing, but it greatly speeds up searches where
// an initial letter is specified. The return values of most compile
// routines now represent a mask, so a return of zero is an error, rather
// than the old -1.
//
// The second mask represents a minimal set of letters that must appear.
// For each literal letter in the regexp that is not followed by a ? closure
// operator, we set the corresponding bit. When building the word before 
// passing to the regexp matcher, we build a similar mask of letters 
// actually used. We then enforce ((min & actual) == min).
//
// Some other optimisations: determine from the regexp the max
// and min word lengths. The max is unlimited unless the regexp is
// anchored on both ends and contains no + or * closures. This means it
// often won't be useful, but when it is bounded, it should speed things
// up a lot as it eliminates many subtrees.
// The minimum is the number of chars or ranges that have either
// no closure operator or a + closure operator applied.
//
// Also, if we have a regexp that is not anchored at the end, and we have
// a prefix that matches the regexp, the entire subtree rooted at that
// prefix will match. As we don't want to test every prefix, this could 
// be exploited by: if we have a regexp with no $, and we have a matching
// word which has a non-zero first child, then match everything until we
// change the char at the current terminal stack pos or pop past this
// stack pos. I think the way to do this is to have a var, `match_from_sp',
// which should be set to sp+1 before we recurse to the first child.
// Then if sp>=match_from_sp we don't need to test the regexp, but if 
// sp < match_from_sp we reset match_from_sp to 32.
// However, for the Palm itself, it isn't worth implementing this, as we
// only have to do ten matches before we pause anyway waiting for a tap.
// This optimisation is only useful for the Windows prog, which is fast
// enough anyway.


class WFRegExp
{
	// Define useful constants with an enumeration, as "const"s are read-only vars, not literals
	enum {
	    MAXPAT	= 256,

	    EOS		= 0,
	    BOL		= '^',
	    EOL		= '$',
	    ANY		= '.',
	    CCL		= '[',
	    CCLEND	= ']',
	    NCCL	= '{',
	    CLOSURE	= '*',
	    CLOSUR1	= '+',
	    ESCAPE	= '\\',
	    NOT		= '^',
	    OPTIONAL	= '?',

	    // not done yet

	    CHOICE		= ']',
	    GROUP		= '(',

	    CLOSIZE	= 4,
	    COUNT	= 1,
	    PREVCL	= 2,
	    START	= 3,

	    DASH	= '-',
	    LETN	= 'n',
	    LETT	= 't',
	    TAB		= '\t',
	    NEWLINE	= '\n'
	};

    inline int index(char c) const
    {
        return (c-'A')&0x1F;
    }
    inline unsigned long bitmask(char c) const
    {
        return 1ul << index(c);
    }
    unsigned long addset(char c, char *set, int j) const;

    inline int addchar(char c, char *set, int &j, int maxsiz) const
    {
        if (j>=maxsiz) return -1;
        set[j++] = c;
        return 0;
    }

    unsigned long dodash(const char *array, int &i, char *set, int j) const;

    unsigned long filset(char delim, const char *array, int &i, char *set, int &j) const;

    // expand char class at arg[i] into pat[j]

    unsigned long getrange(const char *arg, int &i, char *pat, int &j) const;

    // insert closure entry at pat[j]
        
    int stclos(char closure, char *pat, int &j, int &lastj, int &lastcl) const;

    // make pattern from arg[from], terminate at delim
    
    unsigned long makepat(const char *arg, char *pat, unsigned long &minreq,
    				int &minlen, int &maxlen) const;

    //------------------------------------------------------------

    // match a single pattern at pat[j]

    int omatch(const char *lin, int &i, const char *pat, int &j) const;
    
    // return size of pattern entry at pat[n]
    
    int patsiz(const char *pat, int n) const;
  
    // Look for match starting at lin[from]
    
    int amatch(const char *lin, int from, char *pat);
 
    // Find a match anywhere in the line, given the compiled pattern
    
    int match(const char *lin, char *pat);
 
    char pat[MAXPAT];
  public:
    WFRegExp()
    {
        pat[0] = 0;
    }
    // we return the initlettermask, or zero if an error
    inline unsigned long Compile(const char *arg,
    				 unsigned long &minreq,
    				 int &minlen, int &maxlen)
    {
	return makepat(arg, pat, minreq, minlen, maxlen);
    }
    inline int Match(const char *line)
    {
	return match(line, pat);
    }
};

#endif



