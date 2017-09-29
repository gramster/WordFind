#ifndef PILOT_H

#  define PILOT_H

#  if !defined(UNIX) && !defined(TEST) && !defined(DUMP)
#    define IS_FOR_PALM
#  endif

#  ifdef IS_FOR_PALM
#    ifdef __cplusplus
       extern "C" {
#    endif
#    include <PalmOS.h>
#    ifdef __cplusplus
       }
#    endif
#  else
#    include <stdio.h>
#    include <string.h>
#    include <ctype.h>

     typedef unsigned short UInt;
     typedef unsigned short Boolean;
#    define ltoa(x)	itoa(x)
#  endif

// App specific

#define WORDFINDPP
#define MY_CREATOR_ID		'xWrd'

#define USE_DATABASES
#define USE_LISTS
#define USE_DATABASE_LIST_SOURCES
#define USE_FIELDS
#define USE_DIALOGS
#define USE_GLOB

// res file

#include "wordfind++_res.h"

#endif

