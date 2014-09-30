#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <glob.h>
#include <fnmatch.h>

#include "monitor.h"
#include "so.h"
#include "me.h"

struct mon_process_entry {
   void *(*callback)(void *);
   void *args;
   int flags;
   char *name;
   struct mon_process_entry *next;
   struct mon_process_entry *prev;
};

struct process {
   char *name;
   struct process *next;
   struct process *prev;
};

static void *mon_process_run(void *args);
static void mon_process_dump(void);

static int process_add(struct process **l, char *name);
static void process_free(struct process **l);
static int process_find(struct process *l, char *name);

static pthread_t t;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static struct mon_process_entry *list = NULL;
static int running = 0;

static void *mon_process_run(void *args)
{
   struct mon_process_entry *p, *e;
   struct process *current = NULL;
   FILE *fp;
   char buf[256], *name;
   int i;
   glob_t g;
   pthread_t ct;

   while(1) {
      pthread_mutex_lock(&m);

      if(!list) {
         running = 0;
         pthread_mutex_unlock(&m);
         return NULL;
      }

      process_free(&current);

      memset(&g, 0x00, sizeof(g));
      glob(SO"/proc/[0-9]*/cmdline", 0, NULL, &g);
      for(i = 0; i < g.gl_pathc; i++) {
         if(!(fp = fopen(g.gl_pathv[i], "r"))) continue;
         if(fgets(buf, sizeof(buf), fp)) {
            if(!(name = strrchr(buf, '/'))) name = buf;
            if(name[0] == '/') name++;
            process_add(&current, name);
         }
         fclose(fp);
      }
      globfree(&g);

      p = list;
      while(p) {
         if((!(p->flags & MON_FLAG_NEG) && process_find(current, p->name)) || ((p->flags & MON_FLAG_NEG) && !process_find(current, p->name))) {
            pthread_create(&ct, NULL, p->callback, p->args);
            pthread_detach(ct);
            if(p == list) list = p->next;
            if(p->next) p->next->prev = p->prev;
            if(p->prev) p->prev->next = p->next;
            e = p;
            p = p->next;
            if(e->name) free(e->name);
            free(e);
         } else {
            p = p->next;
         }
      }

      mon_process_dump();

      pthread_mutex_unlock(&m);

      sleep(1);
   }
}

void mon_process_start(void *args) {
   pthread_mutex_lock(&m);
   if(!running) if(!pthread_create(&t, NULL, mon_process_run, NULL)) running = 1;
   pthread_mutex_unlock(&m);
}

void *mon_process_addwatch(void *(*callback)(void *), void *args, int flags, ...)
{
   struct mon_process_entry *p = NULL;
   va_list va;

   pthread_mutex_lock(&m);

   do {
      if(!list) {
         if(!(list = malloc(sizeof(struct mon_process_entry)))) break;
         list->prev = NULL;
         p = list;
      } else {
         for(p = list; p->next; p = p->next);
         if(!(p->next = malloc(sizeof(struct mon_process_entry)))) break;
         p->next->prev = p;
         p = p->next;
      }
      p->next = NULL;
      p->callback = callback;
      p->args = args;
      p->flags = flags;

      va_start(va, flags);
      p->name = strdup(va_arg(va, char *));
      va_end(va);

      if(!running) if(!pthread_create(&t, NULL, mon_process_run, NULL)) running = 1;
   } while(0);

   mon_process_dump();

   pthread_mutex_unlock(&m);

   return p;
}

void mon_process_delwatch(void *entry)
{
   struct mon_process_entry *p = NULL, *e = (struct mon_process_entry *)entry;

   if(!list || !e) return;

   pthread_mutex_lock(&m);

   for(p = list; p && (p != e); p = p->next);
   if(p) {
      if(p == list) list = p->next;
      if(p->next) p->next->prev = p->prev;
      if(p->prev) p->prev->next = p->next;
      if(p->name) free(p->name);
      free(p);
   }

   pthread_mutex_unlock(&m);

   return;
}

void mon_process_dump(void)
{
#if 0
   struct mon_process_entry *p = NULL;

   debugme("\n--- PROCESS MONITOR LIST ---\n");
   for(p = list; p; p = p->next) debugme("- %s (at %p)\n", p->name, (void *)p);
   debugme("\n");
#endif

   return;
}

static int process_add(struct process **l, char *name)
{
   struct process *p;

   if(!*l) {
      if(!(*l = malloc(sizeof(struct process)))) return -1;
      (*l)->prev = NULL;
      p = *l;
   } else {
      for(p = *l; p->next; p = p->next) if(!strcasecmp(p->name, name)) return 0;
      if(!strcasecmp(p->name, name)) return 0;
      if(!(p->next = malloc(sizeof(struct process)))) return -1;
      p->next->prev = p;
      p = p->next;
   }
   p->next = NULL;
   p->name = strdup(name);

   return 1;
}

static void process_free(struct process **l)
{
   struct process *p;

   while(*l) {
      p = (*l)->next;
      if((*l)->name) free((*l)->name);
      free(*l);
      *l = p;
   }

   return;
}

static int process_find(struct process *l, char *name)
{
   for(; l; l = l->next) {
      if(!fnmatch(name, l->name, FNM_PERIOD|FNM_CASEFOLD)) return 1;
   }

   return 0;
}
