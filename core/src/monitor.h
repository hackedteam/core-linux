#ifndef _MONITOR_H
#define _MONITOR_H 1

/*** 0 - Time monitor ***/
#define MON_TIME_INDEX 0
#define MON_TIME monitorlist[MON_TIME_INDEX]
#define MON_TIME_ENTRY { MON_TIME_INDEX, mon_time_start, NULL, mon_time_addwatch, mon_time_delwatch }
void mon_time_start(void *args);
void *mon_time_addwatch(void *(*callback)(void *), void *args, int flags, ...);
void mon_time_delwatch(void *entry);

/*** 1 - Process monitor ***/
#define MON_PROCESS_INDEX 1
#define MON_PROCESS monitorlist[MON_PROCESS_INDEX]
#define MON_PROCESS_ENTRY { MON_PROCESS_INDEX, mon_process_start, NULL, mon_process_addwatch, mon_process_delwatch }
void mon_process_start(void *args);
void *mon_process_addwatch(void *(*callback)(void *), void *args, int flags, ...);
void mon_process_delwatch(void *entry);

/*** 2 - Connection monitor ***/
#define MON_CONNECTION_INDEX 2
#define MON_CONNECTION monitorlist[MON_CONNECTION_INDEX]
#define MON_CONNECTION_ENTRY { MON_CONNECTION_INDEX, mon_connection_start, NULL, mon_connection_addwatch, mon_connection_delwatch }
void mon_connection_start(void *args);
void *mon_connection_addwatch(void *(*callback)(void *), void *args, int flags, ...);
void mon_connection_delwatch(void *entry);

/*** 3 - Screensaver monitor ***/
#define MON_SCREENSAVER_INDEX 3
#define MON_SCREENSAVER monitorlist[MON_SCREENSAVER_INDEX]
#define MON_SCREENSAVER_ENTRY { MON_SCREENSAVER_INDEX, mon_screensaver_start, NULL, mon_screensaver_addwatch, mon_screensaver_delwatch }
void mon_screensaver_start(void *args);
void *mon_screensaver_addwatch(void *(*callback)(void *), void *args, int flags, ...);
void mon_screensaver_delwatch(void *entry);

/*** 4 - Idle monitor ***/
#define MON_IDLE_INDEX 4
#define MON_IDLE monitorlist[MON_IDLE_INDEX]
#define MON_IDLE_ENTRY { MON_IDLE_INDEX, mon_idle_start, NULL, mon_idle_addwatch, mon_idle_delwatch }
void mon_idle_start(void *args);
void *mon_idle_addwatch(void *(*callback)(void *), void *args, int flags, ...);
void mon_idle_delwatch(void *entry);

#if 0
/*** 5 - Process monitor ***/
#define MON_WINDOW_INDEX 5
#define MON_WINDOW monitorlist[MON_WINDOW_INDEX]
#define MON_WINDOW_ENTRY { MON_WINDOW_INDEX, mon_window_start, NULL, mon_window_addwatch, mon_window_delwatch }
void mon_window_start(void *args);
void *mon_window_addwatch(void *(*callback)(void *), void *args, int flags, ...);
void mon_window_delwatch(void *entry);
#endif

#define MON_FLAG_NONE 0x0000
#define MON_FLAG_NEG  0x0001
#define MON_FLAG_PERM 0x0002

typedef struct {
   int monitor_id;
   void (*start)(void *);
   void (*stop)(void *);
   void *(*addwatch)(void *(*)(void *), void *args, int flags, ...);
   void (*delwatch)(void *);
} monitor_t;

extern monitor_t monitorlist[];

#endif /* _MONITOR_H */
