#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "monitor.h"
#include "so.h"

struct mon_screensaver_entry {
   void *(*callback)(void *);
   void *args;
   int flags;
   struct mon_screensaver_entry *next;
   struct mon_screensaver_entry *prev;
};

static void *mon_screensaver_run(void *args);
static void mon_screensaver_dump(void);

static pthread_t t;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static struct mon_screensaver_entry *list = NULL;
static int running = 0;

static void *mon_screensaver_run(void *args)
{
   struct mon_screensaver_entry *p, *e;
   int active;
   pthread_t ct;
   FILE *pp = NULL;
   char buf[64];

   while(1) {
      pthread_mutex_lock(&m);

      if(!list) {
         running = 0;
         pthread_mutex_unlock(&m);
         return NULL;
      }

      /* TODO XXX riscrivere con i segnali dbus e la libreria */
      do {
         active = -1;
         if(!(pp = popen(SO"dbus-send --dest=org.gnome.ScreenSaver --print-reply /org/gnome/ScreenSaver org.gnome.ScreenSaver.GetActive", "r"))) break;
         while(fgets(buf, sizeof(buf), pp)) {
            if(strstr(buf, SO"boolean true")) {
               active = 1;
               break;
            } else if(strstr(buf, SO"boolean false")) {
               active = 0;
               break;
            }
         }
      } while(0);
      if(pp) { pclose(pp); pp = NULL; }
 
      do {
         if(active != -1) break;
         if(!(pp = popen(SO"dbus-send --dest=org.freedesktop.ScreenSaver --print-reply /ScreenSaver org.freedesktop.ScreenSaver.GetActive", "r"))) break;
         while(fgets(buf, sizeof(buf), pp)) {
            if(strstr(buf, SO"boolean true")) {
               active = 1;
               break;
            } else if(strstr(buf, SO"boolean false")) {
               active = 0;
               break;
            }
         }
      } while(0);
      if(pp) { pclose(pp); pp = NULL; }

      if(active != -1) {
         p = list;
         while(p) {
            if((!(p->flags & MON_FLAG_NEG) && active) || ((p->flags & MON_FLAG_NEG) && !active)) {
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
      }

      mon_screensaver_dump();

      pthread_mutex_unlock(&m);

      sleep(1);
   }
}

void mon_screensaver_start(void *args) {
   pthread_mutex_lock(&m);
   if(!running) if(!pthread_create(&t, NULL, mon_screensaver_run, NULL)) running = 1;
   pthread_mutex_unlock(&m);
}

void *mon_screensaver_addwatch(void *(*callback)(void *), void *args, int flags, ...)
{
   struct mon_screensaver_entry *p = NULL;

   pthread_mutex_lock(&m);

   do {
      if(!list) {
         if(!(list = malloc(sizeof(struct mon_screensaver_entry)))) break;
         list->prev = NULL;
         p = list;
      } else {
         for(p = list; p->next; p = p->next);
         if(!(p->next = malloc(sizeof(struct mon_screensaver_entry)))) break;
         p->next->prev = p;
         p = p->next;
      }
      p->next = NULL;
      p->callback = callback;
      p->args = args;
      p->flags = flags;

      if(!running) if(!pthread_create(&t, NULL, mon_screensaver_run, NULL)) running = 1;
   } while(0);

   mon_screensaver_dump();

   pthread_mutex_unlock(&m);

   return p;
}

void mon_screensaver_delwatch(void *entry)
{
   struct mon_screensaver_entry *p = NULL, *e = (struct mon_screensaver_entry *)entry;

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

void mon_screensaver_dump(void)
{
#if 0
   struct mon_screensaver_entry *p = NULL;

   debugme("\n--- SCREENSAVER MONITOR LIST ---\n");
   for(p = list; p; p = p->next) debugme("- (at %p)\n", (void *)p);
   debugme("\n");
#endif

   return;
}
