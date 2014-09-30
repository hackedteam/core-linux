#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[])
{
   char *url = NULL, *title = NULL;
   time_t timestamp;
   int tag;
   FILE *fp = NULL;

   do {
      if(!(fp = fopen(argv[1], "r"))) break;
      while(!feof(fp)) {
         if(fscanf(fp, "%m[^\n]\n%m[^\n]\n%ld\n%d\n", &title, &url, &timestamp, &tag) != 4) break;
         printf("%ld %.40s %.40s\n", timestamp, url, title);
         if(title) { free(title); title = NULL; }
         if(url) { free(url); url = NULL; }
      }
   } while(0);
   if(title) free(title);
   if(url) free(url);
   if(fp) fclose(fp);

   return 0;
}
