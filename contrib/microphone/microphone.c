#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pulse/simple.h>
#include <speex/speex.h>

int main(int argc, char *argv[])
{
   pa_simple *pa = NULL;
   pa_sample_spec paspec = { PA_SAMPLE_S16NE, 32000, 1 };
   void *state = NULL;
   SpeexBits bits;
   int quality = 3;
   int complexity = 1;
   int framesize = 0;
   int16_t *framein = NULL;
   char *frameout = NULL;
   int len;

   do {
      if(!(pa = pa_simple_new(NULL, "", PA_STREAM_RECORD, NULL, "", &paspec, NULL, NULL, NULL))) break;
      speex_bits_init(&bits);
      if(!(state = speex_encoder_init(&speex_uwb_mode))) break;
      if(speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality)) break;
      if(speex_encoder_ctl(state, SPEEX_SET_COMPLEXITY, &complexity)) break;
      if(speex_encoder_ctl(state, SPEEX_GET_FRAME_SIZE, &framesize) || !framesize) break;
      if(!(framein = calloc(framesize, sizeof(int16_t)))) break;
      if(!(frameout = calloc(framesize, sizeof(int16_t)))) break;

      while(1) {
         if(pa_simple_read(pa, framein, framesize * sizeof(int16_t), NULL)) break;
         speex_bits_reset(&bits);
         speex_encode_int(state, framein, &bits);
         if(!(len = speex_bits_write(&bits, frameout, framesize * sizeof(int16_t)))) break;
         fwrite(&len, sizeof(int), 1, stdout);
         fwrite(frameout, 1, len, stdout);
      }
   } while(0);
   if(framein) free(framein);
   if(frameout) free(frameout);
   if(state) speex_encoder_destroy(state);
   speex_bits_destroy(&bits);
   if(pa) pa_simple_free(pa);

   return 0;
}
