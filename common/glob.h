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
    class DynamicString pattern;
public:
    GlobPattern();
    const char *Pattern() const
    {
	return pattern.Text();
    }
    char *Compile(const char *pat);
    int Match(const char *str);
};

inline GlobPattern::GlobPattern()
    : pattern()
{
}

#endif

