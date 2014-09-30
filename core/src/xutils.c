#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "xutils.h"
#include "so.h"
#include "runtime.h"

Window getactivewindow(Display *display)
{
   Atom name, type, rettype;
   int format;
   unsigned long nitems, after;
   unsigned char *data = NULL;
   Window ret = 0;

   do {
      name = XInternAtom(display, SO"_NET_ACTIVE_WINDOW", True);
      if((name == BadAlloc) || (name == BadValue)) break;
      type = XInternAtom(display, SO"WINDOW", True);
      if((type == BadAlloc) || (type == BadValue)) break;

      XGetWindowProperty(display, DefaultRootWindow(display), name, 0, sizeof(Window), False, type, &rettype, &format, &nitems, &after, &data);
      if(data) {
         ret = *(Window *)data;
         XFree(data);
      }
   } while(0);

   return (ret ? ret : DefaultRootWindow(display));
}

Window getfocuswindow(Display *display)
{
   Window ret = 0;
   int revert;

   if(display) XGetInputFocus(display, &ret, &revert);

   return (ret ? ret : DefaultRootWindow(display));
}

void getwindowproperties(Display *display, Window window, char **process, char **title)
{
   Atom pid_atom, name1_atom, name2_atom, class_atom;
   Atom type;
   int format;
   unsigned long nitems, after;
   unsigned char *data = NULL;
   pid_t pid = 0;
   char *name1 = NULL, *name2 = NULL, *class = NULL;
   Window parent = 0, root = 0;
   Window *children = NULL;
   unsigned int nchildren;

   do {
      pid_atom = XInternAtom(display, SO"_NET_WM_PID", True);
      if((pid_atom == BadAlloc) || (pid_atom == BadValue)) break;
      name1_atom = XInternAtom(display, SO"_NET_WM_NAME", True);
      if((name1_atom == BadAlloc) || (name1_atom == BadValue)) break;
      name2_atom = XInternAtom(display, SO"WM_NAME", True);
      if((name2_atom == BadAlloc) || (name2_atom == BadValue)) break;
      class_atom = XInternAtom(display, SO"WM_CLASS", True);
      if((class_atom == BadAlloc) || (class_atom == BadValue)) break;

      while(window && (window != root)) {
         if(!pid) {
            XGetWindowProperty(display, window, pid_atom, 0, 1, False, AnyPropertyType, &type, &format, &nitems, &after, &data);
            if(data) {
               pid = *(pid_t *)data;
               XFree(data);
            }
         }

         if(!name1) {
            XGetWindowProperty(display, window, name1_atom, 0, 256, False, AnyPropertyType, &type, &format, &nitems, &after, &data);
            if(data) {
               name1 = strdup((char *)data);
               XFree(data);
            }
         }

         if(!name2 && !name1) {
            XGetWindowProperty(display, window, name2_atom, 0, 256, False, AnyPropertyType, &type, &format, &nitems, &after, &data);
            if(data) {
               name2 = strdup((char *)data);
               XFree(data);
            }
         }

         if(!class) {
            XGetWindowProperty(display, window, class_atom, 0, 16, False, AnyPropertyType, &type, &format, &nitems, &after, &data);
            if(data) {
               class = strdup((char *)((unsigned long)strchr((char *)data, '\0') + 1));
               XFree(data);
            }
         }

         if(pid && name1 && class) break;

         if(!XQueryTree(display, window, &root, &parent, &children, &nchildren)) break;
         if(children) {
            XFree(children);
            children = NULL;
         }

         window = parent;
      }

      asprintf(process, "%s (%u)", class ? class : SO"Unknown", pid ? pid : 0);
      *title = strdup(name1 ? name1 : (name2 ? name2 : ""));
   } while(0); 

   if(name1) free(name1);
   if(name2) free(name2);
   if(class) free(class);

   return;
}
