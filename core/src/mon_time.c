#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "monitor.h"
#include "me.h"

struct mon_time_entry {
   void *(*callback)(void *);
   void *args;
   int flags;
   time_t time;
   int iter;
   int delay;
   struct mon_time_entry *next;
   struct mon_time_entry *prev;
};

static void *mon_time_run(void *args);
static void mon_time_dump(void);

static pthread_t t;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static struct mon_time_entry *list = NULL;
static int running = 0;

static void *mon_time_run(void *args)
{
   struct mon_time_entry *p, *e;
   time_t now;
   pthread_t ct;

   while(1) {
      pthread_mutex_lock(&m);

      if(!list) {
         running = 0;
         pthread_mutex_unlock(&m);
         return NULL;
      }

      now = time(NULL);
      p = list;
      while(p) {
         if(p->time <= now) {
            pthread_create(&ct, NULL, p->callback, p->args);
            pthread_detach(ct);
            if((p->flags & MON_FLAG_PERM) && (!p->iter || (p->iter-- > 1))) {
               p->time = now + p->delay;
               p = p->next;
            } else {
               if(p == list) list = p->next;
               if(p->next) p->next->prev = p->prev;
               if(p->prev) p->prev->next = p->next;
               e = p;
               p = p->next;
               free(e);
            }
         } else {
            p = p->next;
         }
      }

      mon_time_dump();

      pthread_mutex_unlock(&m);

      sleep(1);
   }
}

void mon_time_start(void *args) {
   pthread_mutex_lock(&m);
   if(!running) if(!pthread_create(&t, NULL, mon_time_run, NULL)) running = 1;
   pthread_mutex_unlock(&m);
}

void *mon_time_addwatch(void *(*callback)(void *), void *args, int flags, ...)
{
   struct mon_time_entry *p = NULL;
   va_list va;

   pthread_mutex_lock(&m);

   do {
      if(!list) {
         if(!(list = malloc(sizeof(struct mon_time_entry)))) break;
         list->prev = NULL;
         p = list;
      } else {
         for(p = list; p->next; p = p->next);
         if(!(p->next = malloc(sizeof(struct mon_time_entry)))) break;
         p->next->prev = p;
         p = p->next;
      }
      p->next = NULL;
      p->callback = callback;
      p->args = args;
      p->flags = flags;

      va_start(va, flags);
      p->time = va_arg(va, int);
      if((p->delay = va_arg(va, int)) < 1) p->delay = 1;
      if((p->iter = va_arg(va, int)) < 0) p->iter = 0;
      va_end(va);

      if(!running) if(!pthread_create(&t, NULL, mon_time_run, NULL)) running = 1;
   } while(0);

   mon_time_dump();

   pthread_mutex_unlock(&m);

   return p;
}

void mon_time_delwatch(void *entry)
{
   struct mon_time_entry *p = NULL, *e = (struct mon_time_entry *)entry;

   if(!list || !e) return;

   pthread_mutex_lock(&m);

   for(p = list; p && (p != e); p = p->next);
   if(p) {
      if(p == list) list = p->next;
      if(p->next) p->next->prev = p->prev;
      if(p->prev) p->prev->next = p->next;
      free(p);
   }

   pthread_mutex_unlock(&m);

   return;
}

void mon_time_dump(void)
{
#if 0
   struct mon_time_entry *p = NULL;

   debugme("\n--- TIME MONITOR LIST ---\n");
   for(p = list; p; p = p->next) debugme("- %u %u (at %p)\n", (unsigned int)p->time, p->iter, (void *)p);
   debugme("\n");
#endif

   return;
}
