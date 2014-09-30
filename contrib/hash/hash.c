#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define SO

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

int main(int argc, char *argv[])
{
   printf("%d\n", hashmark(3, argv[1]));

   return 0;
}
