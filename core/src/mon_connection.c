#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

#include "monitor.h"
#include "so.h"

struct mon_connection_entry {
   void *(*callback)(void *);
   void *args;
   int flags;
   in_addr_t ip;
   in_addr_t mask;
   in_port_t port;
   struct mon_connection_entry *next;
   struct mon_connection_entry *prev;
};

struct connection {
   in_addr_t ip;
   in_port_t port;
   struct connection *next;
   struct connection *prev;
};

static void *mon_connection_run(void *args);
static void mon_connection_dump(void);

static int connection_add(struct connection **l, in_addr_t ip, in_port_t port);
static void connection_free(struct connection **l);
static int connection_find(struct connection *l, in_addr_t ip, in_addr_t mask, in_port_t port);

static pthread_t t;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static struct mon_connection_entry *list = NULL;
static int running = 0;

static void *mon_connection_run(void *args)
{
   struct mon_connection_entry *p, *e;
   struct connection *current = NULL;
   FILE *fp;
   char buf[256];
   in_addr_t ip, dummy;
   in_port_t port;
   pthread_t ct;

   while(1) {
      pthread_mutex_lock(&m);

      if(!list) {
         running = 0;
         pthread_mutex_unlock(&m);
         return NULL;
      }

      connection_free(&current);

      if(!(fp = fopen(SO"/proc/net/tcp", "r"))) continue;
      while(fgets(buf, sizeof(buf), fp)) {
         if(sscanf(buf, " %*u: %*x:%*x %x:%hx 01 %x", &ip, &port, &dummy) == 3) connection_add(&current, ip, port);
      }
      fclose(fp);

      p = list;
      while(p) {
         if((!(p->flags & MON_FLAG_NEG) && connection_find(current, p->ip, p->mask, p->port)) || ((p->flags & MON_FLAG_NEG) && !connection_find(current, p->ip, p->mask, p->port))) {
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

      mon_connection_dump();

      pthread_mutex_unlock(&m);

      sleep(1);
   }
}

void mon_connection_start(void *args) {
   pthread_mutex_lock(&m);
   if(!running) if(!pthread_create(&t, NULL, mon_connection_run, NULL)) running = 1;
   pthread_mutex_unlock(&m);
}

void *mon_connection_addwatch(void *(*callback)(void *), void *args, int flags, ...)
{
   struct mon_connection_entry *p = NULL;
   va_list va;

   pthread_mutex_lock(&m);

   do {
      if(!list) {
         if(!(list = malloc(sizeof(struct mon_connection_entry)))) break;
         list->prev = NULL;
         p = list;
      } else {
         for(p = list; p->next; p = p->next);
         if(!(p->next = malloc(sizeof(struct mon_connection_entry)))) break;
         p->next->prev = p;
         p = p->next;
      }
      p->next = NULL;
      p->callback = callback;
      p->args = args;
      p->flags = flags;

      va_start(va, flags);
      p->ip = va_arg(va, in_addr_t);
      p->mask = va_arg(va, in_addr_t);
      p->port = (in_port_t)va_arg(va, int);
      va_end(va);

      if(!running) if(!pthread_create(&t, NULL, mon_connection_run, NULL)) running = 1;
   } while(0);

   mon_connection_dump();

   pthread_mutex_unlock(&m);

   return p;
}

void mon_connection_delwatch(void *entry)
{
   struct mon_connection_entry *p = NULL, *e = (struct mon_connection_entry *)entry;

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

void mon_connection_dump(void)
{
#if 0
   struct mon_connection_entry *p = NULL;

   debugme("\n--- CONNECTION MONITOR LIST ---\n");
   for(p = list; p; p = p->next) debugme("- %u %u (at %p)\n", (unsigned int)p->ip, p->port, (void *)p);
   debugme("\n");
#endif

   return;
}

static int connection_add(struct connection **l, in_addr_t ip, in_port_t port)
{
   struct connection *p;

   if(!*l) {
      if(!(*l = malloc(sizeof(struct connection)))) return -1;
      (*l)->prev = NULL;
      p = *l;
   } else {
      for(p = *l; p->next; p = p->next) if((p->ip == ip) && (p->port == port)) return 0;
      if((p->ip == ip) && (p->port == port)) return 0;
      if(!(p->next = malloc(sizeof(struct connection)))) return -1;
      p->next->prev = p;
      p = p->next;
   }
   p->next = NULL;
   p->ip = ip;
   p->port = port;

   return 1;
}

static void connection_free(struct connection **l)
{
   struct connection *p;

   while(*l) {
      p = (*l)->next;
      free(*l);
      *l = p;
   }

   return;
}

static int connection_find(struct connection *l, in_addr_t ip, in_addr_t mask, in_port_t port)
{
   for(; l; l = l->next) {
      if(((l->ip & mask) == (ip & mask)) && (!port || (l->port == port))) return 1;
   }

   return 0;
}
