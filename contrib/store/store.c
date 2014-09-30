#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int storeput(const char *store, const char *name, const char *value);
int storeget(const char *store, const char *name, char **value);

int storeput(const char *store, const char *name, const char *value)
{
   int ret = -1;
   FILE *fp = NULL;

   do {
      if(!(fp = fopen(store, "w"))) break;
      if(fputs(name, fp) == EOF) break;
      if(fputs("=", fp) == EOF) break;
      if(fputs(value, fp) == EOF) break;
      if(fputs("\n", fp) == EOF) break;
      ret = 0;
   } while(0);
   if(fp) fclose(fp);

   return ret;
}

int storeget(const char *store, const char *name, char **value)
{
   int ret = -1;
   FILE *fp = NULL;
   char *rname = NULL, *rvalue = NULL;

   do {
      if(!(fp = fopen(store, "r"))) break;
      while(!feof(fp)) {
         if(rname) { free(rname); rname = NULL; }
         if(rvalue) { free(rvalue); rvalue = NULL; }
         if(fscanf(fp, "%m[^=]=%m[^\n]\n", &rname, &rvalue) != 2) continue;
         if(strcmp(rname, name)) continue;
         *value = strdup(rvalue);
         ret = 0;
         break;
      }
   } while(0);
   if(fp) fclose(fp);
   if(rname) free(rname);
   if(rvalue) free(rvalue);

   return ret;
}

int main(void)
{
   char *val = NULL;
   //storeput("prova", "uno", "due");

   storeget("prova", "tre", &val);
   printf("%s\n", val);

   return 0;
}
