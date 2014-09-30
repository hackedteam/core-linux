#ifndef _EVENTMANAGER_H
#define _EVENTMANAGER_H 1

#include <json.h>
#include <time.h>
#include <netinet/in.h>

typedef struct {
   int type;
   int enabled;
   int active;
   int begin;
   void *bp;
   int repeat;
   void *rp;
   int end;
   void *ep;
   int delay;
   int iter;
   void *param;
} event_t;

#define EVENT_NONE -1

#define EVENT_PROCESS 0
typedef struct {
   char name[256];
   int window;
   int focus;
} event_process_t;
#define EVENT_PROCESS_P(x) ((event_process_t *)((x)->param))

#define EVENT_CONNECTION 1
typedef struct {
   in_addr_t ip;
   in_addr_t mask;
   in_port_t port;
} event_connection_t;
#define EVENT_CONNECTION_P(x) ((event_connection_t *)((x)->param))

#define EVENT_TIMER 2
typedef struct {
   int daily;
   time_t begin;
   time_t end;
} event_timer_t;
#define EVENT_TIMER_P(x) ((event_timer_t *)((x)->param))

#define EVENT_SCREENSAVER 3
typedef void event_screensaver_t;
#define EVENT_SCREENSAVER_P(x) ((event_screensaver_t *)((x)->param))

#define EVENT_IDLE 4
typedef struct {
   int idle;
} event_idle_t;
#define EVENT_IDLE_P(x) ((event_idle_t *)((x)->param))

#define EVENT_QUOTA 5
typedef struct {
   int quota;
} event_quota_t;
#define EVENT_QUOTA_P(x) ((event_quota_t *)((x)->param))

#define EVENT_WINDOW 6
typedef void event_window_t;
#define EVENT_WINDOW_P(x) ((event_window_t *)((x)->param))

#define EVENT_BEGIN 0
#define EVENT_END   1
#define EVENT_FLUSH 2

void em_parseevents(json_object *config);
void em_flushevents(void);
void em_scheduleevents(void);
void em_enableevent(int eventnun);
void em_disableevent(int eventnum);

#endif /* _EVENTMANAGER_H */
