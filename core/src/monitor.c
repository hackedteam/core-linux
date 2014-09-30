#include <stdio.h>

#include "monitor.h"

monitor_t monitorlist[] = {
   MON_TIME_ENTRY,
   MON_PROCESS_ENTRY,
   MON_CONNECTION_ENTRY,
   MON_SCREENSAVER_ENTRY,
   MON_IDLE_ENTRY,
#if 0
   MON_WINDOW_ENTRY,
#endif
};
