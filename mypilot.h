#ifndef PILOT_H

#  define PILOT_H

#  ifdef TEST
#    include <stdio.h>
#    include <string.h>
#    include <ctype.h>

     typedef unsigned short UInt;
     typedef unsigned short Boolean;
#    define ltoa(x)	itoa(x)
#  else
#    ifdef __cplusplus
       extern "C" {
#    endif
#    include <Pilot.h>
#    include <Graffiti.h>
#    include <SysEvtMgr.h>
#    ifdef __cplusplus
       }
#    endif

#  endif // TEST

#endif

