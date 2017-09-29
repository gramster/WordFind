# Makefile for libcdsstring
# $Id: Makefile,v 1.27 2000/04/10 11:29:58 cvg Exp $

.include <citadel.defs.mk>

CFLAGS+=-DHAS_STRLCPY

LIB=    cdsstring
SRCS=	strutil.cc fixstring.cc dynstring.cc inpstring.cc globpattern.cc \
	regexp.cc charbuff.cc
HDRS=	strutil.h fixstring.h dynstring.h inpstring.h globpattern.h \
	regexp.h charbuff.h

.include <citadel.lib.mk>
.include <citadel.rules.mk>

