/*
 * glob-style pattern matcher.
 * Written by Graham Wheeler. gram@bradygirl.com
 * (c) 2001, All Rights Reserved
 */

#ifndef GLOB_H
#define GLOB_H

class GlobPattern
{
    int plen;
    unsigned long *pat;
    inline int LetIdx(c) const
    {
	return ((c&0x5F)-('A'&0x5F));
    }
    inline int IsAlpha(c) const
    {
	return (c>='A' && c<='z' && LetIdx(c)<26);
    }
    inline unsigned long LetMask(char c) const
    {
        return 1ul << LetIdx(c);
    }
  public:
    GlobPattern()
      : plen(0), pat(0)
    {}
    const char *Compile(const char *p);
    int Match(const char *s);
    ~GlobPattern()
    {
        delete [] pat;
    }
};

#endif


