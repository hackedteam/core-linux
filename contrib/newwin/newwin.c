#include <stdio.h>
#include <X11/Xlib.h>

int main(void)
{
   Display *display = NULL;
   XEvent e;

   if(!(display = XOpenDisplay(NULL))) return -1;
   if(XSelectInput(display, DefaultRootWindow(display), SubstructureNotifyMask) == BadWindow) return -1;

   while(1) {
      XNextEvent(display, &e);
      if((e.xany.type == CreateNotify) && !e.xcreatewindow.override_redirect) {
         printf("New window %d\n", (int)e.xcreatewindow.window);
      }
   }

   return 0;
}
