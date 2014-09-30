#ifndef _XUTILS_H
#define _XUTILS_H

#include <X11/Xlib.h>

Window getactivewindow(Display *display);
Window getfocuswindow(Display *display);
void getwindowproperties(Display *display, Window window, char **process, char **title);

#endif /* _XUTILS_H */
