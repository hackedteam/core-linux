#ifndef _BIOUTILS_H
#define _BIOUTILS_H

#define _GNU_SOURCE
#include <time.h>
#include <openssl/bio.h>

#define PUTS16P 0x01
#define PUTS16N 0x02

int strlen16(const char *v);
int BIO_puti32(BIO *b, unsigned int v);
int BIO_putfiletime(BIO *b, time_t v);
int BIO_puts16full(BIO *b, const char *v, int flags);

#define BIO_puts16(b, v) BIO_puts16full(b, v, 0)
#define BIO_puts16p(b, v) BIO_puts16full(b, v, PUTS16P)
#define BIO_puts16n(b, v) BIO_puts16full(b, v, PUTS16N)
#define BIO_puts16pn(b, v) BIO_puts16full(b, v, PUTS16P|PUTS16N)
#define BIO_putsep(b) BIO_write(bio_data, "\xDE\xC0\xAD\xAB", 4)
#define BIO_putn(b) BIO_write(bio_data, "\0\0", 2)

#endif /* _BIOUTILS_H */
