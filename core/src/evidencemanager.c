#define _BSD_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <string.h>
#include <unistd.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "config.h"
#include "params.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"

#define EVIDENCE_VERSION 2008121901U

typedef struct {
   unsigned int version;
   unsigned int type;
   unsigned int htimestamp;
   unsigned int ltimestamp;
   unsigned int hostlen;
   unsigned int userlen;
   unsigned int sourcelen;
   unsigned int additionallen;
} __attribute__((packed)) evidence_header_t;

BIO *evidence_open(int type, char *additional, long additionallen)
{
   BIO *evidence = NULL, *cipher = NULL;
   EVP_CIPHER_CTX *ctx = NULL;
   unsigned char iv[AES_BLOCK_SIZE] = {0};
   int structlen, headerlen, paddinglen;
   unsigned long long ts;
   evidence_header_t eh;
   char *filename = NULL;
   struct timeval tv;

   do {
      ts = ((unsigned long long)time(NULL) + 11644473600LL) * 10000000LL;

      eh.version = EVIDENCE_VERSION;
      eh.type = type;
      eh.htimestamp = (unsigned int)(ts>>32);
      eh.ltimestamp = (unsigned int)(ts&0xffffffff);
      eh.hostlen = 0;
      eh.userlen = 0;
      eh.sourcelen = 0;
      eh.additionallen = (int)additionallen;

      structlen = sizeof(eh) + eh.additionallen;
      paddinglen = (structlen % 16) ? (16 - (structlen % 16)) : 0;
      headerlen = structlen + paddinglen;

      gettimeofday(&tv, NULL);
      asprintf(&filename, SO".tmp-%010u-%06u-XXXXXX", (unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec);
      if(!(evidence = BIO_new_fp(fdopen(mkstemp(filename), "w"), BIO_CLOSE))) break;
      if(!(cipher = BIO_new(BIO_f_cipher()))) break;
      BIO_set_cipher(cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), bps.evidencekey, iv, 1);
      BIO_get_cipher_ctx(cipher, &ctx);
      EVP_CIPHER_CTX_set_padding(ctx, 0);
      BIO_push(cipher, evidence);

      if(BIO_write(evidence, &headerlen, sizeof(headerlen)) != sizeof(headerlen)) break;
      if(BIO_write(cipher, &eh, sizeof(eh)) != sizeof(eh)) break;
      if(additional) if(BIO_write(cipher, additional, additionallen) != additionallen) break;
      while(paddinglen--) BIO_write(cipher, "\0", 1);

      BIO_flush(cipher);
      BIO_pop(cipher);
   } while(0);

   if(cipher) BIO_free(cipher);
   if(filename) free(filename);

   return evidence;
}

int evidence_add(BIO *evidence, char *data, long datalen)
{
   BIO *cipher = NULL;
   EVP_CIPHER_CTX *ctx = NULL;
   unsigned char iv[AES_BLOCK_SIZE] = {0};
   int paddinglen, ret = -1;

   do {
      if(!datalen) break;

      if(!(cipher = BIO_new(BIO_f_cipher()))) break;
      BIO_set_cipher(cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), bps.evidencekey, iv, 1);
      BIO_get_cipher_ctx(cipher, &ctx);
      EVP_CIPHER_CTX_set_padding(ctx, 0);
      BIO_push(cipher, evidence);

      paddinglen = (datalen % 16) ? (16 - (datalen % 16)) : 0;

      if(BIO_puti32(evidence, datalen) == -1) break;
      BIO_flush(evidence);
      if(BIO_write(cipher, data, datalen) != datalen) break;
      while(paddinglen--) BIO_write(cipher, "\0", 1);

      BIO_flush(cipher);
      BIO_pop(cipher);

      ret = 0;
   } while(0);

   if(cipher) BIO_free(cipher);

   return ret;
}

int evidence_close(BIO *evidence)
{
   FILE *fp = NULL;

   BIO_flush(evidence);
   BIO_get_fp(evidence, &fp);
   fchmod(fileno(fp), S_IRUSR | S_IWUSR | S_ISVTX);
   BIO_free(evidence);

   return 0;
}

int evidence_write(int type, char *additional, long additionallen, char *data, long datalen)
{
   BIO *evidence = NULL;

   evidence = evidence_open(type, additional, additionallen);
   evidence_add(evidence, data, datalen);
   evidence_close(evidence);

   return 0;
}
