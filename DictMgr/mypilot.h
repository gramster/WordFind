#ifndef PILOT_H

#  define PILOT_H

#  if !defined(UNIX) && !defined(TEST) && !defined(DUMP)
#    define IS_FOR_PALM
#  endif

#  ifdef IS_FOR_PALM
#    ifdef __cplusplus
       extern "C" {
#    endif
#    include <Pilot.h>
#    include <Graffiti.h>
#    include <SysEvtMgr.h>
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

#define DICTMGR
#endif

