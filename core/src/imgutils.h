#ifndef _IMGUTILS_H
#define _IMGUTILS_H

#include <X11/Xlib.h>

long getscreenimage(Display *display, Window window, int x, int y, int width, int height, int quality, char **outbuf);
long encodeimage(unsigned char *inbuf, int width, int height, int quality, char **outbuf);
long encodeimage80(unsigned char *inbuf, int width, int height, int quality, char **outbuf);
long encodeimage62(unsigned char *inbuf, int width, int height, int quality, char **outbuf);

#endif /*_IMGUTILS_H */
