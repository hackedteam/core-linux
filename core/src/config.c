#define _BSD_SOURCE
#define _XOPEN_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <json.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "params.h"
#include "eventmanager.h"
#include "actionmanager.h"
#include "module.h"
#include "so.h"
#include "runtime.h"

#if 1
#   define ABORT(x) { break; }
#else
#  define ABORT(x) { debugme(x "\n"); break; }
#endif

int parseconfig(char *filename)
{
   json_object *config = NULL;
   BIO *bio_file = NULL, *bio_cipher = NULL, *bio_mem = NULL, *bio_null = NULL, *bio_md = NULL;
   char buf[1024], *ptr = NULL, sha[SHA_DIGEST_LENGTH];
   unsigned char iv[AES_BLOCK_SIZE];
   int len, ret = -1;

   do {
#if 1
      memset(iv, 0x00, sizeof(iv));
      if(!(bio_file = BIO_new_file(filename, "r"))) ABORT("Unable to open the file");
      if(!(bio_cipher = BIO_new(BIO_f_cipher()))) ABORT("Unable to initialize bio_cipher");
      BIO_set_cipher(bio_cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), bps.confkey, iv, 0);
      BIO_push(bio_cipher, bio_file);

      if(!(bio_mem = BIO_new(BIO_s_mem()))) ABORT("Unable to initialize bio_mem");
      do {
         if((len = BIO_read(bio_cipher, buf, sizeof(buf))) > 0) BIO_write(bio_mem, buf, len);
      } while(len == sizeof(buf));
      if(len < 0) ABORT("Error reading the file");

      bio_null = BIO_new(BIO_s_null());
      bio_md = BIO_new(BIO_f_md());
      if(!BIO_set_md(bio_md, EVP_sha1())) ABORT("Unable to initialize bio_md");
      BIO_push(bio_md, bio_null);

      if((len = BIO_get_mem_data(bio_mem, &ptr)) <= sizeof(sha)) ABORT("Invalid configuration file");
      if(BIO_write(bio_md, ptr, len - sizeof(sha)) != len - sizeof(sha)) ABORT("Error writing to bio_md");
      if(BIO_gets(bio_md, sha, sizeof(sha)) != sizeof(sha)) ABORT("Error reading from bio_md");
      if(memcmp(sha, &ptr[len - sizeof(sha)], sizeof(sha))) ABORT("Invalid checksum");

      ptr[len - sizeof(sha)] = '\0';
      config = json_tokener_parse(ptr);
#else
      config = json_object_from_file(filename);
#endif
      em_parseevents(config);
      am_parseactions(config);
      parsemodules(config);

      ret = 0;
   } while(0);

   if(bio_file) BIO_free(bio_file);
   if(bio_cipher) BIO_free(bio_cipher);
   if(bio_mem) BIO_free(bio_mem);
   if(bio_null) BIO_free(bio_null);
   if(bio_md) BIO_free(bio_md);
   if(config) json_object_put(config);

   return ret;
}

int get_int(json_object *json, char *key, int *val)
{
   json_object *o;

   if(!(o = json_object_object_get(json, key)) || !json_object_is_type(o, json_type_int)) {
      *val = -1;
      return -1;
   }

   *val = json_object_get_int(o);

   return 0;
}

int get_double(json_object *json, char *key, int *val)
{
   json_object *o;

   if(!(o = json_object_object_get(json, key)) || !json_object_is_type(o, json_type_double)) {
      *val = -1;
      return -1;
   }

   *val = json_object_get_double(o);

   return 0;
}

int get_string(json_object *json, char *key, char **val)
{
   json_object *o;

   if(!(o = json_object_object_get(json, key)) || !json_object_is_type(o, json_type_string)) return -1;

   *val = (char *)json_object_get_string(o);

   return 0;
}

int get_boolean(json_object *json, char *key, int *val)
{
   json_object *o;

   if(!(o = json_object_object_get(json, key)) || !json_object_is_type(o, json_type_boolean)) return -1;

   *val = json_object_get_boolean(o);

   return 0;
}
