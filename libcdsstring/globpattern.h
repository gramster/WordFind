/*
 * Glob-style pattern classes for Gram's Commander.
 * Written by Graham Wheeler. gram@aztec.co.za
 * (c) 1994, All Rights Reserved
 */

#ifndef GLOBPATTERN_H
#define GLOBPATTERN_H

#include "debug.h"
#include "dynstring.h"

// Tcl glob style patterns (like DOS)

class GlobPattern
{
    TRACE(GlobPattern)
    class DynamicString pattern;
    int matchcase;
public:
    GlobPattern();
    const char *Pattern() const
    {
	return pattern.Text();
    }
    char *Compile(const char *pat, int matchcase_in = 1);
    int Match(const char *str);
    ~GlobPattern();
};

inline GlobPattern::GlobPattern()
    : pattern()
{
}

inline GlobPattern::~GlobPattern()
{
}

#endif

