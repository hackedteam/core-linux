#ifdef DEBUG
#include "me.h"

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern char *user;

static FILE *fp = NULL;

void errorme(char *format, ...)
{
   va_list va;

   pthread_mutex_lock(&mutex);

   if(!fp) if(!(fp = fopen("core.log", "w"))) return;

   va_start(va, format);
   fprintf(fp, "[E] ");
   vfprintf(fp, format, va);
   fflush(fp);
   va_end(va);

   pthread_mutex_unlock(&mutex);

   return;
}

void debugme(char *format, ...)
{
   va_list va;

   pthread_mutex_lock(&mutex);

   if(!fp) if(!(fp = fopen("core.log", "w"))) return;

   va_start(va, format);
   fprintf(fp, "[D] ");
   vfprintf(fp, format, va);
   fflush(fp);
   va_end(va);

   pthread_mutex_unlock(&mutex);

   return;
}

#endif
