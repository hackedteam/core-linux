#define _GNU_SOURCE
#include <stdio.h>
#include <jpeglib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "imgutils.h"
#include "runtime.h"
#include "config.h"

long getscreenimage(Display *display, Window window, int x, int y, int width, int height, int quality, char **outbuf)
{
   XWindowAttributes wininfo;
   XImage *image = NULL;
   unsigned char *buf = NULL;
   long outlen = 0;
   unsigned int pixel = 0;

   do {
      if(!XGetWindowAttributes(display, window, &wininfo)) break;
      if(x == -1) x = wininfo.x;
      if(y == -1) y = wininfo.y;
      if(width == -1) width = wininfo.width;
      if(height == -1) height = wininfo.height;
      if(!(image = XGetImage(display, window, x, y, width, height, AllPlanes, ZPixmap))) break;
      if(image->depth != 24) break;

      if(!(buf = calloc(image->width * 3, image->height))) break;
      for(y = 0; y < image->height; y++) {
         for(x = 0; x < image->width; x++) {
            pixel = XGetPixel(image, x, y);
            buf[y * image->width * 3 + x * 3 + 0] = (unsigned char)((pixel>>16) & 0xff);
            buf[y * image->width * 3 + x * 3 + 1] = (unsigned char)((pixel>>8) & 0xff);
            buf[y * image->width * 3 + x * 3 + 2] = (unsigned char)(pixel & 0xff);
         }
      }

      width = image->width;
      height = image->height;
      outlen = encodeimage(buf, width, height, quality, outbuf);
   } while(0);

   if(buf) free(buf);
   if(image) XDestroyImage(image);

   return outlen;
}

long encodeimage(unsigned char *inbuf, int width, int height, int quality, char **outbuf)
{
   long outlen = 0;

   do {
      if(initlib(INIT_LIBJPEG)) break;

      if(jpeg_libver == 80) {
         outlen = encodeimage80(inbuf, width, height, quality, outbuf);
      } else {
         outlen = encodeimage62(inbuf, width, height, quality, outbuf);
      }
   } while(0);

   return outlen;
}
