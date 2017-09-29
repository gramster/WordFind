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

     typedef unsigned short Boolean;
#    define ltoa(x)	itoa(x)
#  endif

#define WORDFINDLITE
#define MY_CREATOR_ID		'xWrd'

#define USE_DATABASES
#define USE_LISTS
#define USE_FIELDS

#include "wordfindlite_res.h"

#endif

