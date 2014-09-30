#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "so.h"

char *clonefile(const char *src)
{
   int in = -1, out = -1, len = 0;
   char *ret = NULL, *tmp = NULL, buf[4096];

   do {
      if(asprintf(&tmp, "%s", SO"/tmp/tmpXXXXXX") == -1) break;
      if(((out = mkstemp(tmp)) == -1) || ((in = open(src, O_RDONLY)) == -1)) break;
      do {
         if((len = read(in, buf, sizeof(buf))) > 0) write(out, buf, len);
      } while(len > 0);
      if(!len) {
         ret = tmp;
         tmp = NULL;
      }
   } while(0);
   if(in != -1) close(in);
   if(out != -1) close(out);
   if(tmp) {
      unlink(tmp);
      free(tmp);
   }

   return ret;
}

int timemark(unsigned int index, time_t *begin, time_t *end)
{
   int ret = -1;
   char filename[16];
   FILE *fp = NULL;
   time_t b = 0, e = 0;

   do {
      if(snprintf(filename, sizeof(filename), SO".tmp.%03u.t", index) >= sizeof(filename)) break;
      e = time(NULL) - 30;
      if(!(fp = fopen(filename, "r+")) && !(fp = fopen(filename, "w+"))) break;
      if(fscanf(fp, "%ld\n", &b) != 1) b = 0;
      rewind(fp);
      if(fprintf(fp, "%ld\n", e) <= 0) break;
      *begin = b;
      *end = e - 1;
      ret = 0;
   } while(0);
   if(fp) fclose(fp);

   return ret;
}

int hashmark(unsigned int index, const char *hash)
{
   int ret = -1;
   char filename[16], chunk[20];
   int fd = -1;

   do {
      if(snprintf(filename, sizeof(filename), SO".tmp.%03u.h", index) >= sizeof(filename)) break;
      if((fd = open(filename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) == -1) break;
      while((ret = read(fd, chunk, sizeof(chunk))) == sizeof(chunk)) if(!memcmp(hash, chunk, sizeof(chunk))) break;
      if(ret == sizeof(chunk)) {
         ret = 1;
      } else if(ret) {
         ret = -1;
      } else {
         write(fd, hash, sizeof(chunk));
      }
   } while(0);
   if(fd != -1) close(fd);

   return ret;
}
