#ifndef PILOT_H

#  define PILOT_H

#  ifdef TEST
#    include <stdio.h>

     typedef unsigned short UInt;
     typedef unsigned short Boolean;
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

