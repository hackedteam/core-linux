#include "bioutils.h"
#include "runtime.h"

int strlen16(const char *v)
{
   int ret = 0;

   do {
      if(!v) break;

      while(v[0]) {
         if((v[0] & 0xC0) == 0x80) {
            break;
         } else if((v[0] & 0x80) == 0x00) {
            v++;
         } else if(((v[0] & 0xE0) == 0xC0) && ((v[1] & 0xC0) == 0x80)) {
            v += 2;
         } else if(((v[0] & 0xF0) == 0xE0) && ((v[1] & 0xC0) == 0x80) && ((v[2] & 0xC0) == 0x80)) {
            v += 3;
         } else {
            break;
         }
         ret += 2;
      }
   } while(0);
   
   return ((!v || v[0]) ? -1 : ret);
}

int BIO_puti32(BIO *b, unsigned int v)
{
   return BIO_write(b, &v, 4);
}

int BIO_putfiletime(BIO *b, time_t v)
{
   BIO *bio_data = NULL;
   char *dataptr;
   long datalen;
   struct tm t = {};
   int ret = -1;

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      if(!gmtime_r(&v, &t)) break;
      t.tm_mon++;
      t.tm_year += 1900;
      if(BIO_write(bio_data, &t.tm_sec, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_min, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_hour, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_mday, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_mon, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_year, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_wday, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_yday, 4) != 4) break;
      if(BIO_write(bio_data, &t.tm_isdst, 4) != 4) break;

      if((datalen = BIO_get_mem_data(bio_data, &dataptr)) != 36) break;
      ret = BIO_write(b, dataptr, datalen);
   } while(0);
   if(bio_data) BIO_free(bio_data);

   return ret;
}

int BIO_puts16full(BIO *b, const char *v, int flags)
{
   BIO *bio_data = NULL;
   char *dataptr;
   long datalen;
   int len, ret = -1;
   char c[2];

   do {
      if(!v) break;
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if((len = strlen16(v)) == -1) break;

      if(flags & PUTS16P) if(BIO_puti32(bio_data, len) != 4) break;

      while(v[0]) {
         if((v[0] & 0xC0) == 0x80) {
            break;
         } else if((v[0] & 0x80) == 0x00) {
            c[1] = '\0';
            c[0] = v[0];
            v++;
         } else if(((v[0] & 0xE0) == 0xC0) && ((v[1] & 0xC0) == 0x80)) {
            c[1] = (v[0]>>2) & 0x07;
            c[0] = ((v[0] & 0x03)<<6) | (v[1] & 0x3F);
            v += 2;
         } else if(((v[0] & 0xF0) == 0xE0) && ((v[1] & 0xC0) == 0x80) && ((v[2] & 0xC0) == 0x80)) {
            c[1] = ((v[0] & 0x0F)<<4) | ((v[1]>>2) & 0x0F);
            c[0] = ((v[1] & 0x03)<<6) | (v[2] & 0x3F);
            v += 3;
         } else {
            break;
         }
         if(BIO_write(bio_data, c, 2) != 2) break;
      }
      if(v[0]) break;

      if(flags & PUTS16N) if(BIO_write(bio_data, "\0\0", 2) != 2) break;

      if((datalen = BIO_get_mem_data(bio_data, &dataptr)) != (((flags & PUTS16P) ? 4 : 0) + len + ((flags & PUTS16N) ? 2 : 0))) break;
      ret = (datalen ? BIO_write(b, dataptr, datalen) : 0);
   } while(0);
   if(bio_data) BIO_free(bio_data);

   return ret;
}
