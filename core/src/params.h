#ifndef _PARAMS_H
#define _PARAMS_H 1

#include <stdint.h>

struct params {
   uint32_t version;
   char build[16];
   char subtype[16];
   unsigned char evidencekey[32];
   unsigned char confkey[32];
   unsigned char signature[32];
   unsigned char watermark[32];
   unsigned char demo[24];
};

extern struct params bps;

#endif /* _PARAMS_H */
