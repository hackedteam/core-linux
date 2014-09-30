#define _GNU_SOURCE
#define C_ARITH_CODING_SUPPORTED 1
#define D_ARITH_CODING_SUPPORTED 1
#define HAVE_PROTOTYPES 1
#define HAVE_UNSIGNED_CHAR 1
#define HAVE_UNSIGNED_SHORT 1
#define INLINE __attribute__((always_inline))
#define NEED_SYS_TYPES_H 1
#define WITH_SIMD 1
#ifndef JPEG_LIB_VERSION
   #error "JPEG_LIB_VERSION undefined"
#endif
#define JCONFIG_INCLUDED

#include <stdio.h>
#include <jpeglib.h>

#include "imgutils.h"
#include "runtime.h"

#if JPEG_LIB_VERSION >= 80
long encodeimage80(unsigned char *inbuf, int width, int height, int quality, char **outbuf)
#else
long encodeimage62(unsigned char *inbuf, int width, int height, int quality, char **outbuf)
#endif
{
   FILE *mp = NULL;
   struct jpeg_error_mgr jerr = {};
   struct jpeg_compress_struct cinfo = {};
   long outlen, ret = 0;
   JSAMPROW row;

   do {
      if(!(mp = open_memstream(outbuf, (size_t *)&outlen))) break;

      cinfo.err = jpeg_std_error(&jerr);
      jpeg_create_compress(&cinfo);
      jpeg_stdio_dest(&cinfo, mp);

      cinfo.image_width = width;
      cinfo.image_height = height;
      cinfo.input_components = 3;
      cinfo.in_color_space = JCS_RGB;

      jpeg_set_defaults(&cinfo);
      jpeg_set_quality(&cinfo, quality, TRUE);
      jpeg_start_compress(&cinfo, TRUE);

      while(cinfo.next_scanline < cinfo.image_height) {
         row = (JSAMPROW)&inbuf[cinfo.next_scanline * 3 * width];
         jpeg_write_scanlines(&cinfo, &row, 1);
      }
      jpeg_finish_compress(&cinfo);
      fflush(mp);
      ret = outlen;
   } while(0);
   jpeg_destroy_compress(&cinfo);
   if(mp) fclose(mp);

   return ret;
}
