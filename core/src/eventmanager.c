#define _BSD_SOURCE
#define _XOPEN_SOURCE
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "eventmanager.h"
#include "actionmanager.h"
#include "config.h"
#include "monitor.h"
#include "me.h"
#include "so.h"

static void schedule(event_t *e, int hook);
static void *onbegin(void *data);
static void *onrepeat(void *data);
static void *onend(void *data);

static int eventc = 0;
static event_t *eventlist = NULL;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void em_parseevents(json_object *config)
{
   int i;
   json_object *events, *e;
   char *type, *subtype, *v;
   struct tm tm;
   time_t now;
   struct stat s;

   pthread_mutex_lock(&m);

   for(i = 0; i < eventc; i++) {
      schedule(&eventlist[i], EVENT_FLUSH);
      if(eventlist[i].param) free(eventlist[i].param);
   }
   if(eventlist) free(eventlist);

   events = json_object_object_get(config, SO"events");
   eventc = json_object_array_length(events);
   eventlist = malloc(eventc * sizeof(event_t));

   for(i = 0; i < eventc; i++) {
      e = json_object_array_get_idx(events, i);

      eventlist[i].type = EVENT_NONE;
      eventlist[i].active = 0;
      eventlist[i].bp = NULL;
      eventlist[i].rp = NULL;
      eventlist[i].ep = NULL;
      eventlist[i].param = NULL;

      if(get_boolean(e, SO"enabled", &eventlist[i].enabled)) continue;
      get_int(e, SO"start", &eventlist[i].begin);
      get_int(e, SO"repeat", &eventlist[i].repeat);
      get_int(e, SO"delay", &eventlist[i].delay);
      get_int(e, SO"iter", &eventlist[i].iter);
      get_int(e, SO"end", &eventlist[i].end);

      if(get_string(e, SO"event", &type)) continue;
      if(!strcmp(type, SO"process")) {
         if(!(eventlist[i].param = malloc(sizeof(event_process_t)))) continue;
         if(get_string(e, SO"process", &v)) continue;
         snprintf(((event_process_t *)eventlist[i].param)->name, sizeof(((event_process_t *)eventlist[i].param)->name), "%s", v);
         if(get_boolean(e, SO"window", &((event_process_t *)eventlist[i].param)->window)) continue;
         if(get_boolean(e, SO"focus", &((event_process_t *)eventlist[i].param)->focus)) continue;
         eventlist[i].type = EVENT_PROCESS;
      } else if(!strcmp(type, SO"connection")) {
         if(!(eventlist[i].param = malloc(sizeof(event_connection_t)))) continue;
         if(get_string(e, SO"ip", &v)) continue;
         ((event_connection_t *)eventlist[i].param)->ip = inet_addr(v);
         if(get_string(e, SO"netmask", &v)) continue;
         ((event_connection_t *)eventlist[i].param)->mask = inet_addr(v);
         if(get_int(e, SO"port", ((int *)&((event_connection_t *)eventlist[i].param)->port))) continue;
         eventlist[i].type = EVENT_CONNECTION;
      } else if(!strcmp(type, SO"timer")) {
         if(!(eventlist[i].param = malloc(sizeof(event_timer_t)))) continue;
         if(get_string(e, SO"subtype", &subtype)) continue;
         if(!strcmp(subtype, SO"loop")) {
            ((event_timer_t *)eventlist[i].param)->daily = 0;
            ((event_timer_t *)eventlist[i].param)->begin = (time_t)0;
            ((event_timer_t *)eventlist[i].param)->end = (time_t)0;
         } else if(!strcmp(subtype, SO"daily")) {
            ((event_timer_t *)eventlist[i].param)->daily = 1;
            now = time(NULL);
            gmtime_r(&now, &tm);
            if(get_string(e, SO"ts", &v)) continue;
            if(!strptime(v, SO"%H:%M:%S", &tm)) continue;
            ((event_timer_t *)eventlist[i].param)->begin = mktime(&tm);
            if(get_string(e, SO"te", &v)) continue;
            if(!strptime(v, SO"%H:%M:%S", &tm)) continue;
            ((event_timer_t *)eventlist[i].param)->end = mktime(&tm);
            if(((event_timer_t *)eventlist[i].param)->end < now) {
               ((event_timer_t *)eventlist[i].param)->begin += 24 * 3600;
               ((event_timer_t *)eventlist[i].param)->end += 24 * 3600;
            }
         }
         eventlist[i].type = EVENT_TIMER;
      } else if(!strcmp(type, SO"date")) {
         if(!(eventlist[i].param = malloc(sizeof(event_timer_t)))) continue;
         ((event_timer_t *)eventlist[i].param)->daily = 0;
         if(get_string(e, SO"datefrom", &v)) continue;
         if(!strptime(v, SO"%Y-%m-%d %H:%M:%S", &tm)) continue;
         ((event_timer_t *)eventlist[i].param)->begin = mktime(&tm);
         if(get_string(e, SO"dateto", &v)) {
            ((event_timer_t *)eventlist[i].param)->end = (time_t)0;
         } else {
            if(!strptime(v, SO"%Y-%m-%d %H:%M:%S", &tm)) continue;
            ((event_timer_t *)eventlist[i].param)->end = mktime(&tm);
         }
         eventlist[i].type = EVENT_TIMER;
      } else if(!strcmp(type, SO"afterinst")) {
         if(!(eventlist[i].param = malloc(sizeof(event_timer_t)))) continue;
         ((event_timer_t *)eventlist[i].param)->daily = 0;
         if(stat(SO".lock", &s)) continue;
         if(get_int(e, SO"days", ((int *)&((event_timer_t *)eventlist[i].param)->begin))) continue;
         ((event_timer_t *)eventlist[i].param)->begin = (((event_timer_t *)eventlist[i].param)->begin * 24 * 3600) + s.st_mtime;
         ((event_timer_t *)eventlist[i].param)->end = 0;
         eventlist[i].type = EVENT_TIMER;
      } else if(!strcmp(type, SO"screensaver")) {
         eventlist[i].param = NULL;
         eventlist[i].type = EVENT_SCREENSAVER;
      } else if(!strcmp(type, SO"idle")) {
         if(!(eventlist[i].param = malloc(sizeof(event_idle_t)))) continue;
         if(get_int(e, SO"time", &((event_idle_t *)eventlist[i].param)->idle)) continue;
         eventlist[i].type = EVENT_IDLE;
      } else if(!strcmp(type, SO"quota")) {
         if(!(eventlist[i].param = malloc(sizeof(event_quota_t)))) continue;
         if(get_int(e, SO"quota", &((event_quota_t *)eventlist[i].param)->quota)) continue;
         eventlist[i].type = EVENT_QUOTA;
      } else if(!strcmp(type, SO"window")) {
         eventlist[i].param = NULL;
         eventlist[i].type = EVENT_WINDOW;
      }
   }

   pthread_mutex_unlock(&m);

   return;
}

void em_flushevents(void)
{
   int i;

   pthread_mutex_lock(&m);

   for(i = 0; i < eventc; i++) {
      schedule(&eventlist[i], EVENT_FLUSH);
      if(eventlist[i].param) free(eventlist[i].param);
   }
   if(eventlist) free(eventlist);

   eventlist = NULL;
   eventc = 0;

   pthread_mutex_unlock(&m);

   return;
}

void em_scheduleevents(void)
{
   int i;

   pthread_mutex_lock(&m);
   for(i = 0; i < eventc; i++) schedule(&eventlist[i], EVENT_BEGIN);
   pthread_mutex_unlock(&m);

   return;
}

void em_enableevent(int eventnum)
{
   pthread_mutex_lock(&m);
   if(eventnum < eventc) eventlist[eventnum].enabled = 1;
   pthread_mutex_unlock(&m);
}

void em_disableevent(int eventnum)
{
   pthread_mutex_lock(&m);
   if(eventnum < eventc) eventlist[eventnum].enabled = 0;
   pthread_mutex_unlock(&m);
}

static void schedule(event_t *e, int hook)
{
   switch(e->type) {
      case EVENT_PROCESS:
         if(hook == EVENT_BEGIN) {
            if(!EVENT_PROCESS_P(e)->window) e->bp = MON_PROCESS.addwatch(&onbegin, e, MON_FLAG_NONE, EVENT_PROCESS_P(e)->name);
         } else if(hook == EVENT_END) {
            if(!EVENT_PROCESS_P(e)->window) e->ep = MON_PROCESS.addwatch(&onend, e, MON_FLAG_NEG, EVENT_PROCESS_P(e)->name);
         } else if(hook == EVENT_FLUSH) {
            if(!EVENT_PROCESS_P(e)->window) {
               if(e->bp) MON_PROCESS.delwatch(e->bp);
               if(e->rp) MON_PROCESS.delwatch(e->rp);
               if(e->ep) MON_PROCESS.delwatch(e->ep);
            }
         }
         break;
      case EVENT_CONNECTION:
         if(hook == EVENT_BEGIN) {
            e->bp = MON_CONNECTION.addwatch(&onbegin, e, MON_FLAG_NONE, EVENT_CONNECTION_P(e)->ip, EVENT_CONNECTION_P(e)->mask, EVENT_CONNECTION_P(e)->port);
         } else if(hook == EVENT_END) {
            e->ep = MON_CONNECTION.addwatch(&onend, e, MON_FLAG_NEG, EVENT_CONNECTION_P(e)->ip, EVENT_CONNECTION_P(e)->mask, EVENT_CONNECTION_P(e)->port);
         } else if(hook == EVENT_FLUSH) {
            if(e->bp) MON_CONNECTION.delwatch(e->bp);
            if(e->rp) MON_CONNECTION.delwatch(e->rp);
            if(e->ep) MON_CONNECTION.delwatch(e->ep);
         }
         break;
      case EVENT_TIMER:
         if(hook == EVENT_BEGIN) {
            if(!EVENT_TIMER_P(e)->end || (EVENT_TIMER_P(e)->end > time(NULL))) {
               e->bp = MON_TIME.addwatch(&onbegin, e, MON_FLAG_NONE, EVENT_TIMER_P(e)->begin, 0, 0);
            }
         } else if(hook == EVENT_END) {
            if(EVENT_TIMER_P(e)->end) {
               e->ep = MON_TIME.addwatch(&onend, e, MON_FLAG_NONE, EVENT_TIMER_P(e)->end, 0, 0);
               if(EVENT_TIMER_P(e)->daily) {
                  EVENT_TIMER_P(e)->begin += 24 * 3600;
                  EVENT_TIMER_P(e)->end += 24 * 3600;
               }
            }
         } else if(hook == EVENT_FLUSH) {
            if(e->bp) MON_TIME.delwatch(e->bp);
            if(e->rp) MON_TIME.delwatch(e->rp);
            if(e->ep) MON_TIME.delwatch(e->ep);
         }
         break;
      case EVENT_SCREENSAVER:
         if(hook == EVENT_BEGIN) {
            e->bp = MON_SCREENSAVER.addwatch(&onbegin, e, MON_FLAG_NONE);
         } else if(hook == EVENT_END) {
            e->ep = MON_SCREENSAVER.addwatch(&onend, e, MON_FLAG_NEG);
         } else if(hook == EVENT_FLUSH) {
            if(e->bp) MON_SCREENSAVER.delwatch(e->bp);
            if(e->rp) MON_SCREENSAVER.delwatch(e->rp);
            if(e->ep) MON_SCREENSAVER.delwatch(e->ep);
         }
         break;
      case EVENT_IDLE:
         if(hook == EVENT_BEGIN) {
            e->bp = MON_IDLE.addwatch(&onbegin, e, MON_FLAG_NONE, EVENT_IDLE_P(e)->idle);
         } else if(hook == EVENT_END) {
            e->ep = MON_IDLE.addwatch(&onend, e, MON_FLAG_NEG, EVENT_IDLE_P(e)->idle);
         } else if(hook == EVENT_FLUSH) {
            if(e->bp) MON_IDLE.delwatch(e->bp);
            if(e->rp) MON_IDLE.delwatch(e->rp);
            if(e->ep) MON_IDLE.delwatch(e->ep);
         }
         break;
      default:
         break;
   }
 
   return;
}

static void *onbegin(void *data)
{
   event_t *e = data;

   if(e->active) return NULL;

   pthread_mutex_lock(&m);

   e->active = 1;

   e->bp = NULL;

   if(e->repeat >= 0) {
      if(e->rp) MON_TIME.delwatch(e->rp);
      e->rp = MON_TIME.addwatch(&onrepeat, e, MON_FLAG_PERM, time(NULL) + e->delay, e->delay, e->iter);
   }

   schedule(e, EVENT_END);

   if(e->enabled && (e->begin >= 0)) {
      am_queueaction(e->begin);
      debugme("[%lu]> begin action %d\n", time(NULL), e->begin);
   }

   pthread_mutex_unlock(&m);

   return NULL;
}

static void *onrepeat(void *data)
{
   event_t *e = data;

   pthread_mutex_lock(&m);

   if(e->enabled && (e->repeat >= 0)) {
      am_queueaction(e->repeat);
      debugme("[%lu]> repeat action %d (delay: %d, iter: %d)\n", time(NULL), e->repeat, e->delay, e->iter);
   }

   pthread_mutex_unlock(&m);

   return NULL;
}

static void *onend(void *data)
{
   event_t *e = data;

   if(!e->active) return NULL;

   pthread_mutex_lock(&m);

   e->active = 0;

   e->ep = NULL;

   if(e->rp) {
      MON_TIME.delwatch(e->rp);
      e->rp = NULL;
   }

   schedule(e, EVENT_BEGIN);

   if(e->enabled && (e->end >= 0)) {
      am_queueaction(e->end);
      debugme("[%lu]> end action %d\n", time(NULL), e->end);
   }

   pthread_mutex_unlock(&m);

   return NULL;
}
