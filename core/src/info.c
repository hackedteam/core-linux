#include <openssl/bio.h>

#include "evidencemanager.h"
#include "bioutils.h"
#include "me.h"

void info(const char *msg)
{
   BIO *bio_data = NULL;
   char *dataptr = NULL;
   long datalen;

   do {
      if(!msg || !msg[0]) break;
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      BIO_puts16n(bio_data, msg);
      if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;
      debugme("INFO %s\n", msg);
      evidence_write(EVIDENCE_TYPE_INFO, NULL, 0, dataptr, datalen);
   } while(0);
   if(bio_data) BIO_free(bio_data);

   return;
}
