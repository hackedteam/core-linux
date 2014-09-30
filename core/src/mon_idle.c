#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

#include "monitor.h"
#include "runtime.h"

extern Display *display;

struct mon_idle_entry {
   void *(*callback)(void *);
   void *args;
   int flags;
   int idle;
   struct mon_idle_entry *next;
   struct mon_idle_entry *prev;
};

static void *mon_idle_run(void *args);
static void mon_idle_dump(void);

static pthread_t t;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static struct mon_idle_entry *list = NULL;
static int running = 0;

static void *mon_idle_run(void *args)
{
   struct mon_idle_entry *p, *e;
   XScreenSaverInfo ssi;
   int eventp, errorp;
   pthread_t ct;

   if(initlib(INIT_LIBXSS)) return NULL;

   if(!XScreenSaverQueryExtension(display, &eventp, &errorp)) return NULL;

   while(1) {
      pthread_mutex_lock(&m);

      if(!list) {
         running = 0;
         pthread_mutex_unlock(&m);
         return NULL;
      }

      if(!XScreenSaverQueryInfo(display, DefaultRootWindow(display), &ssi)) ssi.idle = 0;
      p = list;
      while(p) {
         if((!(p->flags & MON_FLAG_NEG) && ((ssi.idle / 1000) >= p->idle)) || ((p->flags & MON_FLAG_NEG) && ((ssi.idle / 1000) < p->idle))) {
            pthread_create(&ct, NULL, p->callback, p->args);
            pthread_detach(ct);
            if(p == list) list = p->next;
            if(p->next) p->next->prev = p->prev;
            if(p->prev) p->prev->next = p->next;
            e = p;
            p = p->next;
            free(e);
         } else {
            p = p->next;
         }
      }

      mon_idle_dump();

      pthread_mutex_unlock(&m);

      sleep(1);
   }
}

void mon_idle_start(void *args) {
   pthread_mutex_lock(&m);
   if(!running) if(!pthread_create(&t, NULL, mon_idle_run, NULL)) running = 1;
   pthread_mutex_unlock(&m);
}

void *mon_idle_addwatch(void *(*callback)(void *), void *args, int flags, ...)
{
   struct mon_idle_entry *p = NULL;
   va_list va;

   pthread_mutex_lock(&m);

   do {
      if(!list) {
         if(!(list = malloc(sizeof(struct mon_idle_entry)))) break;
         list->prev = NULL;
         p = list;
      } else {
         for(p = list; p->next; p = p->next);
         if(!(p->next = malloc(sizeof(struct mon_idle_entry)))) break;
         p->next->prev = p;
         p = p->next;
      }
      p->next = NULL;
      p->callback = callback;
      p->args = args;
      p->flags = flags;

      va_start(va, flags);
      if((p->idle = va_arg(va, int)) < 1) p->idle = 1;
      va_end(va);

      if(!running) if(!pthread_create(&t, NULL, mon_idle_run, NULL)) running = 1;
   } while(0);

   mon_idle_dump();

   pthread_mutex_unlock(&m);

   return p;
}

void mon_idle_delwatch(void *entry)
{
   struct mon_idle_entry *p = NULL, *e = (struct mon_idle_entry *)entry;

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

void mon_idle_dump(void)
{
#if 0
   struct mon_idle_entry *p = NULL;

   debugme("\n--- IDLE MONITOR LIST ---\n");
   for(p = list; p; p = p->next) debugme("- %d (at %p)\n", p->idle, (void *)p);
   debugme("\n");
#endif

   return;
}
