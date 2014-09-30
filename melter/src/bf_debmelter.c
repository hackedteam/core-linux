#include <string.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include "bf_debmelter.h"

static int debmelterf_write(BIO *b, const char *buf, int size);
static int debmelterf_read(BIO *b, char *buf, int size);
static long debmelterf_ctrl(BIO *b, int cmd, long num, void *ptr);
static int debmelterf_new(BIO *b);
static int debmelterf_free(BIO *b);
static long debmelterf_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp);

static int step_init(BIO *b, BIO_F_DEBMELTER_CTX *ctx);
static int step_entry_header(BIO *b, BIO_F_DEBMELTER_CTX *ctx, ENTRY_HEADER *header)
static int step_entry_melt(BIO *b, BIO_F_DEBMELTER_CTX *ctx, ENTRY_HEADER *header)
static int step_entry_data(BIO *b, BIO_F_DEBMELTER_CTX *ctx, ENTRY_HEADER *header)
static int step_passthru(BIO *b, BIO_F_DEBMELTER_CTX *ctx);

typedef struct bio_f_debmelterf_ctx {
   BUF_MEM *buf;
   char *ptr;
   char *filename;
   int step;
   int size;
} BIO_F_DEBMELTER_CTX;

#define AR_MAGIC "!<arch>\n"
#define ENTRY_MAGIC "`\n"
#define CONTROL_FILENAME "control.tar.gz  "

typedef struct entry_header {
   char filename[16];
   char mtime[12];
   char uid[6];
   char gid[6];
   char mode[8];
   char size[10];
   char magic[2];
} __attribute__((packed)) ENTRY_HEADER;

static BIO_METHOD methods_debmelterf = { BIO_TYPE_DEBMELTER,
                                         "debmelter filter",
                                         debmelterf_write,
                                         debmelterf_read,
                                         NULL,
                                         NULL,
                                         debmelterf_ctrl,
                                         debmelterf_new,
                                         debmelterf_free,
                                         debmelterf_callback_ctrl,
                                       };

BIO_METHOD *BIO_f_debmelter(void)
{
   return(&methods_debmelterf);
}

BIO *BIO_new_debmelter(const char *filename)
{
   BIO *ret = NULL;

   do {
      if(!(ret = BIO_new(BIO_f_debmelter()))) break;
      BIO_set_debmelter_filename(ret, filename);
   } while(0);

   return ret;
}

static int debmelterf_write(BIO *b, const char *buf, int size)
{
   int ret = 0, count = 0;
   BIO_F_DEBMELTER_CTX *ctx;
   ENTRY_HEADER header = NULL;

   do {
      if(!buf || !b->next_bio || (size <= 0)) break;

      ctx = (BIO_F_DEBMELTER_CTX *)b->ptr;

      if(ctx->step == BIO_F_DEBMELTER_STEP_PASSTHRU) {
         ret = BIO_write(b->next_bio, buf, size);
         break;
      }

      if(BUF_MEM_grow(ctx->buf, ctx->buf->length + size) != (ctx->buf->length + size)) break;
      memcpy(ctx->buf->data + ctx->buf->length, buf, size);

      if((ctx->step == BIO_F_DEBMELTER_STEP_INIT) && (ctx->buf->length >= strlen(AR_MAGIC))) {
         if(memcmp(ctx->p, AR_MAGIC, strlen(AR_MAGIC)))

      while(ctx->buf->length && (ret != -1)) {
         if((ctx->step == BIO_F_DEBMELTER_STEP_INIT) && (ctx->buf->length >= strlen(AR_MAGIC))) {
            ret = step_init(b, ctx);
         } else if((ctx->step == BIO_F_DEBMELTER_STEP_ENTRY_HEADER) && (ctx->buf->length >= sizeof(ENTRY_HEADER))) {
            ret = step_entry_header(b, ctx, &header);
         } else if(ctx->step == BIO_F_DEBMELTER_STEP_ENTRY_MELT) {
            ret = step_entry_melt(b, ctx, header);
         } else if(ctx->step == BIO_F_DEBMELTER_STEP_ENTRY_DATA) {
            ret = step_entry_data(b, ctx, header);
         } else if(ctx->step == BIO_F_DEBMELTER_STEP_PASSTHRU) {
            ret = step_passthru(b, ctx);
         } else {
            break;
         }
         count += ret;
      }
   } while(0);
   if(b && b->next_bio) {
      BIO_clear_retry_flags(b);
      BIO_copy_next_retry(b);
   }

   return ((ret == -1) ? -1 : count);
}

static int debmelterf_read(BIO *b, char *buf, int size)
{
   int ret = 0;

   do {
      if(!buf || !b->next_bio) break;
      ret = BIO_read(b->next_bio, buf, size);
      BIO_clear_retry_flags(b);
      BIO_copy_next_retry(b);
   } while(0);

   return ret;
}

static long debmelterf_ctrl(BIO *b, int cmd, long num, void *ptr)
{
   long ret = 0;
   const char **pptr = NULL;
   BIO_F_DEBMELTER_CTX *ctx;

   do {
      if(!b || !b->init || !b->ptr) break;

      ctx = (BIO_F_DEBMELTER_CTX *)b->ptr;

      switch(cmd) {
         case BIO_CTRL_GET_DEBMELTER_FILENAME:
            pptr = (const char **)ptr;
            *pptr = ctx->filename;
            ret = 1;
            break;
         case BIO_CTRL_SET_DEBMELTER_FILENAME:
            if(ctx->filename) OPENSSL_free(ctx->filename);
            if((ctx->filename = (ptr ? OPENSSL_strdup(ptr) : NULL))) ret = 1;
            break;
         default:
            ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
            break;
      }
   } while(0);

   return ret;
}

static int debmelterf_new(BIO *b)
{
   int ret = 0;
   BIO_F_DEBMELTER_CTX *ctx;

   do {
      if(!b) break;

      if(!(ctx = (BIO_F_DEBMELTER_CTX *)OPENSSL_malloc(sizeof(BIO_F_DEBMELTER_CTX)))) break;
      if(!(ctx->buf = BUF_MEM_new())) break;
      ctx->ptr = (BUF_MEM *)ctx->buf->data;
      ctx->filename = NULL;
      ctx->step = BIO_F_DEBMELTER_STEP_INIT;
      ctx->size = 0;

      b->ptr = (char *)ctx;
      b->flags = 0;
      b->init = 1;

      ret = 1;
   } while(0);
   if(!ret) {
      if(ctx->buf) BUF_MEM_free(ctx->buf);
      if(ctx) OPENSSL_free(ctx);
   }

   return ret;
}

static int debmelterf_free(BIO *b)
{
   int ret = 0;
   BIO_F_DEBMELTER_CTX *ctx;

   do {
      if(!b || !b->init || !b->ptr) break;

      ctx = (BIO_F_DEBMELTER_CTX *)b->ptr;
      if(ctx->filename) OPENSSL_free(ctx->filename);
      if(ctx->buf) BUF_MEM_free((BUF_MEM *)ctx->buf);
      OPENSSL_free(ctx);

      b->ptr = NULL;
      b->flags = 0;
      b->init = 0;

      ret = 1;
   } while(0);

   return ret;
}

static long debmelterf_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp)
{
   long ret = 0;

   do {
      if(!b || !b->next_bio) break;
      ret = BIO_callback_ctrl(b->next_bio, cmd, fp);
   } while(0);

   return(ret);
}

static int step_init(BIO *b, BIO_F_DEBMELTER_CTX *ctx)
{
   int ret = 0;

   do {
      if(memcmp(ctx->buf->data, AR_MAGIC, strlen(AR_MAGIC))) {
         ctx->step = BIO_F_DEBMELTER_STEP_PASSTHRU;
         break;
      }

      if((ret = BIO_write(b->next_bio, AR_MAGIC, strlen(AR_MAGIC))) != strlen(AR_MAGIC)) {
         ret = -1;
         break;
      }

      ctx->buf->length -= strlen(AR_MAGIC);
      memmove(ctx->buf->data, ctx->buf->data + strlen(AR_MAGIC), ctx->buf->length);
      ctx->step = BIO_F_DEBMELTER_STEP_ENTRY_HEADER;
   } while(0);
   if(ret > 0) {
      BIO_clear_retry_flags(b);
      BIO_copy_next_retry(b);
   }

   return ret;
}

static int step_entry_header(BIO *b, BIO_F_DEBMELTER_CTX *ctx, ENTRY_HEADER *header)
{
   int ret = 0;
   ENTRY_HEADER *h;

   do {
      h = (ENTRY_HEADER *)ctx->buf->data;

      if(memcmp(h->magic, ENTRY_MAGIC, strlen(ENTRY_MAGIC))) {
         ctx->step = BIO_F_DEBMELTER_STEP_PASSTHRU;
         break;
      }

      memcpy(header, h, sizeof(ENTRY_HEADER));

      if(!memcmp(h->filename, CONTROL_FILENAME, strlen(CONTROL_FILENAME))) {
         ctx->step = BIO_F_DEBMELTER_STEP_ENTRY_MELT
         break;
      }

      ctx->step = BIO_F_DEBMELTER_STEP_ENTRY_DATA;
      if((ret = BIO_write(b->next_bio, ctx->buf->data, strlen(CONTROL_FILENAME)) != strlen(CONTROL_FILENAME)) {
         ret = -1;
         break;
      }

      ctx->buf->length -= strlen(CONTROL_FILENAME);
      memmove(ctx->buf->data, ctx->buf->data + strlen(CONTROL_FILENAME), ctx->buf->length);
   } while(0);
   if(ret > 0) {
      BIO_clear_retry_flags(b);
      BIO_copy_next_retry(b);
   ]

   return ret;
}

static int step_passthru(BIO *b, BIO_F_DEBMELTER_CTX *ctx)
{
   int ret = 0;

   do {
      if((ret = BIO_write(b->next_bio, ctx->buf->data, ctx->buf->length)) == -1) break;
      ctx->buf->length -= ret;
      memmove(ctx->buf->data, ctx->buf->data + ret, ctx->buf->length);
   } while(0);
   BIO_clear_retry_flags(b);
   BIO_copy_next_retry(b);

   return ret;
}
